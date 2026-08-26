#include "Model.h"
#include<math.h>

#include <array>
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

constexpr const char* appName = "21_Instancing";

constexpr unsigned int width  = 800;
constexpr unsigned int height = 800;

// The model pass and the skybox pass build their projections separately, so
// these must stay shared -- if they drift apart the horizon no longer lines up.
constexpr float fovDegrees = 45.0f;
constexpr float nearPlane  = 0.1f;
constexpr float farPlane   = 100.0f;

// Both models are authored far larger than the ~29 units of view height at
// the camera distance below, so each is rescaled into its rest pose at load.
// jupiter is a 37.27-unit sphere centred on the origin; the asteroid is 9.36
// units and only 990 triangles -- which is why it is the one worth instancing.
constexpr float jupiterScale  = 0.27f;    // -> ~10.1 units across
constexpr float asteroidScale = 0.085f;   
constexpr float asteroidOrbit = 15.0f;    // ring radius, in world units

// Naive baseline: this many separate Draw calls per frame, one asteroid each.
// Instancing replaces the whole loop with a single call -- watch the FPS
// readout in the title bar before and after to see what it buys.
constexpr unsigned int asteroidCount = 8000;
constexpr float orbitSpread = 6.5f;       // radial + vertical scatter
constexpr float twoPi = 6.28318530718f;

const std::string assetDirectory =
	"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/Assets";

// ===========================================================================
// Skybox cube geometry
// ===========================================================================

float skyboxVertices[] =
{
	//   Coordinates
	-1.0f, -1.0f,  1.0f,//        7--------6
	 1.0f, -1.0f,  1.0f,//       /|       /|
	 1.0f, -1.0f, -1.0f,//      4--------5 |
	-1.0f, -1.0f, -1.0f,//      | |      | |
	-1.0f,  1.0f,  1.0f,//      | 3------|-2
	 1.0f,  1.0f,  1.0f,//      |/       |/
	 1.0f,  1.0f, -1.0f,//      0--------1
	-1.0f,  1.0f, -1.0f
};

unsigned int skyboxIndices[] =
{
	1, 2, 6,  6, 5, 1,   // Right
	0, 4, 7,  7, 3, 0,   // Left
	4, 5, 6,  6, 7, 4,   // Top
	0, 3, 2,  2, 1, 0,   // Bottom
	0, 1, 5,  5, 4, 0,   // Back
	3, 7, 6,  6, 2, 3    // Front
};

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

// Uniform random float in [-1, 1].
//
// rand() lands in [0, RAND_MAX]; dividing by RAND_MAX/2.0f maps that to
// [0, 2], and the -1.0f shifts it to [-1, 1]. The 2.0f matters -- an integer
// RAND_MAX/2 would truncate and the division would collapse to 0 or 1.
//
// srand() is never called, so the sequence is identical on every run. That
// keeps the asteroid field reproducible; seed it if you want variety.
float randf()
{
	return -1.0f + (rand() / (RAND_MAX / 2.0f));
}

// Stop before model parsing when a required asset is missing.
void requireFile(const std::string& path, const char* assetName)
{
	if (std::ifstream(path).good())
		return;

	throw std::runtime_error(std::string("Missing ") + assetName + " model: " + path);
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

struct SkyboxMesh
{
	unsigned int vao = 0;
	unsigned int vbo = 0;
	unsigned int ebo = 0;
};

// Position-only cube: no texture coordinates are stored, because skybox.vert
// derives the cubemap lookup direction straight from the vertex position.
SkyboxMesh createSkyboxMesh()
{
	SkyboxMesh mesh;
	glGenVertexArrays(1, &mesh.vao);
	glGenBuffers(1, &mesh.vbo);
	glGenBuffers(1, &mesh.ebo);

	glBindVertexArray(mesh.vao);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), &skyboxIndices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// The VAO is unbound before the element buffer so it keeps that binding.
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return mesh;
}

