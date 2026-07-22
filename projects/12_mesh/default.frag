#version 330 core

out vec4 FragColor;

in vec2 texCoord;
in vec3 Normal;
in vec3 crntPos;

uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec3 camPos;

// Return ambient, diffuse, and specular strengths in one vector.
vec3 computePhongTerms(vec3 normal, vec3 lightDirection, float ambientStrength, float specularStrength, float shininess)
{
	float diffuse = max(dot(normal, lightDirection), 0.0f);
	vec3 viewDirection = normalize(camPos - crntPos);
	vec3 reflectionDirection = reflect(-lightDirection, normal);
	float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), shininess);
	float specular = specAmount * specularStrength;

	return vec3(ambientStrength, diffuse, specular);
}

// Fade smoothly between the spotlight's inner and outer cone.
float computeSpotIntensity(vec3 lightDirection, vec3 coneDirection, float outerCone, float innerCone)
{
	float angle = dot(coneDirection, -lightDirection);
	return clamp((angle - outerCone) / (innerCone - outerCone), 0.0f, 1.0f);
}

// Apply the lighting terms to the material textures.
vec4 composeLitColor(vec3 terms, float lightIntensity)
{
	vec4 diffuseTex = texture(diffuse0, texCoord);
	float specMap = texture(specular0, texCoord).r;
	float ambient = terms.x;
	float diffuse = terms.y;
	float specular = terms.z;

	return (diffuseTex * (diffuse * lightIntensity + ambient) + specMap * specular * lightIntensity) * lightColor;
}

vec4 computeSpotLightColor()
{
	// A larger inner-cone value produces a narrower bright center.
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
	FragColor = computeSpotLightColor();
}
