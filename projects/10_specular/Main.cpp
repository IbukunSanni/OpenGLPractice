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
#include "Camera.h"

const unsigned int width = 800;
const unsigned int height = 800;

// Pyramid with duplicated vertices per face so each face has its own normal.
// Each vertex: X, Y, Z,  R, G, B,  U, V,  Nx, Ny, Nz
GLfloat vertices[] =
{ //     COORDINATES     /        COLORS          /    TexCoord   /        NORMALS       //
	-0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,    0.0f, 0.0f,      0.0f, -1.0f, 0.0f, // Bottom side
	-0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,    0.0f, 5.0f,      0.0f, -1.0f, 0.0f, // Bottom side
	 0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,    5.0f, 5.0f,      0.0f, -1.0f, 0.0f, // Bottom side
	 0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,    5.0f, 0.0f,      0.0f, -1.0f, 0.0f, // Bottom side

	-0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,    0.0f, 0.0f,     -0.8f, 0.5f,  0.0f, // Left side
	-0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,    5.0f, 0.0f,     -0.8f, 0.5f,  0.0f, // Left side
	 0.0f, 0.8f,  0.0f,     0.92f, 0.86f, 0.76f,    2.5f, 5.0f,     -0.8f, 0.5f,  0.0f, // Left side

	-0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,    5.0f, 0.0f,      0.0f, 0.5f, -0.8f, // Back side
	 0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,    0.0f, 0.0f,      0.0f, 0.5f, -0.8f, // Back side
	 0.0f, 0.8f,  0.0f,     0.92f, 0.86f, 0.76f,    2.5f, 5.0f,      0.0f, 0.5f, -0.8f, // Back side

	 0.5f, 0.0f, -0.5f,     0.83f, 0.70f, 0.44f,    0.0f, 0.0f,      0.8f, 0.5f,  0.0f, // Right side
	 0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,    5.0f, 0.0f,      0.8f, 0.5f,  0.0f, // Right side
	 0.0f, 0.8f,  0.0f,     0.92f, 0.86f, 0.76f,    2.5f, 5.0f,      0.8f, 0.5f,  0.0f, // Right side

	 0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,    5.0f, 0.0f,      0.0f, 0.5f,  0.8f, // Front side
	-0.5f, 0.0f,  0.5f,     0.83f, 0.70f, 0.44f,    0.0f, 0.0f,      0.0f, 0.5f,  0.8f, // Front side
	 0.0f, 0.8f,  0.0f,     0.92f, 0.86f, 0.76f,    2.5f, 5.0f,      0.0f, 0.5f,  0.8f  // Front side
};

// Two triangles form the base and four triangles form the side faces.
GLuint indices[] =
{
	0, 1, 2,
	0, 2, 3,
	4, 6, 5,
	7, 9, 8,
	10, 12, 11,
	13, 15, 14
};

GLfloat lightVertices[] =
{ //     COORDINATES     //
	-0.1f, -0.1f,  0.1f,
	-0.1f, -0.1f, -0.1f,
	 0.1f, -0.1f, -0.1f,
	 0.1f, -0.1f,  0.1f,
	-0.1f,  0.1f,  0.1f,
	-0.1f,  0.1f, -0.1f,
	 0.1f,  0.1f, -0.1f,
	 0.1f,  0.1f,  0.1f
};

// 12 triangles (36 indices) forming a cube for the light source marker.
GLuint lightIndices[] =
{
	0, 1, 2,
	0, 2, 3,
	0, 4, 7,
	0, 7, 3,
	3, 7, 6,
	3, 6, 2,
	2, 6, 5,
	2, 5, 1,
	1, 5, 4,
	1, 4, 0,
	4, 5, 6,
	4, 6, 7
};

