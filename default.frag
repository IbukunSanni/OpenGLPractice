#version 330 core
out vec4 FragColor;

in vec3 color;
uniform float scale;
void main()
{
   FragColor = vec4((1.0f -color.x) * scale,
					(1.0f -color.y) * scale,
					(1.0f -color.z) * scale,
					1.0f
					);
}