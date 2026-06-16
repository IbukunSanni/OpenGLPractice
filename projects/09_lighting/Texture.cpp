#include "Texture.h"

#include <direct.h>
#include <stdexcept>
#include <string>

using namespace std;

namespace
{
	string get_working_directory()
	{
		char buffer[_MAX_PATH];
		if (_getcwd(buffer, _MAX_PATH) != nullptr)
		{
			return string(buffer);
		}

		return string("unknown");
	}

	unsigned char* load_texture_bytes(const char* image, int& widthImg, int& heightImg, int& numColCh)
	{
		unsigned char* bytes = stbi_load(image, &widthImg, &heightImg, &numColCh, 0);
		if (bytes != nullptr)
		{
			return bytes;
		}

		const char* fallbackPrefixes[] =
		{
			"../",
			"../../",
			"../../../",
			"../../../../"
		};

		for (const char* prefix : fallbackPrefixes)
		{
			string fallbackPath = string(prefix) + image;
			bytes = stbi_load(fallbackPath.c_str(), &widthImg, &heightImg, &numColCh, 0);
			if (bytes != nullptr)
			{
				return bytes;
			}
		}

		throw runtime_error(
			string("Failed to load texture: ") + image +
			" (working directory: " + get_working_directory() + ")"
		);
	}
}

Texture::Texture(const char* image, GLenum texType, GLenum slot, GLenum format, GLenum pixelType)
{
	// Assigns the type of the texture to the texture object
	type = texType;

	// Stores the width, height, and the number of color channels of the image
	int widthImg, heightImg, numColCh;

	// Flips the image so it appears right side up
	stbi_set_flip_vertically_on_load(true);

	// Reads the image from a file and stores it in bytes
	unsigned char* bytes = load_texture_bytes(image, widthImg, heightImg, numColCh);

	// Generates an OpenGL texture object
	glGenTextures(1, &ID);

	// Assigns the texture to a Texture Unit
	glActiveTexture(slot);
	glBindTexture(texType, ID);

	// Configures the type of algorithm
	glTexParameteri(texType, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(texType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Configures the way the texture repeats (if it does at all)
	glTexParameteri(texType, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(texType, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Extra lines in case you choose to use GL_CLAMP_TO_BORDER
	// float flatColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	// float flatColor[] = {1.0f, 1.0f, 1.0f, 1.0f};

	// glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);GLenum format;
	if (numColCh == 4)
		format = GL_RGBA;
	else if (numColCh == 3)
		format = GL_RGB;
	else if (numColCh == 1)
		format = GL_RED;
	else
		throw std::invalid_argument("Automatic Texture type recognition failed");

	glTexImage2D(texType, 0, format, widthImg, heightImg, 0, format, pixelType, bytes);
	// Assigns the image to the OpenGL Texture object
	glTexImage2D(texType, 0, format, widthImg, heightImg, 0, format, pixelType, bytes);
	// Generates MipMaps
	glGenerateMipmap(texType);
	// Deletes the image data as it is already in the OpenGL Texture object
	stbi_image_free(bytes);
	// Unbinds the OpenGL Texture object so that it can't accidentally be modified
	glBindTexture(texType, 0);
}

void Texture::texUnit(Shader& shader, const char* uniform, GLuint unit)
{
	// Gets the location of the uniform
	GLuint texUni = glGetUniformLocation(shader.ID, uniform);
	// Shader needs to be activated before changing the value of a uniform
	shader.Activate();
	// Sets the value of the uniform
	glUniform1i(texUni, unit);
}

void Texture::Bind()
{
	glBindTexture(type, ID);
}

void Texture::Unbind()
{
	glBindTexture(type, 0);
}

void Texture::Delete()
{
	glDeleteTextures(1, &ID);
}
