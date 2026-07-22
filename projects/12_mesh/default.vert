#version 330 core
// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Normals (not necessarily normalized)
layout (location = 1) in vec3 aNormal;
// Colors
layout (location = 2) in vec3 aColor;
// Texture Coordinates
layout (location = 3) in vec2 aTex;


// Outputs the current position for the Fragment Shader
out vec3 crntPos;
// Outputs the normal for the Fragment Shader
out vec3 Normal;
// Outputs the color for the Fragment Shader
out vec3 color;
// Outputs the texture coordinates to the Fragment Shader
out vec2 texCoord;

uniform mat4 camMatrix; // Combined projection * view matrix (uploaded by Camera::Matrix)
uniform mat4 model;     // Object-to-world transform for this mesh

void main()
{
	
	// Transform position to world space (not clip space) so the fragment shader can
	// do world-space distance and direction maths for the Phong lighting model.
	crntPos  = vec3(model * vec4(aPos, 1.0));
	Normal   = aNormal;
	color    = aColor;
	texCoord = aTex;

	gl_Position = camMatrix * model * vec4(aPos, 1.0);
}
