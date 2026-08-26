#pragma once
#include<glad/glad.h>
#include<string>
#include<fstream>
#include<sstream>
#include<iostream>
#include<cerrno>

std::string get_file_contents(const char* filename);

class Shader
{
public:
	// Reference ID of the Shader Program
	GLuint ID;
	// Constructor that build the Shader Program from 3 different shaders
	// Builds the program from a vertex and fragment shader, plus an OPTIONAL
	// geometry shader. Omit it (or pass nullptr) to build exactly as before.
	Shader(const char* vertexFile, const char* fragmentFile, const char* geometryFile = nullptr);

	// Activates the Shader Program
	void Activate();
	// Deletes the Shader Program
	void Delete();
private:
	// Checks if the different Shaders have compiled properly
	void compileErrors(unsigned int shader, const char* type);
};


