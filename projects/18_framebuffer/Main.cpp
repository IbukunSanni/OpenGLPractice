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

// Fullscreen quad as two triangles, already in NDC so no projection is needed.
// Each vertex is x, y followed by u, v.
float rectangleVertices[] =
{
	// Coords    // texCoords
	 1.0f, -1.0f,  1.0f, 0.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	-1.0f,  1.0f,  0.0f, 1.0f,

	 1.0f,  1.0f,  1.0f, 1.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,
	-1.0f,  1.0f,  0.0f, 1.0f
};



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

	WindowPtr window(glfwCreateWindow(width, height, "18_framebuffer", nullptr, nullptr));
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
	Shader framebufferProgram("framebuffer.vert", "framebuffer.frag");

	ShaderGuard shaderGuard(shaderProgram);
	ShaderGuard bufferGuard(framebufferProgram);


	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);
	glm::mat4 lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
	framebufferProgram.Activate();
	// The offscreen color attachment is sampled from texture unit 0.
	glUniform1i(glGetUniformLocation(framebufferProgram.ID, "screenTexture"), 0);

	// Enables the Depth Buffer
	glEnable(GL_DEPTH_TEST);

	// Enables Cull Facing
	glEnable(GL_CULL_FACE);
	// Keeps front faces
	glCullFace(GL_FRONT);
	// Uses counter clock-wise standard
	glFrontFace(GL_CW);
	// Configures the blending function
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Model path parsing requires forward slashes for companion files.
	const std::string assetDirectory =
		"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/Assets";
	const std::string crowPath = assetDirectory + "/Models/crow/scene.gltf";

	requireFile(crowPath, "crow");

	Model crowModel(crowPath.c_str());

	// Prepare framebuffer rectangle VBO and VAO
	unsigned int rectVAO, rectVBO;
	glGenVertexArrays(1, &rectVAO);
	glGenBuffers(1, &rectVBO);
	glBindVertexArray(rectVAO);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);
	// Locations 0 and 1 must match the inputs declared in framebuffer.vert.
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	// Orbit around the crow's world-space bounds center.
	const glm::vec3 initialCameraPosition(0.0f, 15.0f, 35.0f);
	const glm::vec3 initialCameraTarget = crowModel.GetWorldCenter();
	Camera camera(width, height, initialCameraPosition);
	camera.LookAt(initialCameraPosition, initialCameraTarget);
	camera.AttachToWindow(window.get());

	double prevTime = 0.0;
	double crntTime = 0.0;
	double timeDiff;
	unsigned int counter = 0;
	std::string fps = "...";
	std::string ms = "...";

	// Create Frame Buffer Object
	unsigned int FBO;
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	// Create Framebuffer Texture
	unsigned int framebufferTexture;
	glGenTextures(1, &framebufferTexture);
	glBindTexture(GL_TEXTURE_2D, framebufferTexture);
	// NULL pixel data allocates empty storage; the scene pass fills it each frame.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Prevents edge bleeding
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Prevents edge bleeding
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferTexture, 0);

	// Depth and stencil go in a renderbuffer rather than a texture: the scene pass
	// needs them, but nothing samples them later, so a write-only target is cheaper.
	unsigned int RBO;
	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

	// An incomplete framebuffer silently draws nothing, so verify before rendering.
	auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer error: " << fboStatus << std::endl;

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

		// Bind the custom framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);

		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Enable depth testing since it's disabled when drawing the framebuffer rectangle
		glEnable(GL_DEPTH_TEST);

		if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window.get(), true);

		camera.Inputs(window.get());
		camera.UpdateMatrix(45.0f, 0.1f, 100.0f);

		// Display live camera data and FPS for choosing reusable viewpoints.
		std::ostringstream titleStream;
		titleStream << std::fixed << std::setprecision(2)
			<< "18_framebuffer | FPS: " << fps << " | " << ms <<"ms" << " | "
			<< "Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
			<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
		glfwSetWindowTitle(window.get(), titleStream.str().c_str());

		crowModel.Draw(shaderProgram, camera);

		// Bind the default framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		// Draw the framebuffer rectangle
		framebufferProgram.Activate();
		glBindVertexArray(rectVAO);
		glDisable(GL_DEPTH_TEST); // prevents framebuffer rectangle from being discarded
		glBindTexture(GL_TEXTURE_2D, framebufferTexture);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		

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