int main()
{
	// ── GLFW initialisation ────────────────────────────────────────────────
	glfwInit();

	// Request an OpenGL 3.3 Core Profile context (no legacy/deprecated features)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "10_specular", NULL, NULL);
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
	//   attrib 0 → 3 floats (position),  stride = 11 floats, offset = 0
	//   attrib 1 → 3 floats (color),     stride = 11 floats, offset = 3 floats
	//   attrib 2 → 2 floats (UV),        stride = 11 floats, offset = 6 floats
	//   attrib 3 → 3 floats (normal),    stride = 11 floats, offset = 8 floats
	VAO1.LinkAttrib(VBO1, 0, 3, GL_FLOAT, 11 * sizeof(float), (void*)0);
	VAO1.LinkAttrib(VBO1, 1, 3, GL_FLOAT, 11 * sizeof(float), (void*)(3 * sizeof(float)));
	VAO1.LinkAttrib(VBO1, 2, 2, GL_FLOAT, 11 * sizeof(float), (void*)(6 * sizeof(float)));
	VAO1.LinkAttrib(VBO1, 3, 3, GL_FLOAT, 11 * sizeof(float), (void*)(8 * sizeof(float)));

	// Unbind to avoid accidental modification after setup
	VAO1.Unbind();
	VBO1.Unbind();
	EBO1.Unbind();

	// Compile and link the light-cube shaders (light.vert / light.frag).
	// These are kept separate from the pyramid shaders so the cube can render
	// as a plain solid colour independent of the Phong lighting calculation.
	Shader lightShader("light.vert", "light.frag");

	VAO lightVAO;
	lightVAO.Bind();

	VBO lightVBO(lightVertices, sizeof(lightVertices));
	EBO lightEBO(lightIndices, sizeof(lightIndices));

	// The light cube only needs attribute 0 (position — 3 floats, tightly packed).
	// No color, UV, or normal attributes are used by the light shaders.
	lightVAO.LinkAttrib(lightVBO, 0, 3, GL_FLOAT, 3 * sizeof(float), (void*)0);

	lightVAO.Unbind();
	lightVBO.Unbind();
	lightEBO.Unbind();

	// ── Light and pyramid transforms (initial / pre-loop setup) ─────────────
	// These values are used once here to push the initial uniforms to both
	// shaders before the render loop starts. Inside the loop, local copies
	// shadow these — see the per-frame update comment below.
	glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Pure white light
	glm::vec3 lightPos = glm::vec3(1.2f, 0.8f, 0.8f);          // World-space light position
	glm::mat4 lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos); // Place the light cube at lightPos

	glm::vec3 pyramidPos = glm::vec3(0.0f, 0.0f, 0.0f); // Pyramid sits at the world origin
	glm::mat4 pyramidModel = glm::mat4(1.0f);
	pyramidModel = glm::translate(pyramidModel, pyramidPos);

	lightShader.Activate();
	// Set the light cube's model transform and visible color.
	glUniformMatrix4fv(glGetUniformLocation(lightShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(lightModel));
	glUniform4f(glGetUniformLocation(lightShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);

	shaderProgram.Activate();
	// Send light parameters used by the pyramid lighting calculations.
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, "model"), 1, GL_FALSE, glm::value_ptr(pyramidModel));
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);


	// ── Texture ───────────────────────────────────────────────────────────
	// Loads the image, uploads it to texture unit 0, and binds the sampler uniform "tex0"
	// Pass the slot as a plain index (0 = GL_TEXTURE0); Texture internally computes GL_TEXTURE0 + slot.
	Texture brickTex("Assets/Textures/green_plane.jpg", GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE);
	brickTex.texUnit(shaderProgram, "tex0", 0);

	// Depth testing ensures back faces do not overdraw front faces
	glEnable(GL_DEPTH_TEST);

	// Creates camera object
	Camera camera(width, height, glm::vec3(0.0f, 0.0f, 2.0f));
	camera.AttachToWindow(window);

	// ── Render loop ───────────────────────────────────────────────────────
	while (!glfwWindowShouldClose(window))
	{
		// Clear colour and depth buffers at the start of each frame
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Per-frame model matrix rebuild — local variables shadow the pre-loop ones.
		// Keeping this here makes it straightforward to animate the light or pyramid
		// later (e.g. multiply by glfwGetTime()).
		glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
		glm::mat4 lightModel = glm::mat4(1.0f);
		lightModel = glm::translate(lightModel, lightPos); // Cube follows lightPos each frame

		glm::vec3 pyramidPos = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::mat4 pyramidModel = glm::mat4(1.0f);
		pyramidModel = glm::translate(pyramidModel, pyramidPos);

		lightShader.Activate();
		// Update only the model matrix for the light cube draw.
		glUniformMatrix4fv(glGetUniformLocation(lightShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(lightModel));

		shaderProgram.Activate();
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, "model"), 1, GL_FALSE, glm::value_ptr(pyramidModel));

		// Handles camera inputs
		camera.Inputs(window);
		// Updates and exports the camera matrix to the Vertex Shader
		camera.UpdateMatrix(45.0f, 0.1f, 50.0f);

		shaderProgram.Activate();
		// Upload the camera's world-space position every frame so the fragment shader
		// can compute the view direction for the specular highlight calculation.
		glUniform3f(glGetUniformLocation(shaderProgram.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
		camera.Matrix(shaderProgram, "camMatrix");

		lightShader.Activate();
		camera.Matrix(lightShader, "camMatrix");

		lightVAO.Bind();
		glDrawElements(GL_TRIANGLES, sizeof(lightIndices) / sizeof(GLuint), GL_UNSIGNED_INT, 0);

		shaderProgram.Activate();
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
	lightVAO.Delete();
	lightVBO.Delete();
	lightEBO.Delete();
	brickTex.Delete();
	lightShader.Delete();
	shaderProgram.Delete();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
