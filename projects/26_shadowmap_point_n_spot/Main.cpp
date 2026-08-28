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

constexpr const char* appName = "26_shadowmap_point_n_spot";

constexpr unsigned int width  = 800;
constexpr unsigned int height = 800;

// Controls the gamma function
constexpr float gamma = 2.2f;

// The model pass and the skybox pass build their projections separately, so
// these must stay shared -- if they drift apart the horizon no longer lines up.
constexpr float fovDegrees = 45.0f;
constexpr float nearPlane  = 0.1f;
constexpr float farPlane   = 100.0f;

// Authored ~2000x this scene's units, so it is rescaled into its rest pose at
// load. With the floor gone the island is its own receiver: the machines and
// balloon sit above its deck, which is what keeps the shadow pass meaningful.
constexpr float islandScale = 0.0005f;

// A real world-space POSITION now, not just a direction: spot and point both
// measure distance from it. Sits above the balloon (top y = 0.71) and off-axis.
// The directional path still reads it through normalize(), so it is unchanged.
constexpr glm::vec3 lightPosition(0.6f, 1.0f, 0.6f);

// Uploaded straight into default.frag's lightMode uniform, so these numbers are
// half of a contract -- change one side and change the other.
enum class LightMode : int
{
	Directional = 0,
	Spot        = 1,
	Point       = 2
};

const char* lightModeName(LightMode mode)
{
	switch (mode)
	{
	case LightMode::Spot:  return "spot (perspective, one 2D map)";
	case LightMode::Point: return "point (cubemap, six faces)";
	default:               return "directional (orthographic, one 2D map)";
	}
}

// Range the cubemap normalises distance against, and the far plane of its six
// views. Furthest island corner is ~2.75 from the light, so 5.0 clears it.
// The tutorial reuses the camera's 100 here; that wastes 95% of the range and
// rescales what default.frag's world-space bias means, so retune both together.
constexpr float shadowFarPlane  = 5.0f;
constexpr float shadowNearPlane = 0.05f;

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


// COMMENTED OUT -- the tutorial has no light marker either. Five sites go with
// it; search "marker cube". Verbatim from 11_light: drawn unlit, so only
// position is read and the other Vertex fields just satisfy the struct.
/*
Mesh createLightCubeMesh()
{
	const glm::vec3 zero(0.0f);
	const float s = 0.1f;
	std::vector<Vertex> vertices;
	const glm::vec3 corners[8] = {
		{ -s, -s,  s }, { -s, -s, -s }, {  s, -s, -s }, {  s, -s,  s },
		{ -s,  s,  s }, { -s,  s, -s }, {  s,  s, -s }, {  s,  s,  s } };
	for (const glm::vec3& c : corners)
		vertices.push_back({ c, zero, zero, glm::vec2(0.0f, 0.0f) });

	std::vector<GLuint> indices = {
		0, 1, 2,  0, 2, 3,   0, 4, 7,  0, 7, 3,
		3, 7, 6,  3, 6, 2,   2, 6, 5,  2, 5, 1,
		1, 5, 4,  1, 4, 0,   4, 5, 6,  4, 6, 7 };
	std::vector<Texture> none;
	return Mesh(vertices, indices, none);
}
*/

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

// --- shadow map debug overlay -------------------------------------------
// A screen-space quad authored directly in NDC, drawn as an overlay so the
// depth map can be inspected next to the scene it was rendered from.
struct QuadMesh
{
	unsigned int vao = 0;
	unsigned int vbo = 0;
};

