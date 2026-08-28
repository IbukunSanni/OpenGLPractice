#version 330 core

// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Normals (not necessarily normalized)
layout (location = 1) in vec3 aNormal;
// Colors
layout (location = 2) in vec3 aColor;
// Texture Coordinates
layout (location = 3) in vec2 aTex;

out vec3 fragPos;
out vec3 fragNormal;
out vec2 texCoord;

uniform mat4 camMatrix;
uniform mat4 model;
uniform mat4 translation;
uniform mat4 rotation;
uniform mat4 scale;

void main()
{
	fragPos = vec3(model * translation * rotation * scale * vec4(aPos, 1.0f));
	fragNormal = aNormal;
	// Passed through UNCHANGED. This read mat2(0.0, -1.0, 1.0, 0.0) * aTex, a
	// 90-degree UV rotation that belongs with flip=true (as in 13_model_loading).
	// 25 brought it back without the flip, applying both corrections where neither
	// was needed. flip=false plus identity is the coherent pair.
	texCoord = aTex;

	gl_Position = camMatrix * vec4(fragPos, 1.0);
}