// A cubemap is ONE texture object holding six faces, not six textures.
// GL_TEXTURE_CUBE_MAP_POSITIVE_X..NEGATIVE_Z are six consecutive enum values,
// so POSITIVE_X + i walks them in the order `faces` is declared below:
// right, left, top, bottom, front, back. That order is not optional.
unsigned int loadCubemap(const std::array<std::string, 6>& faces)
{
	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// CLAMP_TO_EDGE on all three axes is what keeps the face joins seamless.
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// stb's vertical-flip setting is global rather than per-load, so it is set
	// once here instead of inside the loop.
	stbi_set_flip_vertically_on_load(false);

	for (unsigned int i = 0; i < faces.size(); i++)
	{
		int faceWidth, faceHeight, faceChannels;
		unsigned char* data = stbi_load(faces[i].c_str(), &faceWidth, &faceHeight, &faceChannels, 0);
		if (!data)
		{
			std::cout << "Failed to load texture: " << faces[i] << std::endl;
			stbi_image_free(data);
			continue;
		}

		// GL_RGB assumes 3 bytes/pixel; a 4-channel source read with that
		// stride shears diagonally, because GL then miscomputes each row's
		// offset. The format declared here must match what stb actually read.
		GLenum sourceFormat;
		switch (faceChannels)
		{
		case 3:
			sourceFormat = GL_RGB;
			break;
		case 4:
			sourceFormat = GL_RGBA;
			break;
		default:
			stbi_image_free(data);
			throw std::invalid_argument(
				"Unsupported number of skybox texture color channels: " +
				std::to_string(faceChannels) + " (" + faces[i] + ")");
		}

		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA,
			faceWidth, faceHeight, 0, sourceFormat, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}

	return texture;
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

// Drawn after the model so early-z can discard every fragment the model already
// covers: the cube writes depth 1.0, which loses to any real geometry.
void drawSkybox(Shader& shader, const Camera& camera, const SkyboxMesh& mesh, unsigned int cubemap)
{
	// ...but depth 1.0 must still pass where nothing was drawn, hence LEQUAL.
	glDepthFunc(GL_LEQUAL);
	shader.Activate();

	// Truncating to mat3 and back strips the view matrix's translation column,
	// so moving the camera cannot slide the skybox. Only looking around rotates
	// it, which is what keeps it reading as infinitely far away.
	const glm::mat4 view = glm::mat4(glm::mat3(glm::lookAt(
		camera.Position, camera.Position + camera.Orientation, camera.Up)));
	const glm::mat4 projection = glm::perspective(
		glm::radians(fovDegrees), (float)width / height, nearPlane, farPlane);

	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// The camera sits INSIDE this cube looking at its inner faces, so the
	// winding that reads as front from outside reads as back from in here --
	// the cull state tuned for the model would discard the whole thing.
	glDisable(GL_CULL_FACE);
	glBindVertexArray(mesh.vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	glEnable(GL_CULL_FACE);

	glDepthFunc(GL_LESS);
}

// ===========================================================================
// Main
// ===========================================================================

void run()
{
	// Automatic cleanup occurs in reverse declaration order, so GLFW outlives
	// every object that needs its context.
	GlfwSession glfw;
	WindowPtr window = createWindow(appName);

	// --- shaders ------------------------------------------------------------
	// One lit shader for the model, one unlit shader that samples the cubemap
	// for the sky behind it.
	Shader shaderProgram("default.vert", "default.frag", "default.geom");
	Shader skyboxShader("skybox.vert", "skybox.frag");
	Shader asteroidShader("asteroid.vert", "default.frag");
	ShaderGuard shaderGuard(shaderProgram);
	ShaderGuard skyboxGuard(skyboxShader);

	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	skyboxShader.Activate();
	glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

	asteroidShader.Activate();
	glUniform4f(glGetUniformLocation(asteroidShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(asteroidShader.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	// --- pipeline state -----------------------------------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	// glFrontFace only labels which winding counts as "front"; glCullFace names
	// what gets DISCARDED. FRONT+CW here keeps CCW-projected triangles, the
	// pairing glTF's outward-CCW authoring expects.
	glCullFace(GL_FRONT);
	glFrontFace(GL_CW);

	// --- assets -------------------------------------------------------------
	// Forward slashes throughout: Model resolves its companion .bin and texture
	// paths by string-slicing this one.
	const std::string jupiterPath  = assetDirectory + "/Models/jupiter/scene.gltf";
	const std::string asteroidPath = assetDirectory + "/Models/asteroid/scene.gltf";
	const std::array<std::string, 6> facesCubemap =
	{
		assetDirectory + "/Skybox/space/right.png",
		assetDirectory + "/Skybox/space/left.png",
		assetDirectory + "/Skybox/space/top.png",
		assetDirectory + "/Skybox/space/bottom.png",
		assetDirectory + "/Skybox/space/front.png",
		assetDirectory + "/Skybox/space/back.png"
	};

	requireFile(jupiterPath, "jupiter");
	requireFile(asteroidPath, "asteroid");
	Model jupiter(jupiterPath.c_str());


	// Rest poses baked in at load. ApplyTransform pre-multiplies each mesh node
	// matrix, so these act in world space and GetWorldCenter() below already
	// reflects them.
	jupiter.ApplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(jupiterScale)));

	// One world transform per asteroid, built once. Nothing is baked into the
	// model's rest pose now -- a single Model is drawn at every one of these,
	// which is exactly the workload instancing collapses into one call.
	std::vector<glm::mat4> asteroidTransforms;
	asteroidTransforms.reserve(asteroidCount);
	for (unsigned int i = 0; i < asteroidCount; i++)
	{
		// Generates x and y for the function x^2 + y^2 = radius^2 which is a circle
		float x = randf();
		float finalRadius = asteroidOrbit + randf() * orbitSpread;
		float y = ((rand() % 2) * 2 - 1) * sqrt(1.0f - x * x);

		// Holds transformations before multiplying them
		glm::vec3 tempTranslation;
		glm::quat tempRotation;
		glm::vec3 tempScale;

		// Makes the random distribution more even
		if (randf() > 0.5f)
		{
			// Generates a translation near a circle of radius "radius"
			tempTranslation = glm::vec3(y * finalRadius, randf(), x * finalRadius);
		}
		else
		{
			// Generates a translation near a circle of radius "radius"
			tempTranslation = glm::vec3(x * finalRadius, randf(), y * finalRadius);
		}

		// Generates random rotations
		tempRotation = glm::quat(1.0f, randf(), randf(), randf());
		// Generates random scales
		tempScale = 0.1f * glm::vec3(randf(), randf(), randf());

		// Initialize matrices
		glm::mat4 trans = glm::mat4(1.0f);
		glm::mat4 rot = glm::mat4(1.0f);
		glm::mat4 sca = glm::mat4(1.0f);

		// Transform the matrices to their correct form
		trans = glm::translate(trans, tempTranslation);
		rot = glm::mat4_cast(tempRotation);
		sca = glm::scale(sca, tempScale);

		// Push matrix transformation
		asteroidTransforms.push_back(trans * rot * sca);
	}

	Model asteroid(asteroidPath.c_str());
	asteroid.ApplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(asteroidScale)));


	const SkyboxMesh skybox = createSkyboxMesh();
	const unsigned int cubemapTexture = loadCubemap(facesCubemap);

	// --- camera -------------------------------------------------------------
	// Framed on the model's world-space bounds center, which already reflects
	// the scale baked in above.
	const glm::vec3 initialCameraPosition(0.0f, 15.0f, 35.0f);
	Camera camera(width, height, initialCameraPosition);
	camera.LookAt(initialCameraPosition, jupiter.GetWorldCenter());
	camera.AttachToWindow(window.get());

	// --- render loop --------------------------------------------------------
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

		camera.Inputs(window.get());
		camera.UpdateMatrix(fovDegrees, nearPlane, farPlane);
		glfwSetWindowTitle(window.get(), formatTitle(fps, ms, camera).c_str());

		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		jupiter.Draw(shaderProgram, camera);
		for (const glm::mat4& pose : asteroidTransforms)
			asteroid.Draw(shaderProgram, camera, pose);

	
		drawSkybox(skyboxShader, camera, skybox, cubemapTexture);

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
