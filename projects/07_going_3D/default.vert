#version 330 core

// Per-vertex inputs from the VBO (must match VAO attribute layout indices)
layout (location = 0) in vec3 aPos;    // 3D position in object/model space
layout (location = 1) in vec3 aColor;  // RGB vertex color
layout (location = 2) in vec2 aTex;    // UV texture coordinate

// Passed through to the fragment shader for interpolation across the triangle
out vec3 color;
out vec2 texCoord;

// Unused scale uniform kept for compatibility with the CPU-side uniform upload
uniform float scale;

// MVP matrices uploaded each frame from main.cpp
// Model  : transforms vertices from object space → world space (rotation, translation, scale)
// View   : transforms from world space → camera/eye space (camera position & orientation)
// Proj   : transforms from eye space → clip space (perspective divide, FOV, near/far planes)
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()
{
	// Full MVP transform: object space → world → eye → clip space
	// OpenGL then performs the perspective divide (/ w) to get NDC, then maps to the viewport
	gl_Position = proj * view * model * vec4(aPos, 1.0);

	color    = aColor;
	texCoord = aTex;
}
