#version 330 core

out vec4 FragColor;

in vec2 texCoord;
in vec3 fragNormal;
in vec3 fragPos;

uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec3 camPos;
// Switches the specular model. false falls back to the original Phong.
uniform bool useBlinnPhong;

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
		vec3 viewDirection = normalize(camPos - fragPos);
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
	vec3 toLight = lightPos - fragPos;
	float distanceToLight = length(toLight);
	float intensity = lightStrength * computePointAttenuation(distanceToLight);

	// The one normal the plane has, until a normal map replaces it per fragment.
	vec3 normal = normalize(fragNormal);
	vec3 lightDirection = normalize(toLight);
	LightingTerms lighting = computePhongTerms(normal, lightDirection);

	return composeLitColor(lighting, intensity);
}

void main()
{
	FragColor = computePointLightColor();
}
