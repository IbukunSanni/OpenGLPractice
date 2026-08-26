#pragma once

#include<glad/glad.h>
#include<glm/glm.hpp>
#include<vector>

// Structure to standardize the vertices used in the meshes
struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texUV;
};

class VBO
{
public:
	// Reference ID of the Vertex Buffer Object
	GLuint ID;
	// Uploads an interleaved Vertex array to GL_ARRAY_BUFFER.
	VBO(const std::vector<Vertex>& vertices);
	VBO(std::vector<glm::mat4>& mat4s);

	// Binds the VBO
	void Bind();
	// Unbinds the VBO
	void Unbind();
	// Deletes the VBO
	void Delete();
};
