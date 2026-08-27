#version 330 core

// The light-source cube only needs position; no normals or UVs are required
// because it renders as a plain solid colour with no lighting calculation.
layout (location = 0) in vec3 aPos;

uniform mat4 model;      // Translates the cube to the light's world-space location
uniform mat4 camMatrix;  // Combined projection * view matrix (same as the scene camera)
uniform vec4 lightColor; // The colour of this light; passed through to the fragment shader

out vec4 LightColor;

void main()
{
	// Transform the cube vertex into clip space using the same camera matrix as the scene.
	gl_Position = camMatrix * model * vec4(aPos, 1.0f);

	// Forward the light colour unchanged so the cube visually represents the light's tint.
	LightColor = lightColor;
}
