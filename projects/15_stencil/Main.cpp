#include "Model.h"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

constexpr unsigned int width  = 800;
constexpr unsigned int height = 800;

class GlfwSession
{
public:
	GlfwSession()
	{
		if (!glfwInit())
			throw std::runtime_error("Could not initialize GLFW");
	}

	~GlfwSession()
	{
		glfwTerminate();
	}

	GlfwSession(const GlfwSession&) = delete;
	GlfwSession& operator=(const GlfwSession&) = delete;
};

struct WindowDeleter
{
	void operator()(GLFWwindow* window) const
	{
		glfwDestroyWindow(window);
	}
};

using WindowPtr = std::unique_ptr<GLFWwindow, WindowDeleter>;

class ShaderGuard
{
public:
	explicit ShaderGuard(Shader& shader) : shader(shader) {}

	~ShaderGuard()
	{
		shader.Delete();
	}

	ShaderGuard(const ShaderGuard&) = delete;
	ShaderGuard& operator=(const ShaderGuard&) = delete;

private:
	Shader& shader;
};

void requireFile(const std::string& path, const char* assetName)
{
	if (std::ifstream(path).good())
		return;

	throw std::runtime_error(std::string("Missing ") + assetName + " model: " + path);
}

void run()
{
	GlfwSession glfw;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	WindowPtr window(glfwCreateWindow(width, height, "15_stencil", nullptr, nullptr));
	if (!window)
	{
		const char* description = nullptr;
		glfwGetError(&description);
		throw std::runtime_error(std::string("Could not create GLFW window") +
			(description ? ": " + std::string(description) : ""));
	}

	glfwMakeContextCurrent(window.get());
	if (!gladLoadGL())
		throw std::runtime_error("Could not load OpenGL functions with GLAD");

	glViewport(0, 0, width, height);

	Shader shaderProgram("default.vert", "default.frag");
	Shader outliningProgram("outlining.vert", "outlining.frag");
	ShaderGuard shaderGuard(shaderProgram);
	ShaderGuard outliningShaderGuard(outliningProgram);

	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);
	glm::mat4 lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	// Enables the Depth Buffer
	glEnable(GL_DEPTH_TEST);
	// Enables the Stencil Buffer
	glEnable(GL_STENCIL_TEST);
	// Sets rules for outcomes of stecil tests
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glDepthFunc(GL_LESS);

	const glm::vec3 initialCameraPosition(0.0f, 13.0f, 30.0f);
	const glm::vec3 initialCameraDirection(0.0f, 0.0f, -1.0f);
	Camera camera(width, height, initialCameraPosition);
	camera.SetView(initialCameraPosition, initialCameraDirection);
	camera.AttachToWindow(window.get());

	// Model path parsing requires forward slashes for companion files.
	const std::string assetDirectory =
		"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/Assets";
	const std::string modelPath = assetDirectory + "/Models/crow/scene.gltf";
	const std::string outlinePath = assetDirectory + "/Models/crow-outline/scene.gltf";

	requireFile(modelPath, "crow");
	requireFile(outlinePath, "crow outline");

	Model model(modelPath.c_str());
	Model outline(outlinePath.c_str());

	while (!glfwWindowShouldClose(window.get()))
	{
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window.get(), true);

		camera.Inputs(window.get());
		camera.UpdateMatrix(45.0f, 0.1f, 100.0f);

		std::ostringstream titleStream;
		titleStream << std::fixed << std::setprecision(2)
			<< "15_stencil | Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
			<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
		glfwSetWindowTitle(window.get(), titleStream.str().c_str());

		glStencilFunc(GL_ALWAYS,1, 0xFF);
		glStencilMask(0xFF);

		model.Draw(shaderProgram, camera);

		glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
		glStencilMask(0x00);
		glDisable(GL_DEPTH_TEST);

		// First method from the tutorial
		outliningProgram.Activate();
		glUniform1f(glGetUniformLocation(outliningProgram.ID, "outlining"), 0.18f);
		outline.Draw(outliningProgram, camera);


		// Restore state for the next frame.
		glStencilMask(0xFF);
		glStencilFunc(GL_ALWAYS, 0, 0xFF);
		glEnable(GL_DEPTH_TEST);



		glfwSwapBuffers(window.get());
		glfwPollEvents();
	}
}

int main()
{
	try
	{
		run();
		return EXIT_SUCCESS;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Fatal error: " << exception.what() << '\n';
		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::cerr << "Fatal error: unknown exception\n";
		return EXIT_FAILURE;
	}
}
