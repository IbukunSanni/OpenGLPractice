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

// Computes ambient, diffuse, specular terms for Phong lighting.
// Return order: (ambient, diffuse, specular).
vec3 computePhongTerms(vec3 normal, vec3 lightDirection, float ambientStrength, float specularStrength, float shininess)
{
	float diffuse = max(dot(normal, lightDirection), 0.0f);
	vec3 viewDirection = normalize(camPos - crntPos);
	vec3 reflectionDirection = reflect(-lightDirection, normal);
	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), shininess);
	float specular = specAmount * specularStrength;

	return vec3(ambientStrength, diffuse, specular);
}

// Computes distance attenuation for a point light.
float computePointAttenuation(float distanceToLight, float linearFactor, float quadraticFactor)
{
	return 1.0f / (quadraticFactor * distanceToLight * distanceToLight + linearFactor * distanceToLight + 1.0f);
}

// Computes cone intensity for a spotlight.
float computeSpotIntensity(vec3 lightDirection, vec3 coneDirection, float outerCone, float innerCone)
{
	float angle = dot(coneDirection, -lightDirection);
	return clamp((angle - outerCone) / (innerCone - outerCone), 0.0f, 1.0f);
}

// Combines texture color + specular map with light terms.
// lightIntensity controls attenuation/cone strength.
vec4 composeLitColor(vec3 terms, float lightIntensity)
{
	vec4 diffuseTex = texture(tex0, texCoord);
	float specMap = texture(tex1, texCoord).r;
	float ambient = terms.x;
	float diffuse = terms.y;
	float specular = terms.z;

	return (diffuseTex * (diffuse * lightIntensity + ambient) + specMap * specular * lightIntensity) * lightColor;
}

// Local point light with distance falloff.
vec4 computePointLightColor()
{
	// Light vector from fragment to point light.
	vec3 lightVec = lightPos - crntPos;
	float dist = length(lightVec);
	float linearFactor = 12.0f;
	float quadraticFactor = 12.0f;
	float intensity = computePointAttenuation(dist, linearFactor, quadraticFactor);

	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(lightVec);
	vec3 terms = computePhongTerms(normal, lightDirection, 0.20f, 0.50f, 16.0f);

	return composeLitColor(terms, intensity);
}

// Global directional light (no distance attenuation).
vec4 computeDirectionalLightColor()
{
	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(vec3(1.0f, 1.0f, 0.0f));
	vec3 terms = computePhongTerms(normal, lightDirection, 0.20f, 0.50f, 16.0f);

	return composeLitColor(terms, 1.0f);
}

// Spotlight using cone-based intensity.
vec4 computeSpotLightColor()
{
	// Controls spotlight cone width.
	float outerCone = 0.90f;
	float innerCone = 0.95f;

	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(lightPos - crntPos);
	vec3 terms = computePhongTerms(normal, lightDirection, 0.20f, 0.50f, 16.0f);
	float intensity = computeSpotIntensity(lightDirection, vec3(0.0f, -1.0f, 0.0f), outerCone, innerCone);

	return composeLitColor(terms, intensity);
}

void main()
{
	FragColor = computePointLightColor();
}
