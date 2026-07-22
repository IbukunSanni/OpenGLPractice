#pragma once

#include <string>
#include <vector>

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
	void Draw(Shader& shader, Camera& camera);
};
