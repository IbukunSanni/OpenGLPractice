#pragma once

#include <string>
#include <vector>

#include <glm/gtc/quaternion.hpp>  // glm::quat, glm::mat4_cast

#include "VAO.h"
#include "EBO.h"
#include "Camera.h"
#include "Texture.h"

class Mesh
{
public: 
	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;
	std::vector<Texture> textures;

	// The VAO stores this mesh's vertex layout and element-buffer binding.
	VAO VAO;

	Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices, const std::vector<Texture>& textures);

	// Binds this mesh's textures and submits its indexed triangles.
	// 'matrix' is the accumulated node transform supplied by Model. The
	// translation/rotation/scale components default to identity so a standalone
	// Mesh (one not owned by a Model) can still be drawn with just two arguments.
	void Draw(
		Shader& shader,
		Camera& camera,
		glm::mat4 matrix = glm::mat4(1.0f),
		glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f),
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f)
	);
};
