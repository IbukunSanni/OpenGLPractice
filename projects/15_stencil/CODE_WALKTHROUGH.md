# 12_mesh: Building a Mental Model

This project draws two meshes:

- a textured floor using a diffuse map, a specular map, and spotlight lighting;
- a small white cube that marks the light position.

The most useful mental model is:

> C++ creates data and OpenGL objects. Bind calls select the current objects. Setup calls record how those objects are connected. A draw call consumes the currently selected state.

OpenGL is a state machine. Calls such as `glBufferData`, `glVertexAttribPointer`, `glUniform*`, and `glDrawElements` act on objects that were selected by earlier bind or activation calls.

## Recommended file-reading order

Use two passes. The first pass gives you the story; the second explains the mechanics.

### First pass: understand the program

1. **`Main.cpp`**
   - See the vertex/index data, initialization order, render loop, and two draw calls.
   - Do not stop to understand every class yet.

2. **`VBO.h`**
   - Study `Vertex`. It defines the CPU memory layout sent to the GPU.

3. **`Mesh.h` and `Mesh.cpp`**
   - See how vertices, indices, textures, and a VAO become one drawable unit.

4. **`default.vert`**
   - Match shader input locations to the `Vertex` fields configured in `Mesh.cpp`.

5. **`default.frag`**
   - Follow the interpolated values and textures into the final pixel color.

6. **`Texture.h` and `Texture.cpp`**
   - Understand image loading, texture units, texture objects, and sampler uniforms.

7. **`Camera.h` and `Camera.cpp`**
   - Follow how input produces the `camMatrix` and camera world position.

8. **`light.vert` and `light.frag`**
   - Compare this simple shader path with the textured/illuminated floor path.

### Second pass: understand OpenGL state

1. `VBO.cpp` — vertex allocation and upload.
2. `EBO.cpp` — index allocation and upload.
3. `VAO.cpp` — attribute descriptions and recorded bindings.
4. `Mesh.cpp` constructor — how the VBO, EBO, and VAO are connected.
5. `Mesh::Draw` — which state is restored before drawing.
6. `shaderClass.cpp` — shader compilation, linking, and activation.
7. `Main.cpp` render loop — uniform updates and draw order.

After the second pass, read `Main.cpp` once more. It should feel much smaller because the class calls will now represent complete ideas.

## 1. CPU memory and GPU memory are separate

`Main.cpp` begins with CPU-side vectors:

```cpp
const std::vector<Vertex> floorVertices = { ... };
const std::vector<GLuint> floorIndices = { ... };
```

These vectors live in normal process memory (RAM). OpenGL cannot draw directly from these vectors in this project. Their data is copied into GPU-accessible buffer objects.

```text
CPU RAM                              GPU/OpenGL storage
-------------------------------      ------------------------------
std::vector<Vertex>                  VBO (vertex buffer object)
std::vector<GLuint>                  EBO (element buffer object)
stb_image pixel bytes                OpenGL 2D texture object
model/camera/light values            shader program uniforms
```

The important consequence is that changing a CPU vector after `glBufferData` does not automatically update the GPU copy. You would need another upload, such as `glBufferSubData`, to synchronize it.

## 2. The `Vertex` structure is the memory contract

`VBO.h` defines one interleaved vertex:

```cpp
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texUV;
};
```

The attributes for one vertex are adjacent in memory, followed by the same attributes for the next vertex:

```text
Vertex 0: [position xyz][normal xyz][color rgb][UV xy]
Vertex 1: [position xyz][normal xyz][color rgb][UV xy]
Vertex 2: [position xyz][normal xyz][color rgb][UV xy]
```

`sizeof(Vertex)` is the stride: the number of bytes from the start of one vertex to the start of the next vertex.

`Mesh.cpp` describes this layout to OpenGL:

| Shader location | Vertex field | Components | Offset |
|---:|---|---:|---:|
| 0 | `position` | 3 floats | `0` |
| 1 | `normal` | 3 floats | `3 * sizeof(float)` |
| 2 | `color` | 3 floats | `6 * sizeof(float)` |
| 3 | `texUV` | 2 floats | `9 * sizeof(float)` |

The current floor shader consumes locations 0, 1, and 3. Location 2 remains part of the reusable mesh layout even though this shader does not use vertex color.

When changing `Vertex`, update the attribute declarations in `Mesh.cpp` and the matching `layout(location = ...)` declarations in the vertex shader.

## 3. What VBO binding means

The VBO constructor performs three steps:

```cpp
glGenBuffers(1, &ID);
glBindBuffer(GL_ARRAY_BUFFER, ID);
glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
```

Think of them as:

1. `glGenBuffers` gives you an OpenGL handle.
2. `glBindBuffer(GL_ARRAY_BUFFER, ID)` selects that handle as the current vertex buffer.
3. `glBufferData(GL_ARRAY_BUFFER, ...)` allocates storage for the selected buffer and copies the CPU vertices into it.

