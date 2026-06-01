#version 330 core

// Position attribute from vertex data (location 0)
layout (location = 0) in vec3 aPos;

// Color attribute from vertex data (location 1)
layout (location = 1) in vec3 aColor;

// Texture coordinate attribute from vertex data (location 2)
// These 2D coordinates (u, v) tell the fragment shader which part of an image to sample
layout (location = 2) in vec2 aTex;

// Pass the color to the fragment shader so it can determine each pixel's color
out vec3 color;

// Pass the texture coordinate to the fragment shader for image mapping
out vec2 texCoord;

// Scale uniform - controls how much the geometry is scaled
uniform float scale;

void main()
{
	// Outputs the positions/coordinates of all vertices
	// Applies scale uniformly to x, y, and z axes
	gl_Position = vec4(aPos * scale, 1.0);
	// Forward the per-vertex color and texture coordinate to the fragment shader
	color = aColor;
	texCoord = aTex;
}
