#version 330 core

// Per-vertex inputs from the VBO (must match VAO attribute layout indices)
layout (location = 0) in vec3 aPos;    // 3D position in object/model space
layout (location = 1) in vec3 aColor;  // RGB vertex color
layout (location = 2) in vec2 aTex;    // UV texture coordinate
layout (location = 3) in vec3 aNormal; // Surface normal in object space (used for lighting)

// Passed through to the fragment shader for interpolation across the triangle
out vec3 color;
out vec2 texCoord;
out vec3 Normal;
out vec3 crntPos; // World-space position of this vertex; the fragment shader uses it to
                  // compute the light direction (lightPos - crntPos) and the view direction
                  // (camPos - crntPos) needed for specular highlights.

uniform mat4 camMatrix; // Combined projection * view matrix (uploaded by Camera::Matrix)
uniform mat4 model;     // Object-to-world transform for this mesh

void main()
{
	// Full MVP transform: object space → world space → eye space → clip space.
	// OpenGL then performs the perspective divide (/ w) to reach NDC, then maps to the viewport.
	gl_Position = camMatrix * model * vec4(aPos, 1.0);

	// Transform position to world space (not clip space) so the fragment shader can
	// do world-space distance and direction maths for the Phong lighting model.
	crntPos  = vec3(model * vec4(aPos, 1.0));
	Normal   = aNormal;
	color    = aColor;
	texCoord = aTex;
}
