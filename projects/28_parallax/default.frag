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
// Height map, 1 = surface, 0 = deepest. Read as (1 - r) below so it counts
// DEPTH downward from the surface, which is the direction the ray marches.
uniform sampler2D displacement0;
uniform vec4 lightColor;
// Switches the specular model. false falls back to the original Phong.
uniform bool useBlinnPhong;
// false falls back to the plane's own flat normal, so the map can be compared
// against the geometry it is standing in for.
uniform bool useNormalMap;
// false samples every map at the raw texCoord, i.e. no parallax at all.
uniform bool useParallax;

// Stands in for bounced light. Lowering it darkens what the light misses, which
// is what makes the falloff and highlight readable.
const float sceneAmbient = 0.05f;

// Brightness, separate from the falloff constants which set its shape.
const float lightStrength = 1.5f;
const float attenuationLinear = 0.09f;
const float attenuationQuadratic = 0.25f;

const float materialSpecularStrength = 0.50f;
const float materialShininess = 16.0f;

// How deep the height map reads, in UV units. The single knob worth tuning:
// too high and the silhouette tears, too low and the effect vanishes.
const float parallaxHeightScale = 0.05f;
// Steps along the view ray. Fewer are needed head-on, where the ray barely
// travels sideways; grazing angles need the most, so the count follows the angle.
const float parallaxMinLayers = 8.0f;
const float parallaxMaxLayers = 64.0f;

struct LightingTerms
{
	float ambient;
	float diffuse;
	float specular;
};

// Phong reflects about the normal and measures to the viewer; Blinn uses the
// halfway vector, keeping a highlight at grazing angles where Phong clamps to 0.
// Blinn's angle is ~half Phong's, so it looks wider at the same exponent.
LightingTerms computePhongTerms(vec3 normal, vec3 lightDirection, vec3 viewDirection)
{
	LightingTerms lighting;
	lighting.ambient = sceneAmbient;
	lighting.diffuse = max(dot(normal, lightDirection), 0.0f);
	lighting.specular = 0.0f;

	// Only where the surface faces the light: with diffuse clamped to 0 the
	// view/reflection dot can still go positive and paint an unlit spot.
	if (lighting.diffuse > 0.0f)
	{
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
vec4 composeLitColor(LightingTerms lighting, float lightIntensity, vec2 uv)
{
	// uv, not texCoord: parallax moves the sample point, and the colour and
	// specular have to move with the normal or they slide apart.
	vec4 diffuseSample = texture(diffuse0, uv);
	float specularSample = texture(specular0, uv).r;

	return (diffuseSample * (lighting.diffuse * lightIntensity + lighting.ambient)
		+ specularSample * lighting.specular * lightIntensity) * lightColor;
}

// Walks the view ray through the height field and returns the UV where it first
// goes under the surface. A normal map only reshapes the LIGHTING; this moves the
// texture lookup itself, so features shift as the camera moves and occlude each
// other -- the part a normal map cannot fake.
//
// Everything is in tangent space, which is what makes it tractable: the surface
// is the z = 0 plane, so depth is one coordinate and the ray is a straight line.
vec2 computeParallaxUV(vec3 viewDirection)
{
	// Head-on, the ray barely moves sideways and few steps suffice. At grazing
	// angles it crosses many texels, so the count scales with the view angle.
	float numLayers = mix(parallaxMaxLayers, parallaxMinLayers,
		abs(dot(vec3(0.0f, 0.0f, 1.0f), viewDirection)));
	float layerDepth = 1.0f / numLayers;
	float currentLayerDepth = 0.0f;

	// Total UV distance the ray covers over the full height. Dividing by z is
	// what makes the shift grow at grazing angles; drop it for a tamer, less
	// accurate result that does not stretch near the silhouette.
	vec2 rayUV = viewDirection.xy / viewDirection.z * parallaxHeightScale;
	vec2 deltaUV = rayUV / numLayers;

	vec2 uv = texCoord;
	float currentDepth = 1.0f - texture(displacement0, uv).r;

	// March until the ray is deeper than the surface at this UV. Bounded: the
	// loop counter climbs by layerDepth each step and the map never exceeds 1.
	while (currentLayerDepth < currentDepth)
	{
		uv -= deltaUV;
		currentDepth = 1.0f - texture(displacement0, uv).r;
		currentLayerDepth += layerDepth;
	}

	// The march overshoots by up to one layer. Interpolate between the last step
	// outside the surface and the first one inside it -- this is the "occlusion"
	// half of the name, and without it the result stairsteps visibly.
	vec2 previousUV = uv + deltaUV;
	float afterDepth = currentDepth - currentLayerDepth;
	float beforeDepth = 1.0f - texture(displacement0, previousUV).r - currentLayerDepth + layerDepth;
	float weight = afterDepth / (afterDepth - beforeDepth);

	return previousUV * weight + uv * (1.0f - weight);
}

// Single point light, unshadowed. A lone flat plane has nothing to occlude
// itself with, so the shadow maps went out with the rest of the old scene.
vec4 computePointLightColor()
{
	vec3 viewDirection = normalize(camPosTangent - fragPosTangent);

	// Resolved once, then used by every map below. Sampling some maps at the
	// displaced UV and others at texCoord is the classic way to get a surface
	// whose colour and lighting disagree about where the bumps are.
	vec2 uv = useParallax ? computeParallaxUV(viewDirection) : texCoord;

	// The march can walk off the tile. Wrapping would repeat the pattern into
	// the gap, which reads as geometry that is not there, so drop the fragment.
	if (useParallax && (uv.x > 1.0f || uv.y > 1.0f || uv.x < 0.0f || uv.y < 0.0f))
		discard;

	vec3 toLight = lightPosTangent - fragPosTangent;
	float distanceToLight = length(toLight);
	float intensity = lightStrength * computePointAttenuation(distanceToLight);

	// On: a per-fragment normal decoded from the map, [0,1] texel back to a
	// [-1,1] vector. Off: (0,0,1), which IS the unperturbed surface normal in
	// tangent space -- no varying needed for it.
	vec3 normal = useNormalMap
		? normalize(texture(normal0, uv).xyz * 2.0f - 1.0f)
		: vec3(0.0f, 0.0f, 1.0f);
	vec3 lightDirection = normalize(toLight);
	LightingTerms lighting = computePhongTerms(normal, lightDirection, viewDirection);

	return composeLitColor(lighting, intensity, uv);
}

void main()
{
	FragColor = computePointLightColor();
}
