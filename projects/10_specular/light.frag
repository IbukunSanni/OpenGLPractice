#version 330 core

// The light-cube fragment shader is intentionally trivial:
// it outputs the raw light colour with no lighting calculation, so the small
// marker cube always appears fully lit regardless of scene lighting.
out vec4 FragColor;

in vec4 LightColor; // Colour forwarded from the vertex shader

void main()
{
	// No ambient / diffuse / specular — the light source just glows its own colour.
	FragColor = LightColor;
}
