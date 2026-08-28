#version 330 core

out vec4 FragColor;

in vec2 texCoord;
in vec3 fragNormal;
in vec3 fragPos;
// Fragment position in the light's clip space, for the 2D shadow lookup.
in vec4 fragPosLight;

uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform sampler2D shadowMap;
// Indexed by a direction, not a UV: hand it light-to-fragment and the hardware
// picks face and texel. Needs its own unit (3) -- sampling a unit with a 2D
// texture bound through a samplerCube is undefined.
uniform samplerCube shadowCubeMap;
uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec3 camPos;
// Puts closestDepth back into world units, undoing shadowCubeMap.frag's divide.
uniform float shadowFarPlane;
// Axis of the spotlight cone, pointing away from the light.
uniform vec3 spotDirection;
uniform int lightMode;
// Switches the specular model. false falls back to the original Phong.
uniform bool useBlinnPhong;

// Must match the LightMode enum in Main.cpp -- the two halves of one contract.
const int MODE_DIRECTIONAL = 0;
const int MODE_SPOT        = 1;
const int MODE_POINT       = 2;

// Stands in for bounced light. Lowering it darkens what the light misses, which
// is what makes the falloff and highlight readable. Was 0.20.
const float sceneAmbient = 0.05f;

// Brightness, separate from the falloff constants which set its shape.
// Was 6.0 when the falloff drove intensity to ~0.04; the retuned falloff reaches
// ~0.65, so 6.0 would now clip every lit texel to white. Retune both together.
const float lightStrength = 1.5f;
const float attenuationLinear = 0.09f;
const float attenuationQuadratic = 0.25f;

// One material response for all three lights; these were repeated per call site.
const float materialSpecularStrength = 0.50f;
const float materialShininess = 16.0f;

// Slope-scaled depth bias, per light. The three pairs are NOT comparable: the 2D
// ones are in normalised depth, and perspective bunches that toward the near
// plane (hence the far smaller spot values), while the point pair is world units.
const float dirBiasSlope    = 0.005f,   dirBiasMin   = 0.0005f;
const float spotBiasSlope   = 0.00025f, spotBiasMin  = 0.000005f;
const float pointBiasSlope  = 0.02f,    pointBiasMin = 0.002f;

// Radius 2 gives a 5x5 kernel on the 2D maps and 5x5x5 on the cubemap.
const int shadowSampleRadius = 2;
// World-space jitter of the cubemap lookup direction, since a cube has no single
// texel grid to step along the way textureSize gives the 2D maps.
const float cubeSampleOffset = 0.005f;

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

// Fade smoothly between the spotlight's inner and outer cone.
float computeSpotIntensity(vec3 lightDirection, vec3 coneDirection, float outerCone, float innerCone)
{
	// A cosine, not an angle -- dot of two unit vectors. Larger means closer to
	// the cone axis, which is why innerCone is the BIGGER of the two constants.
	float cosAngle = dot(coneDirection, -lightDirection);
	return clamp((cosAngle - outerCone) / (innerCone - outerCone), 0.0f, 1.0f);
}

// Apply the lighting terms to the material textures.
vec4 composeLitColor(LightingTerms lighting, float lightIntensity)
{
	vec4 diffuseSample = texture(diffuse0, texCoord);
	float specularSample = texture(specular0, texCoord).r;

	return (diffuseSample * (lighting.diffuse * lightIntensity + lighting.ambient)
		+ specularSample * lighting.specular * lightIntensity) * lightColor;
}

// Fraction occluded, 0 to 1, from the 2D map. Shared by the directional and spot
// lights: the lookup is identical and only the bias constants differ, which is
// the point -- one map and one lookup cover both, ortho or perspective.
float computeShadowMapOcclusion(vec3 normal, vec3 lightDirection, float biasSlope, float biasMin)
{
	// Inert under ortho (w stays 1), the real projective divide under perspective.
	vec3 lightSpaceCoords = fragPosLight.xyz / fragPosLight.w;
	if (lightSpaceCoords.z > 1.0f)
		return 0.0f;

	// From [-1, 1] to the [0, 1] the map is stored in.
	lightSpaceCoords = (lightSpaceCoords + 1.0f) / 2.0f;
	float currentDepth = lightSpaceCoords.z;
	float bias = max(biasSlope * (1.0f - dot(normal, lightDirection)), biasMin);

	float shadow = 0.0f;
	vec2 texelSize = 1.0f / vec2(textureSize(shadowMap, 0));
	for (int y = -shadowSampleRadius; y <= shadowSampleRadius; y++)
	{
		for (int x = -shadowSampleRadius; x <= shadowSampleRadius; x++)
		{
			float closestDepth = texture(shadowMap, lightSpaceCoords.xy + vec2(x, y) * texelSize).r;
			if (currentDepth > closestDepth + bias)
				shadow += 1.0f;
		}
	}

	int sideLength = shadowSampleRadius * 2 + 1;
	return shadow / float(sideLength * sideLength);
}

