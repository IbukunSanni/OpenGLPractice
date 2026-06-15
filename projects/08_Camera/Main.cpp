#include <iostream>
#include <exception>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Texture.h"
#include "shaderClass.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include"Camera.h"

const unsigned int width = 800;
const unsigned int height = 800;

// A square base pyramid with 5 vertices.
// Each vertex: X, Y, Z,  R, G, B,  U, V
// Base sits on Y=0; apex at Y=0.8 in the centre.
// UV coordinates are stretched (U up to 5) so the brick texture tiles across the faces.
GLfloat vertices[] =
{ //     COORDINATES     /        COLORS          /  TexCoord  //
	-0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,   0.0f, 0.0f, // Front left
	-0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,   5.0f, 0.0f, // Back  left
	 0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,   0.0f, 0.0f, // Back  right
	 0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,   5.0f, 0.0f, // Front right
	 0.0f, 0.8f,  0.0f,     0.92f, 0.86f, 0.76f,   2.5f, 5.0f  // Apex
};

// Two triangles form the square base; four triangles form the side faces.
GLuint indices[] =
{
	0, 1, 2, // Base  triangle 1
	0, 2, 3, // Base  triangle 2
	0, 1, 4, // Left  face
	1, 2, 4, // Back  face
	2, 3, 4, // Right face
	3, 0, 4  // Front face
};

int main()
{
	// ── GLFW initialisation ────────────────────────────────────────────────
	glfwInit();

	// Request an OpenGL 3.3 Core Profile context (no legacy/deprecated features)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "YoutubeOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// ── GLAD — load OpenGL function pointers ──────────────────────────────
	if (!gladLoadGL())
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	// Map the OpenGL viewport to the full window
	glViewport(0, 0, width, height);

	// ── Shader programme ──────────────────────────────────────────────────
	// Compiles default.vert and default.frag, links them into a single programme
	Shader shaderProgram("default.vert", "default.frag");

	// ── GPU buffer setup ──────────────────────────────────────────────────
	// The VAO records all attribute layout / buffer binding calls made while it is bound.
	// Rebinding it later restores the full vertex format automatically.
	VAO VAO1;
	VAO1.Bind();

	VBO VBO1(vertices, sizeof(vertices)); // Upload vertex data to the GPU
	EBO EBO1(indices, sizeof(indices));  // Upload index data to the GPU

	// Tell the VAO how to interpret the raw bytes in VBO1:
	//   attrib 0 → 3 floats (position),  stride = 8 floats, offset = 0
	//   attrib 1 → 3 floats (color),     stride = 8 floats, offset = 3 floats
	//   attrib 2 → 2 floats (UV),        stride = 8 floats, offset = 6 floats
	VAO1.LinkAttrib(VBO1, 0, 3, GL_FLOAT, 8 * sizeof(float), (void*)0);
	VAO1.LinkAttrib(VBO1, 1, 3, GL_FLOAT, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	VAO1.LinkAttrib(VBO1, 2, 2, GL_FLOAT, 8 * sizeof(float), (void*)(6 * sizeof(float)));

	// Unbind to avoid accidental modification after setup
	VAO1.Unbind();
	VBO1.Unbind();
	EBO1.Unbind();

	// Cache the uniform location for the scale value (queried once, reused every frame)
	GLuint uniID = glGetUniformLocation(shaderProgram.ID, "scale");

	// ── Texture ───────────────────────────────────────────────────────────
	// Loads the image, uploads it to texture unit 0, and binds the sampler uniform "tex0"
	Texture brickTex("Assets/Textures/green_plane.jpg", GL_TEXTURE_2D, GL_TEXTURE0, GL_RGBA, GL_UNSIGNED_BYTE);
	brickTex.texUnit(shaderProgram, "tex0", 0);

	// Depth testing ensures back faces do not overdraw front faces
	glEnable(GL_DEPTH_TEST);

	// Creates camera object
	Camera camera(width, height, glm::vec3(0.0f, 0.0f, 2.0f));

	// ── Render loop ───────────────────────────────────────────────────────
	while (!glfwWindowShouldClose(window))
	{
		// Clear colour and depth buffers at the start of each frame
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shaderProgram.Activate();

		camera.Inputs(window);





		// scale uniform is no longer used for positioning but kept to avoid an unused-uniform warning
		glUniform1f(uniID, 0.5f);

		brickTex.Bind(); // Bind texture before the draw call
		VAO1.Bind();     // Restore vertex format

		// Draw all 6 triangles (12 indices) using the EBO index list
		glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(GLuint), GL_UNSIGNED_INT, 0);

		glfwSwapBuffers(window); // Present the finished frame
		glfwPollEvents();        // Process window/input events
	}

	// ── Cleanup ───────────────────────────────────────────────────────────
	VAO1.Delete();
	VBO1.Delete();
	EBO1.Delete();
	brickTex.Delete();
	shaderProgram.Delete();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
