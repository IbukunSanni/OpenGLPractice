#version 330 core
// Locations match the Vertex field offsets configured in Mesh.cpp.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTex;

// Interpolated across each triangle before reaching the fragment shader.
out vec3 crntPos;
out vec3 Normal;
out vec2 texCoord;

uniform mat4 camMatrix; // Projection * view
uniform mat4 model;     // Object space -> world space

void main()
{
	// Lighting uses world-space positions and normals.
	crntPos = vec3(model * vec4(aPos, 1.0));
	Normal = mat3(transpose(inverse(model))) * aNormal;
	texCoord = aTex;

	gl_Position = camMatrix * model * vec4(aPos, 1.0);
}
