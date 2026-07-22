#include "Mesh.h"

#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

constexpr unsigned int width = 800;
constexpr unsigned int height = 800;

// Floor data follows Vertex's position, normal, color, UV layout.
const std::vector<Vertex> floorVertices =
{
	Vertex{glm::vec3(-1.0f, 0.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(0.0f, 0.0f)},
	Vertex{glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(0.0f, 1.0f)},
	Vertex{glm::vec3(1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(1.0f, 1.0f)},
	Vertex{glm::vec3(1.0f, 0.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)}
};

const std::vector<GLuint> floorIndices =
{
	0, 1, 2,
	0, 2, 3
};

// A small cube marks the light's world-space position.
const std::vector<Vertex> lightVertices =
{
	Vertex{glm::vec3(-0.1f, -0.1f,  0.1f)},
	Vertex{glm::vec3(-0.1f, -0.1f, -0.1f)},
	Vertex{glm::vec3(0.1f, -0.1f, -0.1f)},
	Vertex{glm::vec3(0.1f, -0.1f,  0.1f)},
	Vertex{glm::vec3(-0.1f,  0.1f,  0.1f)},
	Vertex{glm::vec3(-0.1f,  0.1f, -0.1f)},
	Vertex{glm::vec3(0.1f,  0.1f, -0.1f)},
	Vertex{glm::vec3(0.1f,  0.1f,  0.1f)}
};


const std::vector<GLuint> lightIndices =
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


int run()
{
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return -1;
	}

	// Request the OpenGL version used by all shaders in this project.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "12_mesh", nullptr, nullptr);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// GLAD must load the driver functions after an OpenGL context exists.
	if (!gladLoadGL())
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	// Map normalized device coordinates to the full window.
	glViewport(0, 0, width, height);

	const std::string textureDirectory =
		R"(C:\Users\Ibukunoluwa\Documents\Coding\C-C++\OpenGL-VSstudio\OpenGLPractice\Assets\Textures)";
	const std::string planksPath = textureDirectory + R"(\planks.png)";
	const std::string planksSpecPath = textureDirectory + R"(\planksSpec.png)";


	// Texture objects must be created after GLAD has loaded the OpenGL functions.
	std::vector<Texture> textures
	{
		Texture(planksPath.c_str(), "diffuse", 0, GL_UNSIGNED_BYTE),
		Texture(planksSpecPath.c_str(), "specular", 1, GL_UNSIGNED_BYTE)
	};

	// The floor and light marker use separate shader programs.
	Shader shaderProgram("default.vert", "default.frag");
	Shader lightShader("light.vert", "light.frag");

	const std::vector<Texture> noTextures;
	Mesh floor(floorVertices, floorIndices, textures);
	Mesh light(lightVertices, lightIndices, noTextures);

	const glm::vec4 lightColor(1.0f);

	lightShader.Activate();
	glUniform4f(glGetUniformLocation(lightShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);

	// Keep nearer fragments and discard fragments hidden behind them.
	glEnable(GL_DEPTH_TEST);

	// Start above and behind the floor, looking toward its center.
	Camera camera(width, height, glm::vec3(0.0f, 0.0f, 2.0f));
	camera.SetView(glm::vec3(-2.0f, 2.0f, -2.0f), glm::vec3(0.5f, -0.5f, 0.5f));
	camera.AttachToWindow(window);

	std::cout << "Default camera viewpoint:" << std::endl;
	std::cout << "  Position  = (" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ")" << std::endl;
	std::cout << "  Direction = (" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")" << std::endl;

	while (!glfwWindowShouldClose(window))
	{
		// Clear both buffers before drawing the next frame.
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);
		const glm::mat4 lightModel = glm::translate(glm::mat4(1.0f), lightPos);
		const glm::mat4 floorModel(1.0f);

		lightShader.Activate();
		glUniformMatrix4fv(glGetUniformLocation(lightShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(lightModel));

		shaderProgram.Activate();
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, "model"), 1, GL_FALSE, glm::value_ptr(floorModel));
		glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

		// Apply input before uploading the camera matrix for this frame.
		camera.Inputs(window);
		camera.UpdateMatrix(45.0f, 0.1f, 50.0f);

		std::ostringstream titleStream;
		titleStream << std::fixed << std::setprecision(2)
			<< "12_mesh | Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
			<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
		glfwSetWindowTitle(window, titleStream.str().c_str());
		// Draw uploads camera uniforms, binds textures, and submits the index buffer.
		floor.Draw(shaderProgram, camera);
		light.Draw(lightShader, camera);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	for (Texture& texture : textures)
	{
		texture.Delete();
	}
	floor.VAO.Delete();
	light.VAO.Delete();
	lightShader.Delete();
	shaderProgram.Delete();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

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
