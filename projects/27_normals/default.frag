#version 330 core

out vec4 FragColor;

in vec2 texCoord;
// TANGENT space, all three, courtesy of default.geom. Lighting is done in that
// space so the sampled normal can be used with no transform of its own.
in vec3 fragPosTangent;
in vec3 lightPosTangent;
in vec3 camPosTangent;

uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform sampler2D normal0;
uniform vec4 lightColor;
// Switches the specular model. false falls back to the original Phong.
uniform bool useBlinnPhong;
// false falls back to the plane's own flat normal, so the map can be compared
// against the geometry it is standing in for.
uniform bool useNormalMap;

// Stands in for bounced light. Lowering it darkens what the light misses, which
// is what makes the falloff and highlight readable.
const float sceneAmbient = 0.05f;

// Brightness, separate from the falloff constants which set its shape.
const float lightStrength = 1.5f;
const float attenuationLinear = 0.09f;
const float attenuationQuadratic = 0.25f;

const float materialSpecularStrength = 0.50f;
const float materialShininess = 16.0f;

struct LightingTerms
{
	float ambient;
	float diffuse;
	float specular;
};

// Phong reflects about the normal and measures to the viewer; Blinn uses the
// halfway vector, keeping a highlight at grazing angles where Phong clamps to 0.
// Blinn's angle is ~half Phong's, so it looks wider at the same exponent.
LightingTerms computePhongTerms(vec3 normal, vec3 lightDirection)
{
	LightingTerms lighting;
	lighting.ambient = sceneAmbient;
	lighting.diffuse = max(dot(normal, lightDirection), 0.0f);
	lighting.specular = 0.0f;

	// Only where the surface faces the light: with diffuse clamped to 0 the
	// view/reflection dot can still go positive and paint an unlit spot.
	if (lighting.diffuse > 0.0f)
	{
		vec3 viewDirection = normalize(camPosTangent - fragPosTangent);
		float specularFactor;

		if (useBlinnPhong)
		{
			vec3 halfwayVec = normalize(viewDirection + lightDirection);
			specularFactor = pow(max(dot(normal, halfwayVec), 0.0f), materialShininess);
		}
		else
		{
			vec3 reflectionDirection = reflect(-lightDirection, normal);
			specularFactor = pow(max(dot(viewDirection, reflectionDirection), 0.0f), materialShininess);
		}

		lighting.specular = specularFactor * materialSpecularStrength;
	}

	return lighting;
}

// Distance falloff for a positional light: 1 / (quadratic*d^2 + linear*d + 1).
float computePointAttenuation(float distanceToLight)
{
	return 1.0f / (attenuationQuadratic * distanceToLight * distanceToLight
		+ attenuationLinear * distanceToLight + 1.0f);
}

// Apply the lighting terms to the material textures.
vec4 composeLitColor(LightingTerms lighting, float lightIntensity)
{
	vec4 diffuseSample = texture(diffuse0, texCoord);
	float specularSample = texture(specular0, texCoord).r;

	return (diffuseSample * (lighting.diffuse * lightIntensity + lighting.ambient)
		+ specularSample * lighting.specular * lightIntensity) * lightColor;
}

// Single point light, unshadowed. A lone flat plane has nothing to occlude
// itself with, so the shadow maps went out with the rest of the old scene.
vec4 computePointLightColor()
{
	vec3 toLight = lightPosTangent - fragPosTangent;
	float distanceToLight = length(toLight);
	float intensity = lightStrength * computePointAttenuation(distanceToLight);

	// On: a per-fragment normal decoded from the map, [0,1] texel back to a
	// [-1,1] vector. Off: (0,0,1), which IS the unperturbed surface normal in
	// tangent space -- no varying needed for it.
	vec3 normal = useNormalMap
		? normalize(texture(normal0, texCoord).xyz * 2.0f - 1.0f)
		: vec3(0.0f, 0.0f, 1.0f);
	vec3 lightDirection = normalize(toLight);
	LightingTerms lighting = computePhongTerms(normal, lightDirection);

	return composeLitColor(lighting, intensity);
}

void main()
{
	FragColor = computePointLightColor();
}
