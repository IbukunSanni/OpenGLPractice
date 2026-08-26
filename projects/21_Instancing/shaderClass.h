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

	// Releases the program. Nothing else has to remember to do it, which is why
	// there is no separate guard object.
	~Shader();

	// The program handle is owned, so copying would let two Shaders call
	// glDeleteProgram on the same ID. Moving transfers ownership instead and
	// leaves the source with nothing to release.
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;

	// Activates the Shader Program
	void Activate();
	// Deletes the Shader Program. Safe to call more than once.
	void Delete();
private:
	// Checks if the different Shaders have compiled properly
	void compileErrors(unsigned int shader, const char* type);
};