// Fraction occluded, 0 to 1, from the cubemap. The 2D lookup cannot be reused: it
// compares depths within one frustum, and a point light has six. Distance from
// the light is the one quantity well defined in all of them.
float computeShadowCubeOcclusion(vec3 normal, vec3 lightDirection)
{
	// Light TO fragment, which is also the cubemap lookup direction. Reversing it
	// samples the opposite face. (This was named fragToLight, its own opposite.)
	vec3 lightToFrag = fragPos - lightPos;
	float currentDepth = length(lightToFrag);
	float bias = max(pointBiasSlope * (1.0f - dot(normal, lightDirection)), pointBiasMin);

	float shadow = 0.0f;
	for (int z = -shadowSampleRadius; z <= shadowSampleRadius; z++)
	{
		for (int y = -shadowSampleRadius; y <= shadowSampleRadius; y++)
		{
			for (int x = -shadowSampleRadius; x <= shadowSampleRadius; x++)
			{
				float closestDepth = texture(shadowCubeMap, lightToFrag + vec3(x, y, z) * cubeSampleOffset).r;
				// currentDepth was never normalised, so undo the divide or the
				// comparison is nonsense.
				closestDepth *= shadowFarPlane;
				if (currentDepth > closestDepth + bias)
					shadow += 1.0f;
			}
		}
	}

	// Cubed, not squared -- one dimension more than the 2D average.
	int sideLength = shadowSampleRadius * 2 + 1;
	return shadow / float(sideLength * sideLength * sideLength);
}

// Falls off with distance from lightPos and casts in every direction at once.
vec4 computePointLightColor()
{
	vec3 toLight = lightPos - fragPos;
	float distanceToLight = length(toLight);
	// Retuned for this scene. 12/12 came from 11_light's sub-unit distances; over
	// the island's ~3 units it drives intensity to 0.04 and reads as black.
	float intensity = lightStrength * computePointAttenuation(distanceToLight);

	vec3 normal = normalize(fragNormal);
	vec3 lightDirection = normalize(toLight);
	LightingTerms lighting = computePhongTerms(normal, lightDirection);

	float shadow = computeShadowCubeOcclusion(normal, lightDirection);

	return composeLitColor(lighting, intensity * (1.0f - shadow));
}

// Fixed direction, no distance falloff: the source is infinitely far away.
vec4 computeDirectionalLightColor()
{
	vec3 normal = normalize(fragNormal);
	// Direction TO the light. Main.cpp builds its view from the same vector, so
	// shading and shadow map cannot disagree about where the light is.
	vec3 lightDirection = normalize(lightPos);
	LightingTerms lighting = computePhongTerms(normal, lightDirection);

	float shadow = computeShadowMapOcclusion(normal, lightDirection, dirBiasSlope, dirBiasMin);

	return composeLitColor(lighting, 1.0f - shadow);
}

// Same map and same lookup as the directional light -- only Main.cpp's matrix
// changed, perspective for ortho, because a spot cone IS a perspective frustum.
vec4 computeSpotLightColor()
{
	// A larger inner-cone value produces a narrower bright center.
	float outerCone = 0.90f;
	float innerCone = 0.95f;

	vec3 normal = normalize(fragNormal);
	vec3 lightDirection = normalize(lightPos - fragPos);
	LightingTerms lighting = computePhongTerms(normal, lightDirection);

	// From Main.cpp, not hardcoded to (0,-1,0): that is only correct while the
	// light sits directly above its target, else cone and frustum aim apart.
	float intensity = computeSpotIntensity(lightDirection, spotDirection, outerCone, innerCone);

	float shadow = computeShadowMapOcclusion(normal, lightDirection, spotBiasSlope, spotBiasMin);

	return composeLitColor(lighting, intensity * (1.0f - shadow));
}

void main()
{
	// Switched live from L, not by commenting calls in and out. All three stay
	// compiled so they can be compared on the same frame and geometry.
	if (lightMode == MODE_POINT)
		FragColor = computePointLightColor();
	else if (lightMode == MODE_SPOT)
		FragColor = computeSpotLightColor();
	else
		FragColor = computeDirectionalLightColor();
}
