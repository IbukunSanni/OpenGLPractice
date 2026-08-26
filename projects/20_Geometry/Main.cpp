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
	// Right
	1, 2, 6,
	6, 5, 1,
	// Left
	0, 4, 7,
	7, 3, 0,
	// Top
	4, 5, 6,
	6, 7, 4,
	// Bottom
	0, 3, 2,
	2, 1, 0,
	// Back
	0, 1, 5,
	5, 4, 0,
	// Front
	3, 7, 6,
	6, 2, 3
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

	WindowPtr window(glfwCreateWindow(width, height, "20_Geometry", nullptr, nullptr));
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

	// Two separate programs: one lit shader for the model, one unlit shader
	// that samples a cubemap for the skybox behind it.
	Shader shaderProgram("default.vert", "default.frag");
	Shader skyboxShader("skybox.vert", "skybox.frag");

	ShaderGuard shaderGuard(shaderProgram);
	ShaderGuard skyboxGuard(skyboxShader);

	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
	skyboxShader.Activate();
	glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

	// Enables the Depth Buffer
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_CULL_FACE);
	// glFrontFace only labels which winding counts as "front"; glCullFace
	// names what gets DISCARDED. FRONT+CW here keeps CCW-projected
	// triangles — the pairing glTF's outward-CCW authoring expects.
	glCullFace(GL_FRONT);
	glFrontFace(GL_CW);

	// Model path parsing requires forward slashes for companion files.
	const std::string assetDirectory =
		"C:/Users/Ibukunoluwa/Documents/Coding/C-C++/OpenGL-VSstudio/OpenGLPractice/Assets";
	const std::string modelPath = assetDirectory + "/Models/bakugo/scene.gltf";

	// All the faces of the cubemap (make sure they are in this exact order)
	std::array<std::string, 6> facesCubemap =
	{
		assetDirectory + "/Skybox/sky_42/right.png",
		assetDirectory + "/Skybox/sky_42/left.png",
		assetDirectory + "/Skybox/sky_42/top.png",
		assetDirectory + "/Skybox/sky_42/bottom.png",
		assetDirectory + "/Skybox/sky_42/front.png",
		assetDirectory + "/Skybox/sky_42/back.png"
	};

	requireFile(modelPath, "airplane");
	Model model(modelPath.c_str());

	// for bakugo, scale is altered
	model.ApplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.05f)));

	// Skybox cube: position-only. Its texture coordinates aren't stored —
	// skybox.vert derives them straight from the vertex position instead.
	unsigned int skyboxVAO, skyboxVBO, skyboxEBO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glGenBuffers(1, &skyboxEBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), &skyboxIndices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// A cubemap is one texture object holding six faces (not six separate
	// textures) — GL_TEXTURE_CUBE_MAP is the target for all of them.
	unsigned int cubemapTexture;
	glGenTextures(1, &cubemapTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// These are very important to prevent seams
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// stb's vertical-flip setting is global, not per-load, so it only needs
	// setting once before the loop below.
	stbi_set_flip_vertically_on_load(false);

	// GL_TEXTURE_CUBE_MAP_POSITIVE_X..NEGATIVE_Z are six consecutive enum
	// values, so POSITIVE_X + i steps through all six faces in the order
	// facesCubemap was declared: right, left, top, bottom, front, back.
	for (unsigned int i = 0; i < facesCubemap.size(); i++)
	{
		int faceWidth, faceHeight, faceChannels;
		unsigned char* data = stbi_load(facesCubemap[i].c_str(), &faceWidth, &faceHeight, &faceChannels, 0);
		if (data)
		{
			// GL_RGB assumes 3 bytes/pixel; a 4-channel (RGBA) source read with
			// that stride shears diagonally, since GL miscomputes each row's
			// offset. Match the format glTexImage2D is told to the real data.
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
					std::to_string(faceChannels) + " (" + facesCubemap[i] + ")"
				);
			}
			glTexImage2D
			(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0,
				GL_RGBA,
				faceWidth,
				faceHeight,
				0,
				sourceFormat,
				GL_UNSIGNED_BYTE,
				data
			);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Failed to load texture: " << facesCubemap[i] << std::endl;
			stbi_image_free(data);
		}
	}

	// Orbit around the model's world-space bounds center.
	const glm::vec3 initialCameraPosition(0.0f, 15.0f, 35.0f);
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
		// Recompute the displayed FPS/ms every ~1/30s instead of every frame;
		// updating per-frame would flicker too fast to read.
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
			<< "20_Geometry | FPS: " << fps << " | " << ms <<"ms" << " | "
			<< "Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
			<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
		glfwSetWindowTitle(window.get(), titleStream.str().c_str());

		// for bakugo, scale and rotation are alterred
		model.Draw(shaderProgram, camera);

		// Since the cubemap will always have a depth of 1.0, we need that equal sign so it doesn't get discarded
		glDepthFunc(GL_LEQUAL);

		skyboxShader.Activate();
		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 projection = glm::mat4(1.0f);
		// Truncating to mat3 and back strips the view matrix's translation
		// column, so camera movement can't slide the skybox — only rotation
		// (looking around) affects it, keeping it infinitely far away.
		view = glm::mat4(glm::mat3(glm::lookAt(camera.Position, camera.Position + camera.Orientation, camera.Up)));
		projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
		glUniformMatrix4fv(glGetUniformLocation(skyboxShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(skyboxShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

		// Draws the cubemap as the last object so we can save a bit of performance by discarding all fragments
		// where an object is present (a depth of 1.0f will always fail against any object's depth value)

		// The camera sits inside the skybox cube looking at its inner faces, so
		// the winding that reads as "front" from outside reads as the opposite
		// from in here — cull state tuned for the model would drop this cube.
		glDisable(GL_CULL_FACE);

		glBindVertexArray(skyboxVAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glEnable(GL_CULL_FACE);
		// Switch back to the normal depth function
		glDepthFunc(GL_LESS);

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