`GL_STATIC_DRAW` is a usage hint: upload rarely, draw many times.

Binding does not copy data by itself. It only selects the object that later commands will affect.

## 4. What EBO binding means

The EBO contains indices, not complete vertices:

```cpp
0, 1, 2,
0, 2, 3
```

Those six values form two triangles while reusing vertices 0 and 2.

The EBO upload is similar to the VBO upload:

```cpp
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...);
```

There is one critical difference:

> The `GL_ELEMENT_ARRAY_BUFFER` binding is stored inside the currently bound VAO.

That is why `Mesh.cpp` unbinds the VAO before unbinding the EBO. If the EBO were unbound while the mesh VAO was still bound, the VAO would remember “no EBO,” and `glDrawElements` would not have an index buffer.

## 5. A VAO is a saved input recipe

A VAO does not normally contain the vertex bytes. It stores the information needed to retrieve and interpret them.

For each enabled attribute, the VAO records:

- the attribute location;
- the component count and type;
- the stride and byte offset;
- which VBO was bound when `glVertexAttribPointer` was called.

It also stores the EBO binding.

The important setup sequence in the `Mesh` constructor is:

```text
Bind mesh VAO
  Create/bind/upload VBO
  Create/bind/upload EBO
  Describe vertex attributes with glVertexAttribPointer
Unbind mesh VAO
Unbind VBO and EBO
```

Inside `VAO::LinkAttrib`, this happens:

```cpp
VBO.Bind();
glVertexAttribPointer(...);
glEnableVertexAttribArray(layout);
VBO.Unbind();
```

Because the mesh VAO is bound at that time, the attribute description is recorded in that VAO. Unbinding the VBO afterward does not erase the recorded connection.

A good shorthand is:

```text
VBO = vertex bytes
EBO = which vertices form triangles
VAO = how to read the VBO + which EBO to use
```

## 6. What `Mesh` represents

`Mesh` groups the information needed for one indexed draw:

- a CPU copy of its vertices;
- a CPU copy of its indices;
- its material textures;
- a VAO that restores the GPU input configuration.

The constructor performs one-time GPU setup. `Draw` performs per-frame state selection.

### Constructor: one-time setup

```text
Mesh constructor
  -> copy CPU mesh data
  -> bind VAO
  -> upload VBO
  -> upload EBO
  -> record attribute layout
  -> preserve EBO binding in VAO
```

### Draw: per-frame use

```text
Mesh::Draw
  -> activate shader program
  -> bind mesh VAO
  -> connect sampler uniforms to texture units
  -> bind textures to those units
  -> upload camera position and matrix
  -> glDrawElements
```

The final call is:

```cpp
glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
```

OpenGL now has enough state to answer:

- Which program runs? The active shader program.
- Where are the indices? The EBO stored by the bound VAO.
- Where are the vertex bytes? The VBOs captured by the VAO attributes.
- How are the bytes decoded? The VAO attribute descriptions.
- Which images are sampled? The textures bound to the units named by the sampler uniforms.

## 7. Texture objects, units, and samplers

These are three different things:

1. **Texture object** — owns the uploaded image and sampling settings.
2. **Texture unit** — a slot through which shaders can access a bound texture.
3. **Sampler uniform** — stores the integer index of the unit to read.

For the floor:

| Material role | Texture object | Unit index | Sampler name |
|---|---|---:|---|
| diffuse | `planks.png` | 0 | `diffuse0` |
| specular | `planksSpec.png` | 1 | `specular0` |

The mapping works like this:

```cpp
glActiveTexture(GL_TEXTURE0 + unit); // select unit 0 or 1
glBindTexture(GL_TEXTURE_2D, ID);     // bind an image to that unit
glUniform1i(samplerLocation, unit);   // tell the shader which unit to read
```

Notice that the sampler receives `0` or `1`, not `GL_TEXTURE0` or `GL_TEXTURE1`.

`Texture.cpp` loads pixels into CPU memory with `stbi_load`, uploads them with `glTexImage2D`, builds mipmaps, then releases the CPU pixel array. The OpenGL texture keeps its own uploaded copy.

`Mesh::Draw` uses each texture's material role to create the matching GLSL name:

```text
"diffuse"  + "0" -> "diffuse0"
"specular" + "0" -> "specular0"
```

Those names must match the sampler declarations in `default.frag` exactly.

## 8. Shader program state

`Shader` reads the `.vert` and `.frag` files, compiles them, and links them into one program object.

```cpp
shader.Activate(); // glUseProgram(shader.ID)
```

After activation, `glUniform*` calls update uniforms in that program. A uniform value remains part of the program's state until you replace it or delete the program.

This explains why `lightColor` is uploaded once before the loop, while `model`, `lightPos`, `camPos`, and `camMatrix` are refreshed as needed.

## 9. Coordinate spaces through the floor shader

