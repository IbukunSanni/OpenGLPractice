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

	// Tries 'image' as given, then retries under a few '../' prefixes, since
	// asset paths are written relative to the intended project root rather
	// than wherever the executable happens to be launched from.
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

    // glTF UV coordinates use the same top-left image convention as stb_image.
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

    // Determine the format of the source image data
    GLenum sourceFormat;

    switch (numColCh)
    {
    case 1:
        sourceFormat = GL_RED;
        break;

    case 3:
        sourceFormat = GL_RGB;
        break;

    case 4:
        sourceFormat = GL_RGBA;
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
        GL_RGBA,
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
