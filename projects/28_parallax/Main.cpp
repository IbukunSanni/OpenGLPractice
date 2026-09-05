#include "Mesh.h"
#include<math.h>

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <string>
#include <vector>

// ===========================================================================
// Configuration
// ===========================================================================

constexpr const char* appName = "28_parallax";

constexpr unsigned int width  = 800;
constexpr unsigned int height = 800;

// Controls the gamma function
constexpr float gamma = 2.2f;

constexpr float fovDegrees = 45.0f;
constexpr float nearPlane  = 0.1f;
constexpr float farPlane   = 100.0f;

// The plane faces +Z, so the light has to sit at positive z or the surface is
// edge-on to it and nothing lights. Offset in x and y as well: a light aimed
// straight down a surface's own normal is the one case where a normal map
// changes almost nothing, since every perturbed normal still faces it.
constexpr glm::vec3 lightPosition(0.5f, 0.5f, 1.0f);

const std::string assetDirectory =
	"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/Assets";

// ===========================================================================
// Lifetime helpers
// ===========================================================================

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

// ===========================================================================
// Setup
// ===========================================================================

// The whole scene: one quad in the XY plane with its normal along +Z.
//
// Flat and axis-aligned on purpose. A normal map replaces this single constant
// normal per fragment, so a surface whose real normal never varies is the one
// that shows the map's effect and nothing else.
Mesh createPlaneMesh(std::vector<Texture>& textures)
{
	const glm::vec3 normal(0.0f, 0.0f, 1.0f);
	const glm::vec3 white(1.0f);

	// Counter-clockwise seen from +Z, i.e. from the camera. Nothing enables
	// GL_CULL_FACE here, so the back is drawn too and the order only matters if
	// culling is switched on later.
	std::vector<Vertex> vertices = {
		{ glm::vec3(-1.0f, -1.0f, 0.0f), normal, white, glm::vec2(0.0f, 0.0f) },
		{ glm::vec3( 1.0f, -1.0f, 0.0f), normal, white, glm::vec2(1.0f, 0.0f) },
		{ glm::vec3( 1.0f,  1.0f, 0.0f), normal, white, glm::vec2(1.0f, 1.0f) },
		{ glm::vec3(-1.0f,  1.0f, 0.0f), normal, white, glm::vec2(0.0f, 1.0f) },
	};
	std::vector<GLuint> indices = { 0, 1, 2, 0, 2, 3 };

	return Mesh(vertices, indices, textures);
}

// Stop before texture loading when a required asset is missing.
void requireFile(const std::string& path, const char* assetName)
{
	if (std::ifstream(path).good())
		return;

	throw std::runtime_error(std::string("Missing ") + assetName + ": " + path);
}

// Creates the window, makes its context current and loads the GL entry points.
// Nothing before this call may touch gl*; everything after it may.
WindowPtr createWindow(const char* title)
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	WindowPtr window(glfwCreateWindow(width, height, title, nullptr, nullptr));
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
	return window;
}

// ===========================================================================
// Per-frame
// ===========================================================================

// Live camera readout, so a viewpoint worth keeping can be copied back into
// initialCameraPosition.
std::string formatTitle(const std::string& fps, const std::string& ms, const Camera& camera)
{
	std::ostringstream stream;
	stream << std::fixed << std::setprecision(2)
		<< appName << " | FPS: " << fps << " | " << ms << "ms" << " | "
		<< "Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
		<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
	return stream.str();
}

// ===========================================================================

