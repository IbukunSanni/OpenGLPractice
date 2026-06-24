#version 330 core

// Final output color written to the framebuffer for this fragment
out vec4 FragColor;

// ── Interpolated inputs from the vertex shader ──────────────────────────────
in vec3 color;     // Per-vertex color, blended across the triangle
in vec2 texCoord;  // UV coordinate for texture sampling
in vec3 Normal;    // Surface normal in world space
in vec3 crntPos;   // Fragment position in world space (needed to compute light and view directions)

// ── Uniforms set from the CPU ────────────────────────────────────────────────
uniform sampler2D tex0;   // Diffuse/albedo texture
uniform sampler2D tex1;   // Specular map texture (optional)
uniform vec4 lightColor;  // RGBA color of the light source
uniform vec3 lightPos;    // World-space position of the point light
uniform vec3 camPos;      // World-space camera position (required for specular view direction)
uniform float attenuationA;
uniform float attenuationB;

vec4 pontLight(){

	vec3 lightVec = lightPos - crntPos;

	float dist = length(lightVec);
	float a = 0.02;
	float b = 0.001;
	float attenuation = 1.0 / (1.0 + a * dist + b * dist * dist);

	// Renormalize after interpolation — blending normals across a triangle can
	// shorten them, which would produce incorrect dot product results below.
	vec3 normal = normalize(Normal);

	// Direction from the surface fragment toward the light source.
	vec3 lightDirection = normalize(lightVec);

	// ── Ambient ─────────────────────────────────────────────────────────────
	// A small constant added so surfaces never go completely black.
	// Simulates indirect light that has scattered around the scene.
	float ambient = 0.15f;

	// ── Diffuse ─────────────────────────────────────────────────────────────
	// Lambert's cosine law: brightness is proportional to how directly the
	// surface faces the light.
	//   dot == 1 → surface faces the light head-on  → full brightness
	//   dot == 0 → surface is edge-on to the light  → no diffuse contribution
	//   dot <  0 → light is behind the surface       → clamped to 0
	float diffuse = max(dot(normal, lightDirection), 0.0f);

	// ── Specular (Phong reflection model) ───────────────────────────────────
	// Models the bright highlight visible when the view direction aligns with
	// the mirror-reflected light ray (think shiny plastic or polished metal).

	// Overall strength of the specular contribution.
	float specularLight = 0.50f;

	// Direction from this surface fragment toward the camera.
	vec3 viewDirection = normalize(camPos - crntPos);

	// Perfect mirror-reflection of the incoming light about the surface normal.
	// reflect() expects the *incident* direction (light → surface), so we negate
	// lightDirection (which points surface → light) to get the right sign.
	vec3 reflectionDirection = reflect(-lightDirection, normal);

	// How well the viewer's direction aligns with the reflection direction.
	// pow(..., 16) sharpens the falloff: higher exponent = tighter, shinier highlight.
	//   exponent  8 → broad, matte-looking highlight
	//   exponent 64 → tight, mirror-like highlight
	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16);
	float specular = specAmount * specularLight;

	// ── Final colour composition ─────────────────────────────────────────────
	// Sample the texture, then scale by the sum of all three lighting terms
	// multiplied by the light's own colour. Adding ambient + diffuse + specular
	// together is the core of the Phong shading model.
	vec4 texColor = texture(tex0, texCoord);
	float specMap = texture(tex1, texCoord).r;
	vec4 diffuseTerm = texColor * (ambient + diffuse * attenuation);
	vec4 specularTerm = vec4(vec3(specMap * specular * attenuation), 0.0);
	return (diffuseTerm + specularTerm) * lightColor;

}

void main()
{
	FragColor = pontLight();
}
