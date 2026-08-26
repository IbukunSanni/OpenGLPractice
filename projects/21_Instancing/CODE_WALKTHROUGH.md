# 19_Cubemaps_n_Skyboxes — Full Walkthrough

A study document. It assumes you have forgotten the details and rebuilds them from the ground up, then shows how this specific project uses them.

**The organizing principle for everything below:**

> The GPU knows nothing and infers nothing. Every byte's meaning, every connection between one object and another, every value a shader reads — all of it has to be stated explicitly, in the right order, before the draw call. Nearly every OpenGL call in this project exists because *something* has to be told to a machine that will otherwise happily do the wrong thing without complaint.

That last part matters most: **OpenGL almost never errors on a mistake.** Tell it a 4-byte-per-pixel image is 3-byte-per-pixel and it doesn't stop — it produces a sheared image. Point an attribute at the wrong offset and it doesn't stop — it reads a neighbouring field as if it were texture coordinates. This is why the debugging sections below matter as much as the concept sections.

---

## Table of contents

- [Part 0 — Two mental models to hold](#part-0--two-mental-models-to-hold)
- [Part 1 — Where data lives](#part-1--where-data-lives)
- [Part 2 — The vocabulary, rebuilt from scratch](#part-2--the-vocabulary-rebuilt-from-scratch)
- [Part 3 — Reading order for the files](#part-3--reading-order-for-the-files)
- [Part 4 — The pipeline: one vertex to one pixel](#part-4--the-pipeline-one-vertex-to-one-pixel)
- [Part 5 — What is new in this project: cubemaps and skyboxes](#part-5--what-is-new-in-this-project-cubemaps-and-skyboxes)
- [Part 6 — One complete frame](#part-6--one-complete-frame)
- [Part 7 — The bugs you hit, as case studies](#part-7--the-bugs-you-hit-as-case-studies)
- [Part 8 — Debugging checklist](#part-8--debugging-checklist)
- [Part 9 — Self-test](#part-9--self-test)
- [Part 10 — Experiments](#part-10--experiments)

---

## Part 0 — Two mental models to hold

### Model 1: OpenGL is a state machine, not a function library

You almost never say "draw this thing with these settings" in one call. Instead you set a long list of global state, then issue a draw that consumes whatever state happens to be current:

```cpp
glUseProgram(...)        // "the current shader program is now X"
glBindVertexArray(...)   // "the current vertex layout is now Y"
glBindTexture(...)       // "the current texture on unit 0 is now Z"
glDrawElements(...)      // "draw, using whatever is current"
```

The consequence you must internalize: **a draw call's behaviour depends on calls that may be far away in the file, or even in another function entirely.** When something renders wrong, the question is rarely "is this line correct?" and almost always "what was actually bound when this line ran?"

There is a corollary that bites often: **state persists across frames.** If you change `glDepthFunc` in frame 1 and don't change it back, frame 2 starts with your modified value. This project deliberately restores state for exactly this reason (see `glDepthFunc(GL_LESS)` at the end of the loop).

### Model 2: Bind ≠ copy

`glBindBuffer(GL_ARRAY_BUFFER, ID)` does **not** move any data. It sets a pointer meaning "when someone next talks about `GL_ARRAY_BUFFER`, they mean object `ID`." The actual data movement happens in `glBufferData` / `glTexImage2D`, and those act on *whatever is currently bound*.

This is why order matters so much, and why "I called `glBufferData` but nothing appeared" is usually "the wrong thing was bound at that moment."

---

## Part 1 — Where data lives

There are two separate memories in this program, and they do not automatically stay in sync.

```text
CPU RAM (normal C++ memory)          GPU memory (OpenGL objects)
──────────────────────────────       ──────────────────────────────
std::vector<Vertex>            ──►   VBO   (vertex buffer object)
std::vector<GLuint> indices    ──►   EBO   (element buffer object)
stb_image pixel bytes          ──►   texture object (2D or cubemap)
glm::mat4 / vec3 values        ──►   uniforms inside a shader program
skyboxVertices[] (raw array)   ──►   VBO
```

Each `──►` is an explicit upload call. **Changing the CPU-side value after the upload does nothing to the GPU copy.** If you edit `skyboxVertices[]` after `glBufferData`, the GPU still holds the old bytes until you upload again.

The one exception is uniforms, which are cheap and small, so this project re-uploads them every frame (camera matrix, model transforms) rather than trying to track changes.

**Why the split exists at all:** the GPU is physically a separate processor, often with its own dedicated memory. Sending data across that boundary is comparatively slow. So the design is: upload once during setup (`GL_STATIC_DRAW` is literally a hint meaning "I'll upload this rarely and draw it often"), then each frame send only the few small values that changed.

---

## Part 2 — The vocabulary, rebuilt from scratch

For each item: *what it is*, *what problem it solves*, *what happens without it*.

### 2.1 `Vertex` — the memory contract

```cpp
struct Vertex           // VBO.h
{
    glm::vec3 position; // 3 floats — where this corner is
    glm::vec3 normal;   // 3 floats — which way the surface faces here (for lighting)
    glm::vec3 color;    // 3 floats — a per-vertex tint
    glm::vec2 texUV;    // 2 floats — where to look in the texture image
};                      // total: 11 floats = 44 bytes
```

In memory, vertices sit back-to-back with their fields **interleaved**:

```text
byte 0        12       24       36    44       56  ...
     ├────────┼────────┼────────┼─────┼────────┼────
     │position│ normal │ color  │ UV  │position│ ...
     └────────┴────────┴────────┴─────┴────────┴────
     └───────── Vertex 0 (44 bytes) ──┘└─ Vertex 1 …
```

**The problem it solves:** the GPU receives one undifferentiated blob of bytes. Something has to define what a "vertex" is so both sides agree.

**Without it:** you'd have no shared definition, and every buffer would need its layout described from nothing.

### 2.2 VBO — Vertex Buffer Object

```cpp
glGenBuffers(1, &ID);                      // 1. get a handle (an integer name)
glBindBuffer(GL_ARRAY_BUFFER, ID);         // 2. make it current
glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);  // 3. allocate + copy
```

**What it is:** a block of GPU memory holding raw vertex bytes. Nothing more. It has *no idea* those bytes mean position/normal/color/UV — it's a byte array.

**What problem it solves:** getting vertex data into GPU-accessible memory.

**Without it:** the GPU cannot read your `std::vector`; that memory isn't reachable from the GPU.

The three-step pattern (`Gen` → `Bind` → `upload`) repeats for nearly every OpenGL object type. Recognizing it makes unfamiliar GL code much easier to read.

### 2.3 EBO — Element Buffer Object (a.k.a. index buffer)

```cpp
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...);
```

**What it is:** a list of integers naming which vertices form each triangle, three at a time.

**What problem it solves:** vertex reuse. A cube has 8 corners but 12 triangles (36 triangle-corners). Without indices you'd store 36 full vertices — the same corner repeated 4–5 times. With indices you store 8 vertices plus 36 small integers.

Look at this project's skybox data:

```cpp
float skyboxVertices[] = { /* 8 corners, 3 floats each */ };
unsigned int skyboxIndices[] = {
    1, 2, 6,  6, 5, 1,   // Right face = 2 triangles
    0, 4, 7,  7, 3, 0,   // Left
    ...                   // 36 indices total
};
```

Eight positions, thirty-six indices, twelve triangles, six faces. Every index is one of 0–7.

**⚠ The gotcha that will bite you:** the `GL_ELEMENT_ARRAY_BUFFER` binding is **stored inside the currently-bound VAO**. This is why `Mesh.cpp` unbinds in this exact order:

```cpp
VAO.Unbind();   // FIRST — VAO stops recording
vbo.Unbind();
ebo.Unbind();   // safe now
```

Unbind the EBO while the VAO is still bound and the VAO records "my index buffer is: nothing." `glDrawElements` then has no indices and draws nothing — silently.

### 2.4 VAO — Vertex Array Object

This is the one people forget, so here it is plainly:

**What it is:** a saved *recipe* for how to read vertex data. It stores no vertex bytes itself.

For each enabled attribute it records:
- the shader location number (0, 1, 2, 3…)
- how many components and of what type (3 × `GL_FLOAT`)
- the stride — bytes from one vertex to the next
- the offset — bytes from the vertex start to this field
- **which VBO was bound at the moment `glVertexAttribPointer` was called**

Plus the EBO binding.

**What problem it solves:** without it, you'd have to re-issue every `glVertexAttribPointer` call before every single draw. The VAO lets you say it once at setup, then restore all of it with one `glBindVertexArray`.

Here is the recipe this project records for a model mesh (`Mesh.cpp`):

| `glVertexAttribPointer` args | Vertex field | Components | Offset |
|---|---|---:|---:|
| location 0 | `position` | 3 floats | `0` |
| location 1 | `normal` | 3 floats | `3 * sizeof(float)` |
| location 2 | `color` | 3 floats | `6 * sizeof(float)` |
| location 3 | `texUV` | 2 floats | `9 * sizeof(float)` |

Those location numbers are not arbitrary — they must match `default.vert`:

```glsl
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTex;
```

**This table is the single most important correspondence in the whole project.** The C++ side and the GLSL side are two halves of one contract, written in different files and different languages, with nothing checking that they agree.

Compare the skybox's much simpler recipe (`Main.cpp`):

```cpp
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
```

One attribute. Stride is `3 * sizeof(float)` because skybox vertices are *only* positions — no normals, no colors, no UVs. Two different VAOs, two different recipes, and the draw call picks which by whichever is bound.

The shorthand worth memorizing:

```text
VBO = the bytes
EBO = which vertices form which triangles
VAO = how to interpret the bytes + which EBO to use
```

### 2.5 Shader, shader program

A **shader** is a small program that runs on the GPU. Two kinds matter here:

- **Vertex shader** — runs once per vertex. Job: compute `gl_Position` (where this vertex lands on screen) and pass data to the next stage.
- **Fragment shader** — runs once per *potential pixel*. Job: compute `FragColor`.

A **shader program** is a vertex shader and fragment shader compiled and linked together. `shaderClass.cpp` does this:

```cpp
glCreateShader(GL_VERTEX_SHADER) → glShaderSource → glCompileShader
glCreateShader(GL_FRAGMENT_SHADER) → glShaderSource → glCompileShader
glCreateProgram() → glAttachShader ×2 → glLinkProgram
```

**Crucially, this is compiled at runtime, not build time.** Your `.vert`/`.frag` files are just text that MSBuild copies around; the GPU driver compiles them when the program starts. A syntax error in a shader will **not** fail your build — it prints to the console via `compileErrors()` and then that shader silently does nothing.

> This is exactly the bug you hit earlier with `gl_Position = vec4();` — an invalid zero-argument constructor. The C++ build succeeded, the program ran, and the skybox simply didn't draw. **If something isn't appearing, read the console first.**

This project has two programs:

| Program | Files | Draws |
|---|---|---|
| `shaderProgram` | `default.vert` / `default.frag` | the lit model |
| `skyboxShader` | `skybox.vert` / `skybox.frag` | the skybox cube |

### 2.6 Uniforms

**What it is:** a variable inside a shader program, set from C++, constant for the entire draw call.

```cpp
glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);
```

Attributes vary per vertex; uniforms are the same for every vertex and fragment in one draw. Camera matrix, light colour, model transform — all uniforms.

**Two things that catch people:**

1. `glGetUniformLocation` looks up a variable *by name string*. Misspell it and you get `-1`, and `glUniform*` on location `-1` is **silently ignored**. No error. The value just never arrives.
2. Uniforms belong to a specific program. You must `Activate()` (i.e. `glUseProgram`) the right program *before* setting its uniforms. Notice `Main.cpp` doing exactly this:

```cpp
shaderProgram.Activate();
glUniform4f(... "lightColor" ...);      // goes into shaderProgram
skyboxShader.Activate();
glUniform1i(... "skybox" ..., 0);       // goes into skyboxShader
```

Uniform values persist in the program until overwritten, which is why `lightColor` is set once before the loop while camera matrices are re-sent every frame.

### 2.7 Texture object vs texture unit vs sampler uniform

Three different things that are easy to conflate:

1. **Texture object** — owns the uploaded pixels and their sampling settings. Identified by a `GLuint` handle.
2. **Texture unit** — a numbered slot (0, 1, 2…) that a texture gets bound *into*. Think of it as a socket.
3. **Sampler uniform** — a variable in the shader holding the *integer index of the unit* to read from.

The three-step connection:

```cpp
glActiveTexture(GL_TEXTURE0 + unit);       // select which socket
glBindTexture(GL_TEXTURE_2D, ID);          // plug this texture into it
glUniform1i(samplerLocation, unit);        // tell the shader which socket to read
```

**Note the type mismatch that trips everyone:** the sampler receives a plain `0`, not `GL_TEXTURE0`. `GL_TEXTURE0` is a large enum constant; the sampler wants the index.

In this project the skybox uses unit 0:

```cpp
glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);  // setup
...
glActiveTexture(GL_TEXTURE0);                                      // each frame
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
```

`Mesh::Draw` does the same thing dynamically for model textures, building sampler names like `diffuse0` and `specular0` by concatenating the texture's type with a counter — those generated strings must match the `uniform sampler2D diffuse0;` declarations in `default.frag` exactly.

---

## Part 3 — Reading order for the files

Two passes. First pass = the story. Second pass = the machinery.

### First pass — what the program does

| # | File | What to look for |
|---|---|---|
| 1 | `Main.cpp` | Setup order, then the render loop. Don't stop to understand the classes yet — just note *the sequence*. |
| 2 | `VBO.h` | The `Vertex` struct. This is the contract everything else references. |
| 3 | `skybox.vert` / `skybox.frag` | Small and self-contained. The two tricks (`xyww`, position-as-direction) live here. |
| 4 | `default.vert` | Match `layout (location = N)` against the table in §2.4. |
| 5 | `default.frag` | Follow interpolated values + textures into a final colour. |

### Second pass — how it works

| # | File | What to look for |
|---|---|---|
| 1 | `VBO.cpp` | The Gen→Bind→Upload pattern, in 4 lines. |
| 2 | `EBO.cpp` | Same pattern, different target. |
| 3 | `VAO.cpp` | `LinkAttrib` — note it binds the VBO, describes the attribute, then unbinds. The description survives. |
| 4 | `Mesh.cpp` constructor | How VBO+EBO+VAO get wired into one drawable unit. Note the unbind order. |
| 5 | `Mesh::Draw` | What gets re-bound and re-uploaded per frame. |
| 6 | `shaderClass.cpp` | Runtime compilation + `compileErrors`. |
| 7 | `Texture.cpp` | Image load → channel-format decision → upload. |
| 8 | `Camera.h/.cpp` | Input → `Position`/`Orientation` → `cameraMatrix`. |
| 9 | `Model.cpp` | glTF parsing. Large; skim the structure, don't memorize it. |
| 10 | `Main.cpp` again | It should feel much smaller now. |

That last step is the real test. If `Main.cpp` reads as a short list of complete ideas rather than a wall of `gl*` calls, the model has landed.

---

## Part 4 — The pipeline: one vertex to one pixel

```text
   Vertex struct in a std::vector           [CPU]
              │  glBufferData
              ▼
        VBO — raw bytes on GPU
              │  VAO recipe says: "location 1 = 3 floats at offset 12, stride 44"
              ▼
     ┌── VERTEX SHADER (default.vert) ──┐    runs once per vertex
     │  in:  aPos, aNormal, aColor, aTex│
     │  uniforms: camMatrix, model, …   │
     │  out: gl_Position (clip space)   │
     │       crntPos, Normal, texCoord  │
     └──────────────┬───────────────────┘
                    │  perspective divide: gl_Position.xyz / gl_Position.w
                    ▼
              Normalized Device Coordinates (−1..+1 cube)
                    │  viewport transform (glViewport)
                    ▼
              RASTERIZER — which pixels does this triangle cover?
                    │  interpolates crntPos/Normal/texCoord across the surface
                    ▼
     ┌── FRAGMENT SHADER (default.frag) ─┐   runs once per covered pixel
     │  in: interpolated values          │
     │  samplers: diffuse0, specular0    │
     │  out: FragColor                   │
     └──────────────┬────────────────────┘
                    │
                    ▼
              DEPTH TEST — is this closer than what's already there?
                    │  (glDepthFunc decides what "passes")
                    ▼
              FRAMEBUFFER ──glfwSwapBuffers──► screen
```

Two things worth pinning down because they cause confusion later:

**The perspective divide is automatic.** You output `gl_Position` as a `vec4`; the hardware divides `xyz` by `w` for you. This is the mechanism the skybox trick exploits — see §5.3.

**Interpolation is automatic too.** The vertex shader runs 3 times for a triangle, but the fragment shader might run 50,000 times for it. Every `out` from the vertex shader becomes a smoothly blended `in` at each pixel. That's why `Normal` needs `normalize()` again in the fragment shader — blending two unit vectors doesn't give a unit vector.

---

## Part 5 — What is new in this project: cubemaps and skyboxes

Everything up to here was equally true in lessons 12–18. This is the new material.

### 5.1 A cubemap is one texture object with six faces

```cpp
glGenTextures(1, &cubemapTexture);              // ONE handle
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
```

The six faces are uploaded to six *different targets*, which are consecutive enum values:

| Target | = POSITIVE_X + | Face | Direction |
|---|---:|---|---|
| `GL_TEXTURE_CUBE_MAP_POSITIVE_X` | 0 | right | +X |
| `GL_TEXTURE_CUBE_MAP_NEGATIVE_X` | 1 | left | −X |
| `GL_TEXTURE_CUBE_MAP_POSITIVE_Y` | 2 | top | +Y |
| `GL_TEXTURE_CUBE_MAP_NEGATIVE_Y` | 3 | bottom | −Y |
| `GL_TEXTURE_CUBE_MAP_POSITIVE_Z` | 4 | front | +Z |
| `GL_TEXTURE_CUBE_MAP_NEGATIVE_Z` | 5 | back | −Z |

That consecutiveness is why the upload loop can do arithmetic on an enum:

```cpp
for (unsigned int i = 0; i < facesCubemap.size(); i++)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, ...);
```

**This only works because `facesCubemap` is declared in exactly that order.** Nothing verifies it. Swap two entries and you get a perfectly valid cube with the wrong pictures on the wrong walls — the "stupid computer" principle in miniature.

Also note the wrap modes:

```cpp
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
```

Three axes (S, T, **R** — cubemaps are 3D-addressed). `GL_CLAMP_TO_EDGE` stops the sampler from wrapping around and pulling colour from the opposite edge when it samples right at a face boundary, which would show up as visible seams along the cube's edges.

### 5.2 Cubemaps are sampled by direction, not by UV

This is the conceptual leap. A `sampler2D` takes a `vec2`. A `samplerCube` takes a **`vec3` direction**:

```glsl
uniform samplerCube skybox;
in vec3 texCoords;
FragColor = texture(skybox, texCoords);   // texCoords is a DIRECTION
```

The hardware takes that vector, finds which component has the largest magnitude (that picks the face), and uses the other two to index into it. You never compute this yourself.

Which explains why the skybox VAO has no UV attribute at all. `skybox.vert` derives the direction from the position:

```glsl
texCoords = vec3(aPos.x, aPos.y, -aPos.z);
```

**Why this works:** the cube is centred on the origin with corners at ±1. For a shape centred on the origin, *the vector from the centre to a point is the same as the direction to that point*. Position **is** direction here. No separate data needed.

The `-aPos.z` negation is a coordinate-convention fix: OpenGL's world space is right-handed (−Z forward) while cubemap face addressing was inherited from a left-handed convention. Without it, front and back swap.

### 5.3 Making the skybox render "behind everything, forever away"

Two independent tricks. Keep them separate in your head — they solve different problems.

#### Trick A — pin depth to the far plane

```glsl
vec4 pos = projection * view * vec4(aPos, 1.0f);
gl_Position = vec4(pos.x, pos.y, pos.w, pos.w);
//                              ^^^^^ z gets w's value
```

Recall from Part 4 that the hardware divides by `w`. Setting `z = w` means the final depth is always `w / w = 1.0` — the maximum possible depth, no matter how big the cube is or where the camera stands. Meanwhile `x` and `y` are untouched, so the cube still *looks* correctly shaped and perspective-correct.

That pin is useless alone, because of the depth test's default:

```cpp
glDepthFunc(GL_LEQUAL);   // before the skybox draw
...
glDepthFunc(GL_LESS);     // restored right after
```

Default depth testing is `GL_LESS` — strictly nearer wins. A fragment at depth exactly `1.0` can never be strictly nearer than anything, *including the cleared buffer which is also 1.0*. It would be rejected everywhere. `GL_LEQUAL` (≤) lets it through **only where the depth buffer still holds its cleared 1.0** — that is, only where nothing else was drawn.

The payoff: the skybox automatically appears in exactly the gaps between real objects, and the per-pixel filtering is done by dedicated depth-test hardware that was going to run anyway. No CPU-side visibility logic.

#### Trick B — ignore camera translation

```cpp
view = glm::mat4(glm::mat3(glm::lookAt(camera.Position,
                                       camera.Position + camera.Orientation,
                                       camera.Up)));
```

A 4×4 view matrix stores rotation in its upper-left 3×3 block and translation in its last column. Truncating to `mat3` throws that column away; widening back to `mat4` refills it with identity (zero translation).

Result: **camera movement has no effect on the skybox; only camera rotation does.** Walk forward and the model gets closer while the sky stays put — which is precisely how a real distant horizon behaves.

### 5.4 Face culling has to differ per draw

Two independent switches, and the naming actively misleads:

- `glFrontFace(GL_CCW | GL_CW)` — *labels* which winding direction counts as "front."
- `glCullFace(GL_FRONT | GL_BACK)` — names which label gets **thrown away**.

For a normal closed model, you discard the faces pointing away from the camera — you'd never see them, so skipping them is free performance.

The skybox is a fundamentally different situation: **the camera is inside the cube looking at the inner surface of its faces.** A winding that reads as "front" from outside reads as the opposite from within. Whatever setting is correct for the model is therefore wrong for the skybox — so the code steps around it:

```cpp
glDisable(GL_CULL_FACE);
/* draw skybox */
glEnable(GL_CULL_FACE);
```

> **A note on trusting comments.** The tutorial's own source has `// Keeps front faces` written directly above `glCullFace(GL_FRONT)`. That comment is wrong — `glCullFace` names what is *discarded*. It has been wrong since lesson 16 and copied forward ever since. When a comment and the observed behaviour disagree, the behaviour is the source of truth.

---

## Part 6 — One complete frame

Trace this in `Main.cpp`'s render loop:

```text
 1. Update FPS/ms strings          (throttled to ~30 Hz so they're readable)
 2. glClearColor + glClear         (wipe colour AND depth to their defaults;
                                    depth clears to 1.0 — this is what makes
                                    Trick A in §5.3 work)
 3. Poll ESC; camera.Inputs()      (read keyboard/mouse)
 4. camera.UpdateMatrix(45°, 0.1, 100)   (rebuild proj × view)
 5. Update window title with live camera position
 6. model.Draw(shaderProgram, camera)
        → Mesh::Draw for each mesh:
            activate program → bind VAO → bind textures →
            upload camPos/camMatrix/model/translation/rotation/scale →
            glDrawElements
        Writes real depth values into the depth buffer.
 7. glDepthFunc(GL_LEQUAL)         (allow the pinned-depth skybox through)
 8. skyboxShader.Activate()
    build translation-stripped view + projection; upload both
 9. glDisable(GL_CULL_FACE)
    bind skyboxVAO, bind cubemap on unit 0, glDrawElements(36)
    glEnable(GL_CULL_FACE)
10. glDepthFunc(GL_LESS)           (restore, or next frame inherits LEQUAL)
11. glfwSwapBuffers + glfwPollEvents
```

**Why the model is drawn before the skybox:** every skybox fragment landing behind existing geometry is rejected by the depth test *before its fragment shader ever runs*. Drawing the skybox first would shade a full screen of pixels that get painted over moments later. This ordering is a performance decision that the depth trick in §5.3 makes possible.

---

## Part 7 — The bugs you hit, as case studies

Each of these is the "stupid computer" principle producing a specific, non-obvious symptom. They're worth studying as a group because the *symptoms* look nothing like the *causes*.

### 7.1 Channel-count mismatch → diagonal shearing

**Symptom:** skybox looked torn/skewed with `sky_42` (PNG), fine with `og` (JPG).

**Cause:** the loop hardcoded `GL_RGB` (3 bytes/pixel). JPEG has no alpha so it really is 3 channels; the PNGs are RGBA — 4.

**Why shearing, specifically:** `glTexImage2D`'s `format` argument is how OpenGL computes where each row of pixels begins. Told 3 bytes/pixel on a 512-wide image, it steps 1536 bytes per row; the real rows are 2048 bytes apart. Row 1 is off by 512 bytes, row 2 by 1024, row 3 by 1536 — the error accumulates down the image, which reads visually as a **shear**, not a colour problem.

**Fix** — decide the format from what `stbi_load` actually reported:

```cpp
switch (faceChannels)
{
case 3: sourceFormat = GL_RGB;  break;
case 4: sourceFormat = GL_RGBA; break;
default:
    stbi_image_free(data);
    throw std::invalid_argument(/* names the file and the count */);
}
```

`internalFormat` stays `GL_RGBA` — GL fills missing alpha with 1.0. Only `format`, which describes the *incoming* buffer, has to match reality.

**Transferable lesson:** stride/format mismatches produce *geometric* distortion, not colour distortion. If an image looks skewed rather than miscoloured, suspect bytes-per-pixel before suspecting UVs.

### 7.2 The `8` vs `9` attribute offset (lessons 13–16)

`Mesh.cpp` had `(void*)(8 * sizeof(float))` for `texUV`. The correct offset is `9` — position(3) + normal(3) + color(3) = 9 floats before UV starts.

Offset 8 made the UV attribute read the last float of `color` plus the first of `texUV`. One UV component was effectively pinned to a constant, so textures smeared.

**Why it mattered beyond the visual bug:** while that was live, *every experiment on UV handling produced meaningless results.* Toggling `stbi_set_flip_vertically_on_load`, adding or removing the `mat2` rotation — all of it just rearranged garbage. Two "fixes" were adopted on the strength of noise.

**Transferable lesson, and the reason this one is worth remembering longest:** verify that your inputs are non-degenerate *before* reasoning about transforms applied to them. A debug shader that dumps the raw value —

```glsl
FragColor = vec4(texCoord, 0.0, 1.0);   // UVs should be a smooth 2D gradient
```

— answers in ten seconds what an hour of theorizing cannot.

### 7.3 A shader that didn't compile, and didn't say so

`gl_Position = vec4();` — an invalid zero-argument constructor. C++ built fine. The program ran. The skybox was simply absent.

Shaders compile at *runtime*, so shader errors never reach your build output. `compileErrors()` in `shaderClass.cpp` prints them to stdout.

**Transferable lesson:** when geometry is missing entirely, read the console before reading the code.

### 7.4 `facesCubemap->size()` — array-to-pointer decay

```cpp
std::string facesCubemap[6];
facesCubemap->size()   // == facesCubemap[0].size() — the LENGTH OF A PATH STRING
```

A C array decays to a pointer in most expressions, so `->` compiles and means `[0]`. The loop would have run ~80 times over a 6-element array — undefined behaviour.

Switching to `std::array<std::string, 6>` fixed it *structurally*: `std::array` never decays, so `.size()` is correct and `->` wouldn't compile at all.

**Transferable lesson:** prefer types that make the wrong code fail to compile over types that make it compile into undefined behaviour.

---

## Part 8 — Debugging checklist

When a draw shows nothing or shows something wrong, walk this in order. It converts "OpenGL is broken" into a finite list.

**Nothing appears at all:**

1. **Console output?** Shader compile/link errors, texture load failures, the new channel-count exception. Cheapest check, most common cause.
2. **Is the shader program active?** `Activate()` before its uniforms and its draw.
3. **Is the right VAO bound?** And did it retain an EBO (§2.3's unbind-order trap)?
4. **Is it being culled?** Temporarily `glDisable(GL_CULL_FACE)`. If it appears, it's a winding/culling issue, not a geometry issue.
5. **Is it being depth-rejected?** Check `glDepthFunc` and whether something already wrote to those pixels.
6. **Is it off-screen or inside-out?** Check camera position and the projection's near/far planes.

**It appears but looks wrong:**

7. **Skewed/sheared** → bytes-per-pixel mismatch (§7.1).
8. **Smeared or banded textures** → attribute offset/stride mismatch (§7.2). Dump UVs with a debug shader.
9. **Wrong image on wrong surface** → ordering, e.g. `facesCubemap` vs the `POSITIVE_X + i` sequence.
10. **Unlit / black surfaces** → check normals, and check that `camPos` and light uniforms actually arrived (a typo'd uniform name is silently ignored — §2.6).
11. **Correct shape, wrong position** → uniform matrix upload, or matrix multiplication order.

**A general technique that beats all of the above:** make the shader output the suspect value directly.

```glsl
FragColor = vec4(texCoord, 0.0, 1.0);                 // UVs
FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0); // normals
FragColor = vec4(vec3(gl_FragCoord.z), 1.0);          // depth
```

Seeing the actual data beats reasoning about what it probably is.

---

## Part 9 — Self-test

Answer without looking. If one is uncomfortable, the section is listed.

1. What does a VAO actually store? Does it contain vertex data? *(§2.4)*
2. Why does `Mesh.cpp` unbind the VAO *before* the EBO? *(§2.3)*
3. You add a `glm::vec3 tangent` field to `Vertex`. Name every place that must change. *(§2.1, §2.4)*
4. Why is the skybox's stride `3 * sizeof(float)` while the model's is `sizeof(Vertex)`? *(§2.4)*
5. What does a `samplerCube` take that a `sampler2D` does not? *(§5.2)*
6. Why does the skybox VAO need no texture-coordinate attribute? *(§5.2)*
7. What does `gl_Position = vec4(pos.x, pos.y, pos.w, pos.w)` accomplish, and which hardware step makes it work? *(§5.3, Part 4)*
8. Why is `GL_LEQUAL` required rather than the default `GL_LESS`? *(§5.3)*
9. Why is `glDepthFunc(GL_LESS)` restored at the end of every frame? *(Model 1, §5.3)*
10. Why is the model drawn *before* the skybox, when the skybox is conceptually behind it? *(Part 6)*
11. Why does culling need to be disabled specifically for the skybox? *(§5.4)*
12. A texture appears diagonally sheared. What is your first hypothesis? *(§7.1)*
13. Your model renders as a black silhouette. List three candidate causes. *(Part 8)*
14. You misspell a uniform name in `glGetUniformLocation`. What happens? *(§2.6)*
15. You have a GLSL syntax error. Does the build fail? Where does the error appear? *(§2.5, §7.3)*

---

## Part 10 — Experiments

Predict the outcome, then run it, then explain any mismatch. The mismatches are where the learning is.

| # | Change | What it teaches |
|---|---|---|
| 1 | Swap `front`/`back` in `facesCubemap` | Cubemap ordering is a convention nothing enforces (§5.1) |
| 2 | Use `gl_Position = pos;` in `skybox.vert` | The skybox starts occluding real geometry (§5.3 Trick A) |
| 3 | Drop the `glm::mat3(...)` truncation | The skybox slides with the camera (§5.3 Trick B) |
| 4 | Remove `glDisable(GL_CULL_FACE)` around the skybox draw | Which faces vanish, and why *those* (§5.4) |
| 5 | Feed a 1-channel greyscale image as a face | The new `default:` throws with the filename (§7.1) |
| 6 | Change `texUV`'s offset from `9` to `8` in `Mesh.cpp` | Reproduce the historical bug deliberately, then find it with a debug shader (§7.2) |
| 7 | `FragColor = vec4(texCoords * 0.5 + 0.5, 1.0)` in `skybox.frag` | Visualize the sampling direction as colour — see the cube's axes directly |
| 8 | Delete `glDepthFunc(GL_LESS)` restoration | Watch state leak between frames (Model 1) |

---

## The compact summary

If you retain one page, retain this.

**The setup chain:**

```text
Vertex struct defines the byte layout
  → VBO holds the bytes on the GPU
  → VAO records how to read them (+ which EBO to use)
  → EBO says which vertices form which triangles
  → Shader program is compiled at runtime from .vert/.frag text
  → Textures upload pixels; units connect them to sampler uniforms
```

**Before every draw:**

```text
activate program → bind VAO → bind textures → upload uniforms → draw
```

**The skybox, specifically:**

```text
GL_TEXTURE_CUBE_MAP  = one object, six faces (POSITIVE_X + 0..5)
                       format must match stbi_load's real channel count
                       sampled by 3D DIRECTION, not 2D UV
skybox.vert          : position doubles as direction (cube centred on origin)
                       z forced to w → depth always 1.0 (far plane)
Main.cpp             : view truncated through mat3 → translation discarded
render order         : model first (writes real depth)
                       → GL_LEQUAL → skybox fills only untouched pixels
                       → restore GL_LESS
```

**And the principle underneath all of it:** the machine is told everything, checks almost nothing, and fails quietly. Most graphics debugging is finding the one thing you assumed it knew.
