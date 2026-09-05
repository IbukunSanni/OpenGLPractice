#pragma once
#include "Mesh.h"
#include <json/json.h>

using json = nlohmann::json;

// Loads a static glTF 2.0 model (mesh geometry + textures) from disk and
// renders every mesh it contains via Mesh::Draw().
class Model {
public:
	// Loads a model from a file and stores the information in 'data', 'JSON', and 'file'
	// skipNodeNames: glTF nodes to leave out of the load, with their children.
	// Downloaded scenes often ship a stand-in sky sphere that would hide our own
	// skybox. The loader cannot tell it apart, so the caller names it.
	Model(const char* file, unsigned int instancing = 1, std::vector<glm::mat4> instanceMatrix = {},
		std::vector<std::string> skipNodeNames = {});

	// Draws every mesh in the model using its own precomputed node transform.
	// The T/R/S args place the whole model in the world and default to identity,
	// so one loaded Model can be drawn at many positions (e.g. the windows).
	void Draw(
		Shader& shader,
		Camera& camera,
		glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f),
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f)
	);

	// Returns the center of the model's transformed world-space bounds.
	glm::vec3 GetWorldCenter() const;

	// Draws at an arbitrary world transform. Unlike the T/R/S overload above,
	// which is applied inside the glTF node chain and reinterpreted by it, this
	// goes after the node matrix and so means plain world units.
	void Draw(Shader& shader, Camera& camera, const glm::mat4& worldTransform);

	// Bakes a transform into the model's rest pose, so Draw() treats the result
	// as if that were the model as loaded. Cumulative: call twice, both apply.
	void ApplyTransform(const glm::mat4& m);

private:
	// Variables for easy access
	const char* file;
	std::vector<unsigned char> data;
	json JSON;

	std::vector<Mesh> meshes;
	std::vector<glm::vec3> translationsMeshes;
	std::vector<glm::quat> rotationsMeshes;
	std::vector<glm::vec3> scalesMeshes;
	std::vector<glm::mat4> matricesMeshes;

	// Kept so loadMesh() can hand them to every Mesh it builds. A Mesh wires its
	// instance VBO and attribute divisors in its own constructor, so these must
	// already be set by the time traverseNode() runs.
	unsigned int instancing = 1;
	std::vector<glm::mat4> instanceMatrix;

	// Node names traverseNode() refuses to descend into. See the constructor.
	std::vector<std::string> skipNodeNames;

	// Prevents textures from being loaded twice
	std::vector<std::string> loadedTexName;
	std::vector<Texture> loadedTex;

	// Loads a single mesh by its index
	void loadMesh(unsigned int indMesh);

	// Traverses a node recursively, so it essentially traverses all connected nodes
	void traverseNode(unsigned int nextNode, glm::mat4 matrix = glm::mat4(1.0f));

	// Gets the binary data from a file
	std::vector<unsigned char> getData();

	// Interprets the binary data into floats, indices, and textures
	std::vector<float> getFloats(json accessor);
	std::vector<GLuint> getIndices(json accessor);
	std::vector<Texture> getTextures(const json& primitive);

	// Assembles all the floats into vertices
	std::vector<Vertex> assembleVertices
	(
		std::vector<glm::vec3> positions,
		std::vector<glm::vec3> normals,
		std::vector<glm::vec2> texUVs
	);

	// Helps with the assembly from above by grouping floats
	std::vector<glm::vec2> groupFloatsVec2(std::vector<float> floatVec);
	std::vector<glm::vec3> groupFloatsVec3(std::vector<float> floatVec);
	std::vector<glm::vec4> groupFloatsVec4(std::vector<float> floatVec);
};
