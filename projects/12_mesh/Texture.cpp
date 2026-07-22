#include "Texture.h"

#include <direct.h>
#include <stdexcept>
#include <string>

namespace
{
	std::string get_working_directory()
	{
		char buffer[_MAX_PATH];
		if (_getcwd(buffer, _MAX_PATH) != nullptr)
		{
			return std::string(buffer);
		}

		return std::string("unknown");
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
			std::string fallbackPath = std::string(prefix) + image;
			bytes = stbi_load(fallbackPath.c_str(), &widthImg, &heightImg, &numColCh, 0);
			if (bytes != nullptr)
			{
				return bytes;
			}
		}

		const char* failureReason = stbi_failure_reason();
		throw std::runtime_error(
			std::string("Failed to load texture: ") + image +
			" (working directory: " + get_working_directory() + ", reason: " +
			(failureReason != nullptr ? failureReason : "unknown") + ")"
		);
	}
}

Texture::Texture(const char* image, const char* texType, GLuint slot, GLenum pixelType)
{
	// type is a material role used to build sampler names in Mesh::Draw.
	type = texType;

	int widthImg, heightImg, numColCh;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* bytes = load_texture_bytes(image, widthImg, heightImg, numColCh);

	glGenTextures(1, &ID);
	// slot is a zero-based unit index, so unit 0 maps to GL_TEXTURE0.
	glActiveTexture(GL_TEXTURE0 + slot);
	unit = slot;
	glBindTexture(GL_TEXTURE_2D, ID);

	// Use mipmaps when shrinking and nearest-neighbor sampling when enlarging.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Match the OpenGL upload format to the number of channels stb_image found.
	GLenum format;
	if (numColCh == 4)
	{
		format = GL_RGBA;
	}
	else if (numColCh == 3)
	{
		format = GL_RGB;
	}
	else if (numColCh == 1)
	{
		format = GL_RED;
	}
	else
	{
		throw std::invalid_argument("Automatic Texture type recognition failed");
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, widthImg, heightImg, 0, format, pixelType, bytes);
	glGenerateMipmap(GL_TEXTURE_2D);

	// The GPU owns its uploaded copy, so the CPU image can now be released.
	stbi_image_free(bytes);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::texUnit(Shader& shader, const char* uniform, GLuint unit)
{
	GLuint texUni = glGetUniformLocation(shader.ID, uniform);
	shader.Activate();
	glUniform1i(texUni, unit);
}

void Texture::Bind()
{
	// Restore this texture's unit before binding; another texture may have changed it.
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, ID);
}

void Texture::Unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Delete()
{
	glDeleteTextures(1, &ID);
}
