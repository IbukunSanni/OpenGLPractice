#pragma once

#include<glad/glad.h>
#include<stb/stb_image.h>
#include<string>

#include"shaderClass.h"

class Texture
{
public:
	GLuint ID;
	std::string type;
	GLuint unit;
	Texture(const char* image, const char* texType, GLuint slot);

	// Builds a texture from RGBA pixels held in memory instead of a file.
	// Keeps generated stand-ins -- a flat material colour, or the missing-texture
	// checker -- shaped exactly like a real texture, so the shader never needs to
	// know which kind it is sampling.
	Texture(const unsigned char* pixels, int width, int height,
		const char* texType, GLuint slot, bool smooth = true);

	// Assigns a texture unit to a texture
	void texUnit(Shader& shader, const char* uniform, GLuint textureUnit);
	// Binds a texture
	void Bind();
	// Unbinds a texture
	void Unbind();
	// Deletes a texture
	void Delete();
};
