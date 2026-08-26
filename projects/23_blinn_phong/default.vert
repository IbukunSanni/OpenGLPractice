#version 330 core

// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Normals (not necessarily normalized)
layout (location = 1) in vec3 aNormal;
// Colors
layout (location = 2) in vec3 aColor;
// Texture Coordinates
layout (location = 3) in vec2 aTex;


// Handed to the geometry shader, not straight to the fragment shader. The block
// name and the member names/types/order must match default.geom's `in DATA`
// exactly, or the program fails to link.
out DATA
{
	vec3 Normal;
	vec3 color;
	vec2 texCoord;
	mat4 projection;
} data_out;


// Imports the camera matrix
uniform mat4 camMatrix;
// Imports the transformation matrices
uniform mat4 model;
uniform mat4 translation;
uniform mat4 rotation;
uniform mat4 scale;


void main()
{
	mat4 transform = model * translation * rotation * scale;

	// WORLD space, deliberately not clip space. The geometry shader offsets
	// each triangle along its face normal and applies the projection itself,
	// so projecting here would apply camMatrix twice -- and the face normal it
	// computes from these positions would be a clip-space direction, not a
	// surface direction.
	gl_Position = transform * vec4(aPos, 1.0f);

	// Keep normals perpendicular after model rotation and non-uniform scaling.
	data_out.Normal = normalize(transpose(inverse(mat3(transform))) * aNormal);
	// Assigns the colors from the Vertex Data to "color"
	data_out.color = aColor;
	// glTF UVs are top-left origin with v down; stb loads top-row-first with
	// flip disabled, so GL's t already matches v — no correction needed.
	data_out.texCoord = aTex;
	// The projection the geometry shader will apply after displacing.
	data_out.projection = camMatrix;
}