QuadMesh createQuadMesh()
{
	// x, y, u, v -- two triangles covering NDC -1..1. framebuffer.vert reads
	// position at location 0 and texture coordinates at location 1.
	const float quadVertices[] =
	{
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f,

		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f,
		-1.0f,  1.0f,  0.0f, 1.0f
	};

	QuadMesh mesh;
	glGenVertexArrays(1, &mesh.vao);
	glGenBuffers(1, &mesh.vbo);
	glBindVertexArray(mesh.vao);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
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

		// The sky is colour, so it is sRGB-encoded like any other colour map.
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB8_ALPHA8,
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

// Overlays the shadow map bottom-left. Purely diagnostic: it answers whether the
// depth pass rasterised anything and how much of the map it fills, which is what
// makes every later shadow bug tractable.
void drawShadowMapOverlay(Shader& shader, const QuadMesh& mesh,
	unsigned int depthTexture, bool gammaCorrect)
{
	shader.Activate();

	// Square, because the map is square -- letting it stretch to the window's
	// aspect would misrepresent how the caster sits in the light's frustum.
	const int overlaySize = static_cast<int>((width < height ? width : height) / 3);
	glViewport(0, 0, overlaySize, overlaySize);

	// The quad is drawn last and must survive whatever the scene left in the
	// depth buffer, so the test comes off.
	glDisable(GL_DEPTH_TEST);

	// Culling is off by GL default and this program never enables it, but query
	// rather than assume: restoring a state you did not actually save is how a
	// debug overlay silently changes the scene it was added to inspect.
	const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);

	// Depth is linear data, not colour. Leaving the sRGB encode on would apply a
	// display curve to raw depth values and make the readout lie about them.
	glDisable(GL_FRAMEBUFFER_SRGB);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glUniform1i(glGetUniformLocation(shader.ID, "depthMap"), 0);

	glBindVertexArray(mesh.vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	// Restore everything this function changed, for the same reason the depth
	// pass has to restore its framebuffer: GL state is global and sticky.
	if (gammaCorrect)
		glEnable(GL_FRAMEBUFFER_SRGB);
	if (cullWasEnabled)
		glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glViewport(0, 0, width, height);
}

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
	
	glBindVertexArray(mesh.vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);


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
	// No geometry stage on the lit program: default.geom was a pass-through, and
	// one more interface to keep in sync once fragPosLight arrived. Omitting the
	// third argument passes nullptr, so default.vert feeds default.frag directly.
	Shader shaderProgram("default.vert", "default.frag");
	Shader skyboxShader("skybox.vert", "skybox.frag");
	Shader framebufferProgram("framebuffer.vert", "framebuffer.frag");
	// marker cube: unlit, emits lightColor flat with no shading applied to itself.
	//Shader lightShader("light.vert", "light.frag");
	Shader shadowMapProgram("shadowMap.vert", "shadowMap.frag");
	// The third argument is a GEOMETRY shader -- the only one in the project. It
	// is the stage that can emit a triangle per face and route each copy.
	Shader shadowCubeMapProgram("shadowCubeMap.vert", "shadowCubeMap.frag", "shadowCubeMap.geom");
	// Reuses framebuffer.vert -- it already emits an NDC quad with UVs.
	Shader shadowDebugProgram("framebuffer.vert", "shadowDebug.frag");

	ShaderGuard shaderGuard(shaderProgram);
	ShaderGuard skyboxGuard(skyboxShader);
	ShaderGuard framebufferGuard(framebufferProgram);
	// marker cube
	//ShaderGuard lightGuard(lightShader);
	ShaderGuard shadowGuard(shadowMapProgram);
	ShaderGuard shadowCubeGuard(shadowCubeMapProgram);
	ShaderGuard shadowDebugGuard(shadowDebugProgram);


	// Still live: default.frag multiplies every lit fragment by this. Only the
	// marker cube's own copy of it is commented out below.
	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec3 lightPos = lightPosition;

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	skyboxShader.Activate();
	glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

	// marker cube
	//lightShader.Activate();
	//glUniform4f(glGetUniformLocation(lightShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);

	framebufferProgram.Activate();
	glUniform1i(glGetUniformLocation(framebufferProgram.ID, "screenTexture"), 0);
	glUniform1f(glGetUniformLocation(framebufferProgram.ID, "gamma"), gamma);


	// --- pipeline state -----------------------------------------------------
	// The other half of gamma correction. Inputs are decoded to linear when
	// sampled; this re-encodes the final colour to sRGB on write. Without it,
	// linearising the inputs alone just makes everything look too dark.
	glEnable(GL_FRAMEBUFFER_SRGB);

	glEnable(GL_DEPTH_TEST);

	// --- assets -------------------------------------------------------------
	// Forward slashes throughout: Model resolves its companion .bin and texture
	// paths by string-slicing this one.
	const std::array<std::string, 6> facesCubemap =
	{
		assetDirectory + "/Skybox/sky_42/right.png",
		assetDirectory + "/Skybox/sky_42/left.png",
		assetDirectory + "/Skybox/sky_42/top.png",
		assetDirectory + "/Skybox/sky_42/bottom.png",
		assetDirectory + "/Skybox/sky_42/front.png",
		assetDirectory + "/Skybox/sky_42/back.png"
	};

	const std::string islandPath = assetDirectory + "/Models/island/scene.gltf";

	requireFile(islandPath, "island");

	// "Object_19" is the model's own sky: a sphere ~36000 units across. Nothing
	// culls back faces here, so loading it draws that shell over the whole frame
	// and hides our skybox. Skipped at load, so it costs no units or geometry.
	Model islandModel(islandPath.c_str(), 1, {}, { "Object_19" });

	// The island is authored around +/-2900 units wide; at 0.0005 it spans about
	// 2.9 x 1.2 x 0.75 world units, which is deliberately close to the footprint
	// the old 2x2 floor had. That is what lets the light frustum below keep the
	// numbers it was tuned to.
	glm::mat4 restPose = glm::mat4(1.0f);
	restPose = glm::scale(restPose, glm::vec3(islandScale));
	islandModel.ApplyTransform(restPose);

	// marker cube
	//Mesh lightCube = createLightCubeMesh();

	const SkyboxMesh skybox = createSkyboxMesh();
	const QuadMesh debugQuad = createQuadMesh();
	const unsigned int cubemapTexture = loadCubemap(facesCubemap);

	// Framebuffer for Shadow Map
	unsigned int shadowMapFBO;
	glGenFramebuffers(1, &shadowMapFBO);
	
	// Texture for Shadow Map FBO
	unsigned int shadowMapWidth = 2048, shadowMapHeight = 2048;
	unsigned int shadowMap;
	glGenTextures(1, &shadowMap);
	glBindTexture(GL_TEXTURE_2D, shadowMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	// Prevents darkness outside the frustrum
	float clampColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, clampColor);

	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
	// Needed since we don't touch the color buffer
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// --- Framebuffer for the point light's cubemap shadow map ---------------
	// A separate depth target: the one above is a flat GL_TEXTURE_2D, and a point
	// light needs somewhere to put six views.
	unsigned int pointShadowMapFBO;
	glGenFramebuffers(1, &pointShadowMapFBO);

	unsigned int depthCubemap;
	glGenTextures(1, &depthCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

	// Six faces, allocated separately but all one texture object -- which is what
	// lets the geometry shader treat them as six layers of one attachment.
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
			shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// CLAMP_TO_EDGE, not the 2D map's white CLAMP_TO_BORDER: that border makes
	// "outside the frustum" read as lit, and a cubemap has no outside. R is the
	// third axis a samplerCube indexes with; a 2D map has no such parameter.
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, pointShadowMapFBO);
	// glFramebufferTexture, NOT ...Texture2D: the 2D form attaches one image, this
	// attaches the cubemap as a LAYERED one, which is what gl_Layer selects
	// between. Use the 2D form and five of the six faces are never written.
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		throw std::runtime_error("Point-light shadow framebuffer is incomplete");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// --- the directional light's matrix ------------------------------------
	// Box sized to the scene (corners reach 1.68): a loose box gives a blocky
	// shadow, a tight near/far spends depth precision on geometry not empty space.
	// Outside 16..20 is clipped and stops casting; the island spans 16.32..19.68.

	// Eye = normalize(lightPos) * 18. 25 wrote 20.0f * lightPos, which only worked
	// while |lightPos| was ~0.87; at 1.31 it lands at 26.2, behind the far plane.
	constexpr float directionalDistance = 18.0f;
	glm::mat4 orthgonalProjection = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 16.0f, 20.0f);
	glm::mat4 directionalView = glm::lookAt(glm::normalize(lightPos) * directionalDistance,
		glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 directionalProjection = orthgonalProjection * directionalView;

	// --- the spotlight's matrix --------------------------------------------
	// glm::perspective for glm::ortho is the whole of the spotlight half: FBO,
	// depth pass, shader and lookup are reused untouched. A spot cone IS a
	// perspective frustum -- rays from a point, which ortho cannot model.
	glm::mat4 spotProjection =
		glm::perspective(glm::radians(90.0f), 1.0f, shadowNearPlane, shadowFarPlane) *
		glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	// Axis of the cone, handed to default.frag so the lit cone and this frustum
	// cannot drift apart. See the note at computeSpotLightColor().
	const glm::vec3 spotDirection = glm::normalize(glm::vec3(0.0f) - lightPos);

	// --- the point light's six matrices ------------------------------------
	// One 90-degree view per axis; 90 is not a tuning choice, since six square
	// frustums at exactly 90 tile the sphere with no gap or overlap. The up
	// vectors are standard; +Y/-Y differ or lookAt would be degenerate.
	glm::mat4 shadowProj =
		glm::perspective(glm::radians(90.0f), 1.0f, shadowNearPlane, shadowFarPlane);

	std::array<glm::mat4, 6> shadowTransforms =
	{
		shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	// Uploaded element by element: querying "shadowMatrices[i]" per element is the
	// form that cannot go wrong across drivers.
	shadowCubeMapProgram.Activate();
	for (int i = 0; i < 6; i++)
	{
		const std::string name = "shadowMatrices[" + std::to_string(i) + "]";
		glUniformMatrix4fv(glGetUniformLocation(shadowCubeMapProgram.ID, name.c_str()),
			1, GL_FALSE, glm::value_ptr(shadowTransforms[i]));
	}
	// Must match what default.frag multiplies back out, or comparisons are scaled.
	glUniform3f(glGetUniformLocation(shadowCubeMapProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
	glUniform1f(glGetUniformLocation(shadowCubeMapProgram.ID, "shadowFarPlane"), shadowFarPlane);

	// The lit program needs the same constants on its side of the comparison.
	shaderProgram.Activate();
	glUniform1f(glGetUniformLocation(shaderProgram.ID, "shadowFarPlane"), shadowFarPlane);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "spotDirection"), spotDirection.x, spotDirection.y, spotDirection.z);

	// Unit 2 for the 2D map, 3 for the cubemap. 0 and 1 are Mesh::Draw's
	// diffuse0/specular0, and an unset sampler defaults to 0 -- which reads a
	// colour map as depth rather than failing. The two cannot share a unit:
	// sampling a 2D binding through a samplerCube is undefined, and both stay bound.
	glUniform1i(glGetUniformLocation(shaderProgram.ID, "shadowMap"), 2);
	glUniform1i(glGetUniformLocation(shaderProgram.ID, "shadowCubeMap"), 3);



	// --- camera -------------------------------------------------------------
	// The island is 2.9 units wide and a 45-degree FOV covers 0.414 of half-height
	// per unit of distance, so the decks need ~4 units of standoff. Raised on Y to
	// look down onto the decks, where the machines' cast shadows are visible.
	const glm::vec3 initialCameraPosition(1.9f, 2.2f, 1.7f);
	Camera camera(width, height, initialCameraPosition);
	camera.LookAt(initialCameraPosition, glm::vec3(-0.5f, -0.5f, -0.65f));
	camera.AttachToWindow(window.get());

	// --- render loop --------------------------------------------------------
	// Lighting model, flipped live with B so the two can be compared on the
	// same frame. Both lit programs share default.frag, so both are told.
	bool useBlinnPhong = true;
	bool blinnKeyWasDown = false;

	// T hides the island from the camera pass while it keeps casting. With the
	// floor gone it is its own receiver, so this now empties the scene rather
	// than leaving a lone shadow the way it did in 25.
	bool showIsland = true;
	bool showIslandKeyWasDown = false;

	// M overlays the shadow map in the corner so the depth pass can be seen.
	bool showShadowMap = false;
	bool shadowMapKeyWasDown = false;

	// G toggles the output encoding so the difference is visible side by side.
	bool gammaCorrect = true;
	bool gammaKeyWasDown = false;

	// L cycles directional -> spot -> point. The tutorial swaps these by
	// commenting calls in and out, which needs a rebuild each time. All three are
	// built up front here, so the switch costs only the branch below.
	LightMode lightMode = LightMode::Directional;
	bool lightModeKeyWasDown = false;
	std::cout << "Light: " << lightModeName(lightMode) << " (L to cycle)" << std::endl;

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

		// Depth testing needed for Shadow Map
		glEnable(GL_DEPTH_TEST);

		// Both branches render at the MAP's resolution, not the window's: the
		// viewport must match the attachment or the pass covers a fraction of it.
		glViewport(0, 0, shadowMapWidth, shadowMapHeight);

		// Neither branch is guarded by T -- that key is about what the camera sees.
		// An empty map reads as "nothing occludes anything" rather than as an error.
		if (lightMode == LightMode::Point)
		{
			// ONE draw call fills all six faces: the geometry shader re-emits each
			// triangle per face. One geometry pass instead of six, paid for with a
			// 6x fragment load.
			glBindFramebuffer(GL_FRAMEBUFFER, pointShadowMapFBO);
			glClear(GL_DEPTH_BUFFER_BIT);
			islandModel.Draw(shadowCubeMapProgram, camera);
		}
		else
		{
			// Directional and spot share this whole branch; the only difference is
			// which matrix was uploaded, which is the point.
			const glm::mat4& lightProjection = (lightMode == LightMode::Spot)
				? spotProjection
				: directionalProjection;

			shadowMapProgram.Activate();
			glUniformMatrix4fv(glGetUniformLocation(shadowMapProgram.ID, "lightProjection"),
				1, GL_FALSE, glm::value_ptr(lightProjection));

			// The SAME matrix on the lit program: default.vert builds fragPosLight
			// from it, and the comparison only means anything if both passes agree.
			// Uniforms belong to a program, so the line above does not cover this.
			shaderProgram.Activate();
			glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, "lightProjection"),
				1, GL_FALSE, glm::value_ptr(lightProjection));

			glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
			glClear(GL_DEPTH_BUFFER_BIT);
			islandModel.Draw(shadowMapProgram, camera);
		}

		// Both are sticky global state: without restoring them the rest of the
		// frame keeps rendering into this depth-only FBO at 2048x2048, and every
		// colour write is discarded because it has no colour attachment.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);

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

		// See the showIsland declaration above.
		const bool showIslandKeyDown = glfwGetKey(window.get(), GLFW_KEY_T) == GLFW_PRESS;
		if (showIslandKeyDown && !showIslandKeyWasDown)
		{
			showIsland = !showIsland;
			std::cout << "Island model: " << (showIsland ? "shown" : "hidden") << std::endl;
		}
		showIslandKeyWasDown = showIslandKeyDown;

		const bool lightModeKeyDown = glfwGetKey(window.get(), GLFW_KEY_L) == GLFW_PRESS;
		if (lightModeKeyDown && !lightModeKeyWasDown)
		{
			lightMode = static_cast<LightMode>((static_cast<int>(lightMode) + 1) % 3);
			std::cout << "Light: " << lightModeName(lightMode) << std::endl;
		}
		lightModeKeyWasDown = lightModeKeyDown;

		const bool shadowMapKeyDown = glfwGetKey(window.get(), GLFW_KEY_M) == GLFW_PRESS;
		if (shadowMapKeyDown && !shadowMapKeyWasDown)
		{
			showShadowMap = !showShadowMap;
			std::cout << "Shadow map overlay: " << (showShadowMap ? "on" : "off") << std::endl;
		}
		shadowMapKeyWasDown = shadowMapKeyDown;

		camera.Inputs(window.get());
		camera.UpdateMatrix(fovDegrees, nearPlane, farPlane);
		glfwSetWindowTitle(window.get(), formatTitle(fps, ms, camera).c_str());

		glClearColor(pow(0.07f, gamma), pow(0.13f, gamma), pow(0.17f, gamma), 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Uploaded per frame so the B toggle takes effect immediately. A uniform
		// belongs to a program, so each one must be Activated before it is set.
		shaderProgram.Activate();
		glUniform1i(glGetUniformLocation(shaderProgram.ID, "useBlinnPhong"), useBlinnPhong);
		glUniform1i(glGetUniformLocation(shaderProgram.ID, "lightMode"), static_cast<int>(lightMode));

		// Unit bindings are global state, so both maps are re-bound each frame.
		// Both, not just the active one: default.frag declares both samplers
		// whatever lightMode does, and reading a unit with nothing of its type bound
		// is undefined -- it works on one driver and renders black on another.
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, shadowMap);
		glActiveTexture(GL_TEXTURE0 + 3);
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

		// The only lit draw left, and the only one T guards -- the depth pass is
		// not, so T hides the island while it keeps writing the depth map.
		if (showIsland)
			islandModel.Draw(shaderProgram, camera);

		//lightCube.Draw(lightShader, camera,	glm::translate(glm::mat4(1.0f), lightPosition));

		drawSkybox(skyboxShader, camera, skybox, cubemapTexture);

		if (showShadowMap)
			drawShadowMapOverlay(shadowDebugProgram, debugQuad, shadowMap, gammaCorrect);

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