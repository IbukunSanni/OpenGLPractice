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

// Owns GLFW's process-wide lifetime so every exit path terminates it.
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

// Lets unique_ptr destroy the GLFW window automatically.
struct WindowDeleter
{
	void operator()(GLFWwindow* window) const
	{
		glfwDestroyWindow(window);
	}
};

using WindowPtr = std::unique_ptr<GLFWwindow, WindowDeleter>;

// Deletes a shader program while the OpenGL context is still alive.
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

// Stop before model parsing when a required asset is missing.
void requireFile(const std::string& path, const char* assetName)
{
	if (std::ifstream(path).good())
		return;

	throw std::runtime_error(std::string("Missing ") + assetName + " model: " + path);
}

void run()
{
	// Automatic cleanup occurs in reverse declaration order.
	GlfwSession glfw;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	WindowPtr window(glfwCreateWindow(width, height, "17_transaparency_n_blending", nullptr, nullptr));
	if (!window)
	{
		const char* description = nullptr;
		glfwGetError(&description);
		throw std::runtime_error(std::string("Could not create GLFW window") +
			(description ? ": " + std::string(description) : ""));
	}

	glfwMakeContextCurrent(window.get());
	glfwSwapInterval(1); // Cap the frame rate to the monitor's refresh rate (VSync).
	if (!gladLoadGL())
		throw std::runtime_error("Could not load OpenGL functions with GLAD");

	glViewport(0, 0, width, height);

	Shader shaderProgram("default.vert", "default.frag");
	ShaderGuard shaderGuard(shaderProgram);

	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);
	glm::mat4 lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	glEnable(GL_DEPTH_TEST);

	// Discard front-facing polygons (CCW winding), so only back faces render.
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CW);

	// Model path parsing requires forward slashes for companion files.
	const std::string assetDirectory =
		"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/Assets";
	const std::string modelPath = assetDirectory + "/Models/statue/scene.gltf";

	requireFile(modelPath, "statue");

	Model model(modelPath.c_str());

	// Orbit around the loaded model's world-space bounds center. The statue's
	// mesh is only ~1 unit across, so the camera starts close enough to see it.
	const glm::vec3 initialCameraPosition(0.0f, 1.0f, 2.5f);
	const glm::vec3 initialCameraTarget = model.GetWorldCenter();
	Camera camera(width, height, initialCameraPosition);
	camera.LookAt(initialCameraPosition, initialCameraTarget);
	camera.AttachToWindow(window.get());

	double prevTime = 0.0;
	double crntTime = 0.0;
	double timeDiff;
	unsigned int counter = 0;
	std::string fps = "...";
	std::string ms = "...";

	// Render until Escape or the window close button requests shutdown.
	while (!glfwWindowShouldClose(window.get()))
	{
		crntTime = glfwGetTime();
		timeDiff = crntTime - prevTime;
		counter++;
		if (timeDiff >= 1.0 / 30.0)
		{
			fps = std::to_string(static_cast<int>((1.0 / timeDiff) * counter));
			ms = std::to_string((timeDiff / counter) * 1000);
			prevTime = crntTime;
			counter = 0;
		}

		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window.get(), true);

		camera.Inputs(window.get());
		camera.UpdateMatrix(45.0f, 0.1f, 100.0f);

		// Display live camera data and FPS for choosing reusable viewpoints.
		std::ostringstream titleStream;
		titleStream << std::fixed << std::setprecision(2)
			<< "17_transaparency_n_blending | FPS: " << fps << " | " << ms <<"ms" << " | "
			<< "Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
			<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
		glfwSetWindowTitle(window.get(), titleStream.str().c_str());

		model.Draw(shaderProgram, camera);

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
