#include "Skybox.h"

#include <iostream>
#include <stdexcept>

#include <glm/gtc/type_ptr.hpp>
#include <stb/stb_image.h>

namespace {

	float skyboxVertices[] =
	{
		//   Coordinates
		-1.0f, -1.0f,  1.0f,//        7--------6
		 1.0f, -1.0f,  1.0f,//       /|       /|
		 1.0f, -1.0f, -1.0f,//      4--------5 |
		-1.0f, -1.0f, -1.0f,//      | |      | |
		-1.0f,  1.0f,  1.0f,//      | 3------|-2
		 1.0f,  1.0f,  1.0f,//      |/       |/
		 1.0f,  1.0f, -1.0f,//      0--------1
		-1.0f,  1.0f, -1.0f
	};

	unsigned int skyboxIndices[] =
	{
		1, 2, 6,  6, 5, 1,   // Right
		0, 4, 7,  7, 3, 0,   // Left
		4, 5, 6,  6, 7, 4,   // Top
		0, 3, 2,  2, 1, 0,   // Bottom
		0, 1, 5,  5, 4, 0,   // Back
		3, 7, 6,  6, 2, 3    // Front
	};

}

Skybox::Skybox(const std::array<std::string, 6>& faces)
{
	// --- position-only cube -------------------------------------------------
	// No texture coordinates are stored: skybox.vert derives the cubemap lookup
	// direction straight from the vertex position.
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), &skyboxIndices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// The VAO is unbound before the element buffer so it keeps that binding.
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// --- cubemap ------------------------------------------------------------
	// A cubemap is ONE texture object holding six faces, not six textures.
	glGenTextures(1, &cubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// CLAMP_TO_EDGE on all three axes is what keeps the face joins seamless.
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// stb's vertical-flip setting is global rather than per-load, so it is set
	// once here instead of inside the loop.
	stbi_set_flip_vertically_on_load(false);

	// GL_TEXTURE_CUBE_MAP_POSITIVE_X..NEGATIVE_Z are six consecutive enum
	// values, so POSITIVE_X + i walks them in the order `faces` was declared:
	// right, left, top, bottom, front, back. That order is not optional.
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		int faceWidth, faceHeight, faceChannels;
		unsigned char* data = stbi_load(faces[i].c_str(), &faceWidth, &faceHeight, &faceChannels, 0);
		if (!data)
		{
			std::cout << "Failed to load texture: " << faces[i] << std::endl;
			stbi_image_free(data);
			continue;
		}

		// GL_RGB assumes 3 bytes/pixel; a 4-channel source read with that
		// stride shears diagonally, because GL then miscomputes each row's
		// offset. The format declared here must match what stb actually read.
		GLenum sourceFormat;
		switch (faceChannels)
		{
		case 3:
			sourceFormat = GL_RGB;
			break;
		case 4:
			sourceFormat = GL_RGBA;
			break;
		default:
			stbi_image_free(data);
			throw std::invalid_argument(
				"Unsupported number of skybox texture color channels: " +
				std::to_string(faceChannels) + " (" + faces[i] + ")");
		}

		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA,
			faceWidth, faceHeight, 0, sourceFormat, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}
}

Skybox::~Skybox()
{
	glDeleteTextures(1, &cubemap);
	glDeleteBuffers(1, &ebo);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

void Skybox::Draw(Shader& shader, const Camera& camera, const glm::mat4& projection) const
{
	// The cube is written at depth 1.0, which loses to any real geometry under
	// GL_LESS -- but it must still pass where nothing was drawn, hence LEQUAL.
	glDepthFunc(GL_LEQUAL);
	shader.Activate();

	// Truncating to mat3 and back strips the view matrix's translation column,
	// so moving the camera cannot slide the sky. Only looking around rotates
	// it, which is what keeps it reading as infinitely far away.
	const glm::mat4 view = glm::mat4(glm::mat3(glm::lookAt(
		camera.Position, camera.Position + camera.Orientation, camera.Up)));

	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// The camera sits INSIDE this cube looking at its inner faces, so the
	// winding that reads as front from outside reads as back from in here --
	// the cull state tuned for the scene would discard the whole thing.
	glDisable(GL_CULL_FACE);
	glBindVertexArray(vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	glEnable(GL_CULL_FACE);

	glDepthFunc(GL_LESS);
}