The floor vertex starts in object/model space:

```text
aPos (object space)
  -> model
world position
  -> view
camera/eye position
  -> projection
clip-space gl_Position
```

The code combines view and projection in `camMatrix`:

```glsl
gl_Position = camMatrix * model * vec4(aPos, 1.0);
```

Lighting is calculated in world space, so the vertex shader also sends:

```glsl
crntPos = vec3(model * vec4(aPos, 1.0));
Normal = mat3(transpose(inverse(model))) * aNormal;
```

The inverse-transpose normal matrix keeps normals correct if the model later receives non-uniform scaling.

The rasterizer interpolates `crntPos`, `Normal`, and `texCoord` across every triangle before running the fragment shader.

## 10. How the fragment shader creates a pixel

For each covered fragment, `default.frag`:

1. normalizes the interpolated normal;
2. calculates the direction from the fragment to the light;
3. calculates ambient, diffuse, and specular strengths;
4. calculates spotlight cone intensity;
5. samples the diffuse and specular textures;
6. combines the material samples with lighting and `lightColor`;
7. writes `FragColor`.

The depth test then decides whether this fragment is closer than the value already stored in the depth buffer.

## 11. One complete frame in order

Follow this sequence in `Main.cpp`:

```text
1. Clear color and depth buffers.
2. Build the light and floor model matrices.
3. Activate the light shader and upload the light model matrix.
4. Activate the floor shader and upload floor model + light position.
5. Process camera input and rebuild the camera matrix.
6. floor.Draw(...)
     - activate floor shader
     - bind floor VAO
     - bind diffuse/specular textures
     - upload camera uniforms
     - draw 6 indices
7. light.Draw(...)
     - activate light shader
     - bind light VAO
     - upload camera uniforms
     - draw 36 indices
8. Swap the back buffer onto the screen.
9. Poll window and input events.
```

The floor and light cube have different VAOs and shader programs. Calling `Draw` for the second mesh replaces the state selected for the first mesh.

## 12. A “what is bound?” debugging method

When a draw is blank or incorrect, stop at `glDrawElements` and ask these questions in order:

1. **Program:** Which shader program was last passed to `glUseProgram`?
2. **VAO:** Which VAO was last passed to `glBindVertexArray`?
3. **EBO:** Did that VAO record a valid element buffer?
4. **Attributes:** Do shader locations match the VAO locations, sizes, stride, and offsets?
5. **Textures:** Is each texture bound on the unit stored in its sampler uniform?
6. **Uniforms:** Were `model`, `camMatrix`, light values, and camera position uploaded to the correct program?
7. **Pipeline state:** Are the viewport and depth test configured as expected?

This turns “OpenGL shows nothing” into a finite state checklist.

## 13. Object lifetime in the current project

An OpenGL ID is a handle, not the GPU memory itself. A small C++ wrapper going out of scope does not delete its OpenGL object unless its destructor or your code calls `glDelete*`.

In the current `Mesh` constructor, the local `VBO` and `EBO` wrapper variables go out of C++ scope after setup. Their OpenGL buffers remain alive because their `Delete` methods are not called. The VAO still remembers the buffer connections, so drawing works.

This is functional for this learning project, and the graphics driver releases remaining objects when the OpenGL context is destroyed. A production design would normally make `Mesh` explicitly own its VBO and EBO and delete them through controlled RAII cleanup.

Textures and VAOs are explicitly deleted near the end of `Main.cpp`, before the window/context is destroyed.

## 14. Small experiments that reinforce the model

Try one change at a time and predict the result before running:

1. **Index experiment:** Change `floorIndices` to one triangle. Confirm that only half the floor draws.
2. **UV experiment:** Swap two `texUV` values. Watch the texture orientation change without moving geometry.
3. **Binding experiment:** Temporarily skip `VAO.Bind()` in `Mesh::Draw`. The indexed draw should fail or use incorrect state.
4. **Sampler experiment:** Swap texture unit 0 and 1. Observe diffuse/specular data being interpreted incorrectly.
5. **Transform experiment:** Translate `floorModel`. Confirm that `crntPos` and `gl_Position` both follow it.
6. **Normal experiment:** Apply non-uniform scale and compare the current inverse-transpose normal calculation with simply passing `aNormal`.
7. **Shader experiment:** Output `vec4(normalize(Normal) * 0.5 + 0.5, 1.0)` to visualize interpolated normals.

## Compact mental model

If you remember only one chain, remember this:

```text
Vertex struct
  -> VBO stores bytes
  -> VAO explains the bytes
  -> EBO selects vertex order
  -> vertex shader transforms vertices
  -> rasterizer interpolates outputs
  -> fragment shader samples textures and lights fragments
  -> depth test chooses visible fragments
  -> framebuffer appears when GLFW swaps buffers
```

And before every draw:

```text
activate program -> bind VAO -> bind textures -> upload uniforms -> draw
```
