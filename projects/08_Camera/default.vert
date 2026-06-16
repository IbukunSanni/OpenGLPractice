#version 330 core

// Per-vertex inputs from the VBO (must match VAO attribute layout indices)
layout (location = 0) in vec3 aPos;    // 3D position in object/model space
layout (location = 1) in vec3 aColor;  // RGB vertex color
layout (location = 2) in vec2 aTex;    // UV texture coordinate

// Passed through to the fragment shader for interpolation across the triangle
out vec3 color;
out vec2 texCoord;

uniform mat4 camMatrix;

void main()
{
	// Full MVP transform: object space → world → eye → clip space
	// OpenGL then performs the perspective divide (/ w) to get NDC, then maps to the viewport
	gl_Position = camMatrix * vec4(aPos, 1.0);

	color    = aColor;
	texCoord = aTex;
}
