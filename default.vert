#version 330 core

// Position attribute from vertex data (location 0)
layout (location = 0) in vec3 aPos;

// Color attribute from vertex data (location 1)
layout (location = 1) in vec3 aColor;

// Output color passed to the fragment shader
out vec3 color;

// Scale uniform - controls how much the geometry is scaled
uniform float scale;

void main()
{
	// Outputs the positions/coordinates of all vertices
	// Applies scale uniformly to x, y, and z axes
	gl_Position = vec4(aPos.x + aPos.x * scale, aPos.y + aPos.y * scale, aPos.z + aPos.z * scale, 1.0);
	// Assigns the colors from the Vertex Data to "color"
	color = aColor;
}
