#version 330 core

// Intentionally trivial: outputs the raw light colour with no lighting
// calculation, so the marker cube always appears fully lit.
out vec4 FragColor;

in vec4 LightColor; // Colour forwarded from the vertex shader

void main()
{
	// No ambient / diffuse / specular — the light source just glows its own colour.
	FragColor = LightColor;
}
