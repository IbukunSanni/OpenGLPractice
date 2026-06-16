#version 330 core

out vec4 FragColor;

in vec4 LightColor;

void main()
{
	FragColor = LightColor;
}