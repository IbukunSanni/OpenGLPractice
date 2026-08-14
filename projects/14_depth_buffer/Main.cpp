#include "Model.h"

#include <exception>
#include <fstream>   // std::ifstream, used for the model-exists check
#include <iomanip>   // std::setprecision
#include <iostream>
#include <sstream>   // std::ostringstream for building the window title
#include <string>

constexpr unsigned int width  = 800;
constexpr unsigned int height = 800;

// ── run() ─────────────────────────────────────────────────────────────────────
// Separated from main() so every early-return path is caught by the try/catch
// in main() and printed as a clean error message instead of a crash dialog.
int run()
{
	// ── GLFW initialisation ───────────────────────────────────────────────────
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return -1;
	}

	// Request OpenGL 3.3 Core Profile — matches the #version 330 core in every shader.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "14_depth_buffer", nullptr, nullptr);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// GLAD must load the driver function pointers after a context is current.
	if (!gladLoadGL())
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	// Map the OpenGL NDC cube (-1..1) to the full window pixel area.
	glViewport(0, 0, width, height);

	// ── Shaders ───────────────────────────────────────────────────────────────
	// One shader is enough now: the model supplies its own geometry and textures,
	// so there is no separate light-marker cube to draw with a flat shader.
	// Built as a full path (rather than a bare relative filename) so the shader
	// files load correctly regardless of the process's current working directory.
	const std::string shaderDirectory =
		"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/projects/14_depth_buffer";
	const std::string vertexShaderPath = shaderDirectory + "/default.vert";
	const std::string fragmentShaderPath = shaderDirectory + "/default.frag";
	Shader shaderProgram(vertexShaderPath.c_str(), fragmentShaderPath.c_str());

	// ── Light uniforms ────────────────────────────────────────────────────────
	// Both values are constant for the lifetime of the program, so upload them
	// once here instead of repeating the calls every frame. The model matrices
	// and camPos are uploaded per-mesh inside Mesh::Draw().
	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"),   lightPos.x,   lightPos.y,   lightPos.z);

	// ── Depth testing ─────────────────────────────────────────────────────────
	// Ensures closer fragments overwrite farther ones, giving correct 3-D overlap.
	glEnable(GL_DEPTH_TEST);

	// ── Camera setup ──────────────────────────────────────────────────────────
	// Positioned on +Z looking back toward the origin, where the model sits.
	// AttachToWindow stores the GLFW handle so Inputs() can poll keys each frame.
	Camera camera(width, height, glm::vec3(0.0f, 0.0f, 2.0f));
	camera.AttachToWindow(window);

	// ── Model ─────────────────────────────────────────────────────────────────
	// Use FORWARD slashes. Model::getData() and Model::getTextures() locate the
	// companion .bin and texture files by splitting this string on '/', so a path
	// written with backslashes resolves to an empty directory and the model's
	// buffers silently fail to load. Windows accepts forward slashes fine.
	const std::string assetDirectory =
		"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/Assets";
	const std::string modelPath = assetDirectory + "/Models/autumn_sword/scene.gltf";
	// AGENT: fill teh directories
	const std::string groundPath = assetDirectory + "";
	const std::string treesPath = assetDirectory + "";

	// Fail loudly with a useful message if the asset is missing, rather than
	// letting the JSON parser throw something cryptic. std::ifstream is used
	// instead of std::filesystem::exists so this project does not require C++17.
	if (!std::ifstream(modelPath).good())
	{
		std::cerr << "Model file not found:\n  " << modelPath << "\n"
			<< "Download a glTF model into that folder, or edit 'modelPath' in Main.cpp."
			<< std::endl;
		shaderProgram.Delete();
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	// Model parses the .gltf, pulls vertices/indices out of the .bin buffer,
	// loads every referenced texture, and builds one Mesh per glTF mesh node.
	Model model(modelPath.c_str());
	Model model(groundPath.c_str());
	Model model(treesPath.c_str());

	// ── Render loop ───────────────────────────────────────────────────────────
	while (!glfwWindowShouldClose(window))
	{
		// Start each frame with a clean slate — clear color and depth.
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Escape closes the window immediately.
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		// Inputs() reads WASD / mouse and moves the camera.
		// UpdateMatrix() recomputes proj * view and caches it inside the camera
		// so Mesh::Draw() can pass it to whichever shader it needs.
		camera.Inputs(window);
		camera.UpdateMatrix(45.0f, 0.1f, 100.0f); // 45 degree FOV, near=0.1, far=100

		// ── Window title HUD ──────────────────────────────────────────────────
		// Shows live camera position and orientation — useful for debugging.
		std::ostringstream titleStream;
		titleStream << std::fixed << std::setprecision(2)
			<< "14_depth_buffer | Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
			<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
		glfwSetWindowTitle(window, titleStream.str().c_str());

		// Draws every mesh in the model, each with its own node transform.
		model.Draw(shaderProgram, camera);

		// Present the finished frame and process pending OS/input events.
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// ── Cleanup ───────────────────────────────────────────────────────────────
	// The Model owns the textures and VAOs it created, so only the shader and
	// window need explicit teardown here.
	shaderProgram.Delete();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

// ── Entry point ───────────────────────────────────────────────────────────────
// Wraps run() in a try/catch so any exception thrown by OpenGL setup or asset
// loading is printed as a readable message instead of a silent crash.
int main()
{
	try
	{
		return run();
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Application error: " << exception.what() << std::endl;
		return -1;
	}
}
