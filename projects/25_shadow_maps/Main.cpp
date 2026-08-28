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

constexpr const char* appName = "25_shadow_maps";

constexpr unsigned int width  = 800;
constexpr unsigned int height = 800;

// Controls the gamma function
constexpr float gamma = 2.2f;

// The model pass and the skybox pass build their projections separately, so
// these must stay shared -- if they drift apart the horizon no longer lines up.
constexpr float fovDegrees = 45.0f;
constexpr float nearPlane  = 0.1f;
constexpr float farPlane   = 100.0f;

// Both models are authored far larger than the ~29 units of view height at
// the camera distance below, so each is rescaled into its rest pose at load.

// A large flat plane is the case that makes the Blinn-Phong toggle visible: at
// grazing angles Phong's reflection vector swings more than 90 degrees from the
// viewer and the highlight cuts off, while Blinn's halfway vector does not.
// Scene reproduced from 11_light: a 2x2 plane at the origin and a 0.2 marker
// cube at the light. Geometry is used exactly as authored there; placement is
// done with model matrices, same as that lesson.
constexpr glm::vec3 lightPosition(0.5f, 0.5f, 0.5f);

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


// A quad in the XZ plane with normals pointing up.
//
// Winding matters here: the project culls with GL_FRONT + GL_CW, which keeps
// triangles that project counter-clockwise. A triangle whose geometric normal
// faces the viewer projects counter-clockwise, so this order leaves the top
// face visible from above. Reverse the indices and the floor vanishes.
// Floor plane, taken verbatim from 11_light. That file lists its attributes as
// position / colour / texcoord / normal; this project's Vertex struct orders
// them position / normal / colour / texUV, so they are reordered, not changed.
Mesh createFloorMesh(std::vector<Texture>& textures)
{
	const glm::vec3 up(0.0f, 1.0f, 0.0f);
	const glm::vec3 black(0.0f, 0.0f, 0.0f);

	std::vector<Vertex> vertices = {
		{ glm::vec3(-1.0f, 0.0f,  1.0f), up, black, glm::vec2(0.0f, 0.0f) },
		{ glm::vec3(-1.0f, 0.0f, -1.0f), up, black, glm::vec2(0.0f, 1.0f) },
		{ glm::vec3( 1.0f, 0.0f, -1.0f), up, black, glm::vec2(1.0f, 1.0f) },
		{ glm::vec3( 1.0f, 0.0f,  1.0f), up, black, glm::vec2(1.0f, 0.0f) },
	};
	std::vector<GLuint> indices = { 0, 1, 2, 0, 2, 3 };
	return Mesh(vertices, indices, textures);
}

// Light marker cube, vertices and indices verbatim from 11_light. It renders
// unlit through light.frag, so normal, colour and UV are never read -- only
// position matters, and the struct's other fields are filled to satisfy it.
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

