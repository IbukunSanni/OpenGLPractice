#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 camMatrix;
uniform vec4 lightColor;

out vec4 LightColor;

void main()
{
	gl_Position = camMatrix * model * vec4(aPos, 1.0f);
	LightColor = lightColor;
}