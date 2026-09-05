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

	// Tries 'image' as given, then retries under a few '../' prefixes, since asset
	// paths are relative to the intended project root, not the launch directory.
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

	// The GL half of both constructors: create the object, bind it on its unit,
	// set filtering and wrapping, upload level 0 and build the mip chain.
	// Returns the new name and leaves nothing bound to GL_TEXTURE_2D.
	GLuint upload_texture_2d(GLuint slot, GLint minFilter, GLint magFilter,
		GLint internalFormat, int width, int height, GLenum sourceFormat,
		const void* pixels)
	{
		GLuint id = 0;
		glGenTextures(1, &id);

		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
			sourceFormat, GL_UNSIGNED_BYTE, pixels);
		glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);
		return id;
	}
}

// Loads an image from disk, uploads it to a new GL_TEXTURE_2D, and
// generates mipmaps. The CPU-side pixel buffer is freed before returning.
Texture::Texture(const char* image, const char* texType, GLuint slot)
{
	type = texType;
	unit = slot;

	int widthImg, heightImg, numColCh;

	// The ONLY thing deciding UV orientation now -- default.vert no longer
	// rotates on top of it. glTF puts UV (0,0) at the image's top-left and stb
	// hands over the top row first, so not flipping lines the two up exactly.
	stbi_set_flip_vertically_on_load(false);
	unsigned char* bytes =
		load_texture_bytes(image, widthImg, heightImg, numColCh);

	// Only COLOUR maps are sRGB-encoded. Specular, roughness and normal maps hold
	// linear measurements, not colours -- decoding those through the sRGB curve
	// darkens the values and quietly corrupts the lighting that reads them.
	const bool isColorMap = (type == "diffuse");

	GLenum sourceFormat;
	GLint internalFormat;

	if (type == "normal")  // prevents sRGB from deforming normals
	{
		// Checked before the channel count, so a normal map cannot be decoded
		// even if it is registered under the wrong type. The SOURCE format still
		// follows the file: this set's nor_gl map is 3-channel, and claiming
		// RGBA over 3-channel rows shears the image diagonally.
		sourceFormat   = (numColCh == 4) ? GL_RGBA : GL_RGB;
		internalFormat = (numColCh == 4) ? GL_RGBA8 : GL_RGB8;
	}
	else if (numColCh == 4)
	{
		sourceFormat   = GL_RGBA;
		internalFormat = isColorMap ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	}
	else if (numColCh == 3)
	{
		sourceFormat   = GL_RGB;
		internalFormat = isColorMap ? GL_SRGB8 : GL_RGB8;
	}
	else if (numColCh == 2)
	{
		sourceFormat   = GL_RG;
		internalFormat = GL_RG8;
	}
	else if (numColCh == 1)
	{
		// Roughness and specular masks. Linear, and one- and two-channel images
		// cannot be sRGB at all -- the format does not exist.
		sourceFormat   = GL_RED;
		internalFormat = GL_R8;
	}
	else
	{
		stbi_image_free(bytes);
		throw std::invalid_argument("Automatic Texture type recognition failed");
	}

	// Formats are settled BEFORE the texture object exists, so the throw above
	// cannot leak a generated name the destructor never sees.
	ID = upload_texture_2d(slot, GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST,
		internalFormat, widthImg, heightImg, sourceFormat, bytes);

	// CPU image data is no longer needed
	stbi_image_free(bytes);
}

Texture::Texture(const unsigned char* pixels, int width, int height,
	const char* texType, GLuint slot, bool smooth)
{
	type = texType;
	unit = slot;

	// NEAREST for the checker: the point of it is to be unmistakable, and
	// filtering a 16x16 pattern into grey mush defeats that. Flat colours pass
	// smooth = true, where it makes no difference either way.
	const GLint filter = smooth ? GL_LINEAR : GL_NEAREST;

	ID = upload_texture_2d(slot, filter, filter,
		GL_RGBA, width, height, GL_RGBA, pixels);
}

void Texture::texUnit(Shader& shader, const char* uniform, GLuint textureUnit)
{
	GLuint texUni = glGetUniformLocation(shader.ID, uniform);
	shader.Activate();
	glUniform1i(texUni, textureUnit);
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
