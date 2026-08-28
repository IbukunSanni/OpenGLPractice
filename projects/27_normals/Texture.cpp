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
}

// Loads an image from disk, uploads it to a new GL_TEXTURE_2D, and
// generates mipmaps. The CPU-side pixel buffer is freed before returning.
Texture::Texture(const char* image, const char* texType, GLuint slot)
{
    type = texType;

    int widthImg, heightImg, numColCh;

    // The ONLY thing deciding UV orientation now -- default.vert no longer
    // rotates on top of it. glTF puts UV (0,0) at the image's top-left and stb
    // hands over the top row first, so not flipping lines the two up exactly.
    stbi_set_flip_vertically_on_load(false);
    unsigned char* bytes =
        load_texture_bytes(image, widthImg, heightImg, numColCh);

    glGenTextures(1, &ID);

    glActiveTexture(GL_TEXTURE0 + slot);
    unit = slot;

    glBindTexture(GL_TEXTURE_2D, ID);

    // Texture filtering
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST_MIPMAP_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST
    );

    // Texture wrapping
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_REPEAT
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_REPEAT
    );

    // Only COLOUR maps are sRGB-encoded. Specular, roughness and normal maps hold
    // linear measurements, not colours -- decoding those through the sRGB curve
    // darkens the values and quietly corrupts the lighting that reads them.
    // One- and two-channel images cannot be sRGB at all; the format does not exist.
    const bool isColorMap = (type == "diffuse");

    GLenum sourceFormat;
    GLint internalFormat;

    switch (numColCh)
    {
    case 1:
        sourceFormat = GL_RED;
        internalFormat = GL_R8;
        break;
    case 2:
        sourceFormat = GL_RG;
        internalFormat = GL_RG8;
        break;
    case 3:
        sourceFormat = GL_RGB;
        internalFormat = isColorMap ? GL_SRGB8 : GL_RGB8;
        break;
    case 4:
        sourceFormat = GL_RGBA;
        internalFormat = isColorMap ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        break;
    default:
        stbi_image_free(bytes);
        throw std::invalid_argument(
            "Unsupported number of texture color channels"
        );
    }

    // Upload the image data to the GPU
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        widthImg,
        heightImg,
        0,
        sourceFormat,
        GL_UNSIGNED_BYTE,
        bytes
    );

    // Generate smaller versions of the texture
    glGenerateMipmap(GL_TEXTURE_2D);

    // CPU image data is no longer needed
    stbi_image_free(bytes);

    // Unbind the texture
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


Texture::Texture(const unsigned char* pixels, int width, int height,
	const char* texType, GLuint slot, bool smooth)
{
	type = texType;
	unit = slot;

	glGenTextures(1, &ID);
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, ID);

	// NEAREST for the checker: the point of it is to be unmistakable, and
	// filtering a 16x16 pattern into grey mush defeats that. Flat colours pass
	// smooth = true, where it makes no difference either way.
	const GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
}
