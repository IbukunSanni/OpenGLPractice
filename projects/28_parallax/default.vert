#version 330 core

// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Normals (not necessarily normalized)
layout (location = 1) in vec3 aNormal;
// Colors
layout (location = 2) in vec3 aColor;
// Texture Coordinates
layout (location = 3) in vec2 aTex;

// gl_Position leaves here in WORLD space, not clip space. The geometry stage
// needs world-space edges to derive a tangent, so the projection is passed
// through in the block below and applied there instead.
out DATA
{
	vec3 normal;
	vec2 texCoord;
	mat4 projection;
	mat4 model;
	vec3 lightPos;
	vec3 camPos;
} data_out;

uniform mat4 camMatrix;
uniform mat4 model;
uniform mat4 translation;
uniform mat4 rotation;
uniform mat4 scale;
uniform vec3 lightPos;
uniform vec3 camPos;

void main()
{
	mat4 worldMatrix = model * translation * rotation * scale;

	gl_Position = worldMatrix * vec4(aPos, 1.0f);

	data_out.normal = aNormal;
	// Passed through UNCHANGED. This read mat2(0.0, -1.0, 1.0, 0.0) * aTex, a
	// 90-degree UV rotation that belongs with flip=true (as in 13_model_loading).
	// 25 brought it back without the flip, applying both corrections where neither
	// was needed. flip=false plus identity is the coherent pair.
	data_out.texCoord = aTex;
	data_out.projection = camMatrix;
	data_out.model = worldMatrix;
	data_out.lightPos = lightPos;
	data_out.camPos = camPos;
}
