#pragma once

#include <glad/glad.h>
#include <vector>

class EBO
{
public:
	// ID reference of Elements Buffer Object
	GLuint ID;
	// Uploads triangle indices to GL_ELEMENT_ARRAY_BUFFER.
	EBO(const std::vector<GLuint>& indices);

	// Binds the EBO
	void Bind();
	// Unbinds the EBO
	void Unbind();
	// Deletes the EBO
	void Delete();
};
