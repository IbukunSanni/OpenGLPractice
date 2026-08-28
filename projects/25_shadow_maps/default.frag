#version 330 core

out vec4 FragColor;

in vec2 texCoord;
in vec3 Normal;
in vec3 crntPos;
// Imports the fragment position of the light
in vec4 fragPosLight;

uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform sampler2D shadowMap;
uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec3 camPos;
// Switches the specular model. false falls back to the original Phong.
uniform bool useBlinnPhong;

// Flat base light applied everywhere, standing in for bounced light the
// renderer never computes. Lowering it darkens everything the point light does
// not reach, which is what makes the falloff and the specular highlight
// readable instead of washed out. Was 0.20.
const float sceneAmbient = 0.05f;

// Brightness of the light cube, applied on top of distance falloff.
//
// Kept separate from linearFactor/quadraticFactor on purpose: those two come
// from 11_light and define the SHAPE of the falloff, while this sets how bright
// the source is. Turning this up brightens the scene without flattening the
// gradient across the plane, which is what lowering the falloff would do.
const float lightStrength = 6.0f;

// Return ambient, diffuse, and specular strengths in one vector.
//
// Two specular models, chosen by useBlinnPhong:
//
//   Phong        reflect the light about the normal, then measure the angle
//                between that reflection and the viewer.
//   Blinn-Phong  build the halfway vector between the light and view
//                directions, then measure its angle to the normal. No
//                reflect() call, and it keeps a highlight at grazing angles
//                where Phong's reflection vector passes 90 degrees from the
//                viewer and clamps hard to zero.
//
// The halfway angle is roughly half the reflection angle, so at the SAME
// exponent Blinn produces a noticeably wider highlight. Raise shininess by
// about 2-4x if you want the two to match in size rather than differ.
vec3 computePhongTerms(vec3 normal, vec3 lightDirection, float ambientStrength, float specularStrength, float shininess)
{
	float diffuse = max(dot(normal, lightDirection), 0.0f);

	// Specular only where the surface actually faces the light. Without this
	// guard a highlight can appear on geometry turned away from the source:
	// with diffuse already clamped to 0, the view/reflection (or normal/halfway)
	// dot can still come out positive and paint a spot that has no light
	// reaching it.
	float specular = 0.0f;
	if (diffuse > 0.0f)
	{
		vec3 viewDirection = normalize(camPos - crntPos);
		float specAmount;

		if (useBlinnPhong)
		{
			vec3 halfwayVec = normalize(viewDirection + lightDirection);
			specAmount = pow(max(dot(normal, halfwayVec), 0.0f), shininess);
		}
		else
		{
			vec3 reflectionDirection = reflect(-lightDirection, normal);
			specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), shininess);
		}

		specular = specAmount * specularStrength;
	}

	return vec3(ambientStrength, diffuse, specular);
}

// Distance falloff for a point light: 1 / (quadratic*d^2 + linear*d + 1).
float computePointAttenuation(float distanceToLight, float linearFactor, float quadraticFactor)
{
	return 1.0f / (quadraticFactor * distanceToLight * distanceToLight + linearFactor * distanceToLight + 1.0f);
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

// Local point light: brightness falls off with distance from lightPos.
vec4 computePointLightColor()
{
	// Light vector from fragment to point light.
	vec3 lightVec = lightPos - crntPos;
	float dist = length(lightVec);
	float linearFactor = 12.0f;
	float quadraticFactor = 12.0f;
	float intensity = lightStrength * computePointAttenuation(dist, linearFactor, quadraticFactor);

	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(lightVec);
	vec3 terms = computePhongTerms(normal, lightDirection, sceneAmbient, 0.50f, 16.0f);

	return composeLitColor(terms, intensity);
}

// Global directional light: fixed direction, no distance falloff.
// lightPos is ignored, since the source is treated as infinitely far away.
vec4 computeDirectionalLightColor()
{
	vec3 normal = normalize(Normal);
	// Direction TO the light. A directional light has no position, so lightPos
	// is read purely as a direction here -- and Main.cpp builds lightView from
	// that same vector (lookAt from 20.0f * lightPos). One source, so the shading
	// and the shadow map cannot disagree about where the light is. A second
	// hardcoded vector here previously lit from (1,1,0) while the map was
	// rendered from (1,1,1), which detaches shadows from the dark sides they
	// are supposed to extend.
	vec3 lightDirection = normalize(lightPos);
	vec3 terms = computePhongTerms(normal, lightDirection, sceneAmbient, 0.50f, 16.0f);

	
	float shadow = 0.0f;
	vec3 lightCoords = fragPosLight.xyz / fragPosLight.w;
	if(lightCoords.z <= 1.0f){

		lightCoords = (lightCoords + 1.0f) / 2.0f;
		float currentDepth = lightCoords.z;
		float bias = max(0.025f * (1.0f - dot(normal, lightDirection)), 0.0005f);
		

		int sampleRadius = 2;
		vec2 pixelSize = 1.0 / textureSize(shadowMap, 0);

		for(int y = -sampleRadius; y <= sampleRadius; y++){
			for(int x =-sampleRadius; x <= sampleRadius; x++ ){
				float closestDepth = texture(shadowMap, lightCoords.xy + vec2(x,y) * pixelSize).r;
				if (currentDepth > closestDepth + bias){
					shadow += 1.0f;
					}
			}
		}
		// Get average shadow
		shadow /= pow((sampleRadius * 2 + 1), 2);
	}

	float intensity = 1.0f - shadow ;

	return composeLitColor(terms, intensity);
}

// Spotlight: cone-shaped light with a soft edge.
vec4 computeSpotLightColor()
{
	// A larger inner-cone value produces a narrower bright center.
	float outerCone = 0.90f;
	float innerCone = 0.95f;

	vec3 normal = normalize(Normal);
	vec3 lightDirection = normalize(lightPos - crntPos);
	vec3 terms = computePhongTerms(normal, lightDirection, sceneAmbient, 0.50f, 16.0f);
	float intensity = computeSpotIntensity(lightDirection, vec3(0.0f, -1.0f, 0.0f), outerCone, innerCone);

	return composeLitColor(terms, intensity);
}

void main()
{
	// Point light, so the cube's position actually governs the shading. The
	// directional variant ignores lightPos entirely, which would leave the cube
	// looking like a light source without behaving as one.
	FragColor = computeDirectionalLightColor();
}
