#include "Mesh.h"

#include <exception>
#include <iomanip>   // std::setprecision
#include <iostream>
#include <sstream>   // std::ostringstream for building the window title
#include <string>
#include <vector>

constexpr unsigned int width  = 800;
constexpr unsigned int height = 800;

// ── Floor geometry ────────────────────────────────────────────────────────────
// A flat 2x2 quad centred at the origin lying on the XZ plane (Y = 0).
// Each Vertex carries: position, normal (pointing straight up), color (white),
// and a UV coordinate that maps the planks texture once across the quad.
const std::vector<Vertex> floorVertices =
{
	//                  Position                      Normal (up)              Color (white)            UV
	Vertex{glm::vec3(-1.0f, 0.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(0.0f, 0.0f)}, // Front-left
	Vertex{glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(0.0f, 1.0f)}, // Back-left
	Vertex{glm::vec3( 1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(1.0f, 1.0f)}, // Back-right
	Vertex{glm::vec3( 1.0f, 0.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec2(1.0f, 0.0f)}  // Front-right
};

// Two triangles split the quad along the diagonal 0-2.
const std::vector<GLuint> floorIndices =
{
	0, 1, 2, // Triangle 1: front-left → back-left → back-right
	0, 2, 3  // Triangle 2: front-left → back-right → front-right
};

// ── Light marker geometry ─────────────────────────────────────────────────────
// A tiny 0.2-unit cube rendered with the flat light shader so it always appears
// as a bright white block regardless of the scene lighting.
// Only positions are needed — no normals/colors/UVs for a flat-shaded object.
const std::vector<Vertex> lightVertices =
{
	// Bottom face
	Vertex{glm::vec3(-0.1f, -0.1f,  0.1f)}, // 0: bottom front-left
	Vertex{glm::vec3(-0.1f, -0.1f, -0.1f)}, // 1: bottom back-left
	Vertex{glm::vec3( 0.1f, -0.1f, -0.1f)}, // 2: bottom back-right
	Vertex{glm::vec3( 0.1f, -0.1f,  0.1f)}, // 3: bottom front-right
	// Top face
	Vertex{glm::vec3(-0.1f,  0.1f,  0.1f)}, // 4: top front-left
	Vertex{glm::vec3(-0.1f,  0.1f, -0.1f)}, // 5: top back-left
	Vertex{glm::vec3( 0.1f,  0.1f, -0.1f)}, // 6: top back-right
	Vertex{glm::vec3( 0.1f,  0.1f,  0.1f)}  // 7: top front-right
};

// 12 triangles (2 per face × 6 faces) covering the full cube surface.
const std::vector<GLuint> lightIndices =
{
	0, 1, 2,  0, 2, 3,  // Bottom
	0, 4, 7,  0, 7, 3,  // Front
	3, 7, 6,  3, 6, 2,  // Right
	2, 6, 5,  2, 5, 1,  // Back
	1, 5, 4,  1, 4, 0,  // Left
	4, 5, 6,  4, 6, 7   // Top
};


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

	GLFWwindow* window = glfwCreateWindow(width, height, "12_mesh", nullptr, nullptr);
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

	// ── Textures ──────────────────────────────────────────────────────────────
	// Build absolute paths so the program finds the assets regardless of the
	// working directory Visual Studio sets at launch.
	const std::string textureDirectory =
		R"(C:\Users\Ibukunoluwa\Documents\Coding\C-C++\OpenGL-VSstudio\OpenGLPractice\Assets\Textures)";
	const std::string planksPath     = textureDirectory + R"(\planks.png)";
	const std::string planksSpecPath = textureDirectory + R"(\planksSpec.png)";

	// Texture unit 0 → diffuse map  (base color, sampled as "diffuse" in the shader)
	// Texture unit 1 → specular map (shininess mask, sampled as "specular" in the shader)
	std::vector<Texture> textures
	{
		Texture(planksPath.c_str(),     "diffuse",  0, GL_UNSIGNED_BYTE),
		Texture(planksSpecPath.c_str(), "specular", 1, GL_UNSIGNED_BYTE)
	};

	// ── Shaders ───────────────────────────────────────────────────────────────
	// default.vert/frag — Phong lighting (diffuse + specular) used for the floor.
	// light.vert/frag   — flat white shader used for the light-marker cube.
	Shader shaderProgram("default.vert", "default.frag");
	Shader lightShader("light.vert",   "light.frag");

	// ── Mesh objects ──────────────────────────────────────────────────────────
	const std::vector<Texture> noTextures; // Light cube doesn't need textures
	Mesh floor(floorVertices, floorIndices, textures);     // Textured floor quad
	Mesh light(lightVertices, lightIndices, noTextures);   // Untextured light cube

	// ── One-time uniform uploads ──────────────────────────────────────────────
	// lightColor is pure white (1,1,1,1) and never changes, so upload it once
	// here rather than repeating the call every frame.
	const glm::vec4 lightColor(1.0f);

	lightShader.Activate();
	glUniform4f(glGetUniformLocation(lightShader.ID,   "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);

	// ── Depth testing ─────────────────────────────────────────────────────────
	// Ensures closer fragments overwrite farther ones, giving correct 3-D overlap.
	glEnable(GL_DEPTH_TEST);

	// ── Camera setup ──────────────────────────────────────────────────────────
	// Place the camera above-left of the floor, oriented toward its centre.
	// AttachToWindow stores the GLFW handle so Inputs() can poll keys each frame.
	Camera camera(width, height, glm::vec3(0.0f, 0.0f, 2.0f));
	camera.SetView(glm::vec3(-2.0f, 2.0f, -2.0f), glm::vec3(0.5f, -0.5f, 0.5f));
	camera.AttachToWindow(window);

	// Print the starting viewpoint so it is easy to reproduce in tests.
	std::cout << "Default camera viewpoint:" << std::endl;
	std::cout << "  Position  = (" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ")" << std::endl;
	std::cout << "  Direction = (" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")" << std::endl;

	// ── Render loop ───────────────────────────────────────────────────────────
	while (!glfwWindowShouldClose(window))
	{
		// Start each frame with a clean slate — clear color and depth.
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// ── Per-frame transforms ──────────────────────────────────────────────
		// The light stays fixed at (0.5, 0.5, 0.5); its model matrix is just a
		// translation. The floor sits at the world origin so its model is identity.
		const glm::vec3 lightPos(0.5f, 0.5f, 0.5f);
		const glm::mat4 lightModel = glm::translate(glm::mat4(1.0f), lightPos);
		const glm::mat4 floorModel(1.0f); // Identity — floor does not move

		// Upload the model matrix and light position to each shader program.
		lightShader.Activate();
		glUniformMatrix4fv(glGetUniformLocation(lightShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(lightModel));

		shaderProgram.Activate();
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, "model"),    1, GL_FALSE, glm::value_ptr(floorModel));
		glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

		// ── Camera update ─────────────────────────────────────────────────────
		// Inputs() reads WASD / mouse and moves the camera.
		// UpdateMatrix() recomputes proj * view and caches it inside the camera
		// so Mesh::Draw() can pass it to whichever shader it needs.
		camera.Inputs(window);
		camera.UpdateMatrix(45.0f, 0.1f, 50.0f); // 45° FOV, near=0.1, far=50

		// ── Window title HUD ──────────────────────────────────────────────────
		// Shows live camera position and orientation — useful for debugging.
		std::ostringstream titleStream;
		titleStream << std::fixed << std::setprecision(2)
			<< "12_mesh | Pos(" << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z << ") "
			<< "Dir(" << camera.Orientation.x << ", " << camera.Orientation.y << ", " << camera.Orientation.z << ")";
		glfwSetWindowTitle(window, titleStream.str().c_str());

		// ── Draw calls ────────────────────────────────────────────────────────
		// Mesh::Draw() activates the shader, binds textures, uploads the camera
		// matrix uniform, and issues the glDrawElements call.
		floor.Draw(shaderProgram, camera);
		light.Draw(lightShader,   camera);

		// Present the finished frame and process pending OS/input events.
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// ── Cleanup ───────────────────────────────────────────────────────────────
	// Delete GPU resources in reverse order of creation to avoid dangling refs.
	for (Texture& texture : textures)
		texture.Delete();
	floor.VAO.Delete();
	light.VAO.Delete();
	lightShader.Delete();
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