// Drawn after the model so early-z can discard every fragment the model already
// covers: the cube writes depth 1.0, which loses to any real geometry.
// Overlays the shadow map in the bottom-left corner. Purely diagnostic: it
// answers "did the depth pass actually rasterise anything, and how much of the
// map does it fill", which is the question that makes every later shadow bug
// tractable.
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
	// One lit shader for the model, one unlit shader that samples the cubemap
	// for the sky behind it.
	//
	// No geometry stage: default.geom was a pass-through, and once default.vert
	// started emitting fragPosLight for the shadow lookup it became one more
	// interface to keep in sync for no gain. Omitting the third argument passes
	// nullptr, so default.vert feeds default.frag directly.
	Shader shaderProgram("default.vert", "default.frag");
	Shader skyboxShader("skybox.vert", "skybox.frag");
	Shader framebufferProgram("framebuffer.vert", "framebuffer.frag");
	// Unlit: emits lightColor flat, with no shading applied to itself.
	Shader lightShader("light.vert", "light.frag");
	Shader shadowMapProgram("shadowMap.vert", "shadowMap.frag");
	// Reuses framebuffer.vert -- it already emits an NDC quad with UVs.
	Shader shadowDebugProgram("framebuffer.vert", "shadowDebug.frag");

	ShaderGuard shaderGuard(shaderProgram);
	ShaderGuard skyboxGuard(skyboxShader);
	ShaderGuard framebufferGuard(framebufferProgram);
	ShaderGuard lightGuard(lightShader);
	ShaderGuard shadowGuard(shadowMapProgram);
	ShaderGuard shadowDebugGuard(shadowDebugProgram);


	const glm::vec4 lightColor(1.0f, 1.0f, 1.0f, 1.0f);
	// The light lives inside the cube, so the cube reads as its source.
	const glm::vec3 lightPos = lightPosition;

	shaderProgram.Activate();
	glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	skyboxShader.Activate();
	glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

	lightShader.Activate();
	glUniform4f(glGetUniformLocation(lightShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);

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
		assetDirectory + "/Skybox/space/right.png",
		assetDirectory + "/Skybox/space/left.png",
		assetDirectory + "/Skybox/space/top.png",
		assetDirectory + "/Skybox/space/bottom.png",
		assetDirectory + "/Skybox/space/front.png",
		assetDirectory + "/Skybox/space/back.png"
	};

	const std::string mach6Path = assetDirectory + "/Models/mach_6/scene.gltf";
	const std::string planksPath = assetDirectory + "/Textures/planks.png";
	const std::string planksSpecPath = assetDirectory + "/Textures/planksSpec.png";
	
	requireFile(mach6Path, "mach_6");
	requireFile(planksPath, "planks");
	requireFile(planksSpecPath, "planks specular");

	// Shared by floor and cube. Mesh::Draw numbers them into the diffuse0 /
	// specular0 samplers default.frag expects.
	std::vector<Texture> planksTextures = {
		Texture(planksPath.c_str(), "diffuse", 0),
		Texture(planksSpecPath.c_str(), "specular", 1)
	};
	Model mach6Model(mach6Path.c_str());

	glm::mat4 restPose = glm::mat4(1.0f);
	restPose = glm::scale(restPose, glm::vec3(0.05f));
	mach6Model.ApplyTransform(restPose);

	Mesh floorMesh = createFloorMesh(planksTextures);
	Mesh lightCube = createLightCubeMesh();	

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


	// Matrices needed for the light's perspective
	// Sized to THIS scene, not the tutorial's. The floor spans +/-1 and the car
	// is scaled to 0.05, so the whole scene fits inside ~2 units. The tutorial's
	// 70-unit box would spread that across ~30 of the map's 2048 texels and give
	// an unreadably blocky shadow; a tight box is the single biggest factor in
	// shadow map quality. Far only has to clear the light distance (~17) plus
	// the scene radius.
	glm::mat4 orthgonalProjection = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 25.0f);
	glm::mat4 lightView = glm::lookAt(20.0f * lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 lightProjection = orthgonalProjection * lightView;

	shadowMapProgram.Activate();
	glUniformMatrix4fv(glGetUniformLocation(shadowMapProgram.ID, "lightProjection"), 1, GL_FALSE, glm::value_ptr(lightProjection));

	// The SAME matrix the depth pass rendered with, on the lit program too:
	// default.vert builds fragPosLight from it, and the comparison is only
	// meaningful if both passes agree on the light's frustum. A uniform belongs
	// to a program, so setting it on shadowMapProgram does nothing for this one.
	// Uploaded once -- the light does not move.
	shaderProgram.Activate();
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, "lightProjection"), 1, GL_FALSE, glm::value_ptr(lightProjection));

	// Texture unit 2 for the depth map. Units 0 and 1 are claimed by the
	// diffuse0/specular0 pair Mesh::Draw rebinds on every draw, and an unset
	// sampler defaults to unit 0 -- which would silently read the planks colour
	// map as depth rather than failing. The unit number never changes, so only
	// the binding below has to be re-established per frame.
	glUniform1i(glGetUniformLocation(shaderProgram.ID, "shadowMap"), 2);



	// --- camera -------------------------------------------------------------
	// Framed on the model's world-space bounds center, which already reflects
	// the scale baked in above.
	const glm::vec3 initialCameraPosition(0.0f, 0.0f, 2.0f);
	Camera camera(width, height, initialCameraPosition);
	camera.LookAt(initialCameraPosition, glm::vec3(0.0f, 0.0f, 0.0f));
	camera.AttachToWindow(window.get());

	// --- render loop --------------------------------------------------------
	// Lighting model, flipped live with B so the two can be compared on the
	// same frame. Both lit programs share default.frag, so both are told.
	bool useBlinnPhong = true;
	bool blinnKeyWasDown = false;

	// TEMP (shadow-map work): T hides the car so the floor and the light cube
	// are unobstructed while the depth pass is being built. Remove this, the
	// key block below and the `if (showCar)` guard once shadows are working.
	bool showCar = true;
	bool showCarKeyWasDown = false;

	// M overlays the shadow map in the corner so the depth pass can be seen.
	bool showShadowMap = false;
	bool shadowMapKeyWasDown = false;

	// G toggles the output encoding so the difference is visible side by side.
	bool gammaCorrect = true;
	bool gammaKeyWasDown = false;

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

		// Preparations for the Shadow Map
		glViewport(0, 0, shadowMapWidth, shadowMapHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Draw scene for shadow map.
		// Deliberately NOT guarded by T. The car always casts; T hides it from the
		// CAMERA pass only, which leaves its shadow alone on the floor with nothing
		// above it -- the view that makes the shadow inspectable while the lookup is
		// being tuned, since from the default camera the car occludes most of it.
		// Guarding this too would empty the map and take the shadow with it.
		mach6Model.Draw(shadowMapProgram, camera);

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

		// TEMP (shadow-map work): see the showCar declaration above.
		const bool showCarKeyDown = glfwGetKey(window.get(), GLFW_KEY_T) == GLFW_PRESS;
		if (showCarKeyDown && !showCarKeyWasDown)
		{
			showCar = !showCar;
			std::cout << "Car model: " << (showCar ? "shown" : "hidden") << std::endl;
		}
		showCarKeyWasDown = showCarKeyDown;

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

		// Texture unit bindings are global state, not part of the program, so the
		// depth map is re-bound to unit 2 each frame rather than once at startup.
		glActiveTexture(GL_TEXTURE0 + 2);
		glBindTexture(GL_TEXTURE_2D, shadowMap);

		// Both ride the existing pipeline -- default.frag and the Blinn-Phong
		// toggle apply with no extra shader.
		// Culling off for the plane: it is a single quad with one winding, so the
		// underside would otherwise vanish the moment the camera drops below it.
		// TEMP (shadow-map work): guarded by the T toggle; see above. This is the
		// ONLY guarded draw -- the depth pass above is not -- so T removes the car
		// from view while it keeps casting.
		if (showCar)
			mach6Model.Draw(shaderProgram, camera);
	
		floorMesh.Draw(shaderProgram, camera, glm::mat4(1.0f));
	
		lightCube.Draw(lightShader, camera,	glm::translate(glm::mat4(1.0f), lightPosition));

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