void run()
{
	// Automatic cleanup occurs in reverse declaration order, so GLFW outlives
	// every object that needs its context.
	GlfwSession glfw;
	WindowPtr window = createWindow(appName);

	// --- shaders ------------------------------------------------------------
	// The third argument is a GEOMETRY shader. It is required here, not optional:
	// a tangent needs two edges and two UV deltas, so it needs the whole triangle,
	// which no vertex shader ever sees. default.vert therefore emits WORLD space
	// and default.geom applies the projection after building the frame.
	Shader shaderProgram("default.vert", "default.frag", "default.geom");
	ShaderGuard shaderGuard(shaderProgram);

	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos = lightPosition;

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	// --- pipeline state -----------------------------------------------------
	// The other half of gamma correction. Inputs are decoded to linear when
	// sampled; this re-encodes the final colour to sRGB on write. Without it,
	// linearising the inputs alone just makes everything look too dark.
	glEnable(GL_FRAMEBUFFER_SRGB);

	glEnable(GL_DEPTH_TEST);

	// --- assets -------------------------------------------------------------
	const std::string clayRoofDiffusePath =
		assetDirectory + "/Textures/clay_roof/clay_roof_tiles_03_diff_1k.png";
	const std::string clayRoofNormalPath =
		assetDirectory + "/Textures/clay_roof/clay_roof_tiles_03_nor_gl_1k.png";
	const std::string clayRoofDisplacementPath =
		assetDirectory + "/Textures/clay_roof/clay_roof_tiles_03_disp_1k.png";

	// Still the planks map, and deliberately not renamed: it is a stand-in until
	// the clay roof's own roughness map goes in. default.frag samples specular0
	// unconditionally, so leaving unit 1 empty reads whatever is bound there
	// rather than simply dropping the highlight.
	const std::string planksSpecPath = assetDirectory + "/Textures/planksSpec.png";

	requireFile(clayRoofDiffusePath, "clay roof diffuse texture");
	requireFile(clayRoofNormalPath, "clay roof normal texture");
	requireFile(clayRoofDisplacementPath, "clay roof displacement texture");

	// Mesh::Draw numbers these into the diffuse0 / specular0 samplers
	// default.frag expects.
	std::vector<Texture> planeTextures = {
		Texture(clayRoofDiffusePath.c_str(), "diffuse", 0),
	};

	Mesh planeMesh = createPlaneMesh(planeTextures);

	Texture normalMap(clayRoofNormalPath.c_str(), "normal", 1);
	// Unit 2. Single-channel and NOT sRGB -- Texture only decodes "diffuse", and a
	// height map is a measurement, so decoding it would bend every step of the march.
	Texture displacementMap(clayRoofDisplacementPath.c_str(), "displacement", 2);

	// --- camera -------------------------------------------------------------
	// Straight down -Z at the plane's face. The quad spans +/-1 and a 45-degree
	// vertical FOV covers 0.414 units of half-height per unit of distance, so 3
	// units leaves the 2x2 face filling most of the frame with a small margin.
	const glm::vec3 initialCameraPosition(0.0f, 0.0f, 3.0f);
	Camera camera(width, height, initialCameraPosition);
	camera.LookAt(initialCameraPosition, glm::vec3(0.0f, 0.0f, 0.0f));
	camera.AttachToWindow(window.get());

	// --- render loop --------------------------------------------------------
	// Lighting model, flipped live with B so the two can be compared on the
	// same frame.
	bool useBlinnPhong = true;
	bool blinnKeyWasDown = false;

	// G toggles the output encoding so the difference is visible side by side.
	bool gammaCorrect = true;
	bool gammaKeyWasDown = false;

	// N swaps the mapped normal for the plane's own flat one, which is the
	// comparison that shows what the map is actually contributing.
	bool useNormalMap = true;
	bool normalMapKeyWasDown = false;

	// P compares the marched UV against the raw one. With it off the surface goes
	// back to being flat-with-lighting, which is what a normal map alone gives.
	bool useParallax = true;
	bool parallaxKeyWasDown = false;

	double prevTime = 0.0;
	unsigned int frameCounter = 0;
	std::string fps = "...";
	std::string ms = "...";

	while (!glfwWindowShouldClose(window.get()))
	{
		// Refresh the readout ~30x a second; updating it per frame flickers too
		// fast to read.
		const double crntTime = glfwGetTime();
		const double timeDiff = crntTime - prevTime;
		frameCounter++;
		if (timeDiff >= 1.0 / 30.0)
		{
			fps = std::to_string(static_cast<int>((1.0 / timeDiff) * frameCounter));
			ms  = std::to_string((timeDiff / frameCounter) * 1000);
			prevTime = crntTime;
			frameCounter = 0;
		}

		if (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window.get(), true);

		// Edge-detected: without the was-down check, holding B would flip the
		// model every single frame instead of once per press.
		const bool blinnKeyDown = glfwGetKey(window.get(), GLFW_KEY_B) == GLFW_PRESS;
		if (blinnKeyDown && !blinnKeyWasDown)
		{
			useBlinnPhong = !useBlinnPhong;
			std::cout << "Lighting: " << (useBlinnPhong ? "Blinn-Phong" : "Phong")
				<< std::endl;
		}
		blinnKeyWasDown = blinnKeyDown;

		const bool gammaKeyDown = glfwGetKey(window.get(), GLFW_KEY_G) == GLFW_PRESS;
		if (gammaKeyDown && !gammaKeyWasDown)
		{
			gammaCorrect = !gammaCorrect;
			if (gammaCorrect) glEnable(GL_FRAMEBUFFER_SRGB);
			else              glDisable(GL_FRAMEBUFFER_SRGB);
			std::cout << "Gamma correction: " << (gammaCorrect ? "on" : "off") << std::endl;
		}
		gammaKeyWasDown = gammaKeyDown;

		const bool normalMapKeyDown = glfwGetKey(window.get(), GLFW_KEY_N) == GLFW_PRESS;
		if (normalMapKeyDown && !normalMapKeyWasDown)
		{
			useNormalMap = !useNormalMap;
			std::cout << "Normal map: " << (useNormalMap ? "on" : "off (flat normal)")
				<< std::endl;
		}
		normalMapKeyWasDown = normalMapKeyDown;

		const bool parallaxKeyDown = glfwGetKey(window.get(), GLFW_KEY_P) == GLFW_PRESS;
		if (parallaxKeyDown && !parallaxKeyWasDown)
		{
			useParallax = !useParallax;
			std::cout << "Parallax: " << (useParallax ? "on" : "off (raw UVs)")
				<< std::endl;
		}
		parallaxKeyWasDown = parallaxKeyDown;

		camera.Inputs(window.get());
		camera.UpdateMatrix(fovDegrees, nearPlane, farPlane);
		glfwSetWindowTitle(window.get(), formatTitle(fps, ms, camera).c_str());

		glClearColor(pow(0.07f, gamma), pow(0.13f, gamma), pow(0.17f, gamma), 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Uploaded per frame so the B and N toggles take effect immediately.
		shaderProgram.Activate();
		normalMap.Bind();
		glUniform1i(glGetUniformLocation(shaderProgram.ID, "normal0"), 1);
		displacementMap.Bind();
		glUniform1i(glGetUniformLocation(shaderProgram.ID, "displacement0"), 2);

		glUniform1i(glGetUniformLocation(shaderProgram.ID, "useBlinnPhong"), useBlinnPhong);
		glUniform1i(glGetUniformLocation(shaderProgram.ID, "useNormalMap"), useNormalMap);
		glUniform1i(glGetUniformLocation(shaderProgram.ID, "useParallax"), useParallax);

		planeMesh.Draw(shaderProgram, camera, glm::mat4(1.0f));

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
