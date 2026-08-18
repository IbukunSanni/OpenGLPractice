#include "Model.h"

#include <limits>
#include <stdexcept>

// TODO: for string constants need to be added, for ease of expalanation and more readability
namespace {
	// glTF 2.0 accessor component types. These are spec-defined values and
	// happen to be identical to the matching OpenGL enums.
	// See glTF 2.0 spec, 5.1.1 "accessor.componentType".
	constexpr unsigned int GLTF_COMPONENT_BYTE           = 5120;
	constexpr unsigned int GLTF_COMPONENT_UNSIGNED_BYTE  = 5121;
	constexpr unsigned int GLTF_COMPONENT_SHORT          = 5122;
	constexpr unsigned int GLTF_COMPONENT_UNSIGNED_SHORT = 5123;
	constexpr unsigned int GLTF_COMPONENT_UNSIGNED_INT   = 5125;
	constexpr unsigned int GLTF_COMPONENT_FLOAT          = 5126;

	// Reads 'count' tightly packed values of type T starting at 'beginningOfData'
	// and appends them as GLuints. sizeof(T) supplies the byte stride, so the
	// element width no longer has to be hard-coded at each call site.
	template <typename T>
	void readIndicesAs(
		const std::vector<unsigned char>& data,
		unsigned int beginningOfData,
		unsigned int count,
		std::vector<GLuint>& indices)
	{
		const std::size_t bytesRequired = static_cast<std::size_t>(count) * sizeof(T);
		if (beginningOfData > data.size() || bytesRequired > data.size() - beginningOfData)
			throw std::out_of_range("Index accessor reads past the end of the model buffer");

		indices.reserve(count);
		for (unsigned int i = 0; i < count; i++)
		{
			T value{};
			std::memcpy(&value, &data[beginningOfData + i * sizeof(T)], sizeof(T));
			indices.push_back(static_cast<GLuint>(value));
		}
	}
}

Model::Model(const char* file) {
	// Parse the glTF JSON and load its referenced binary buffer up front,
	// since every accessor lookup below reads out of these two members.
	std::string text = get_file_contents(file);
	JSON = json::parse(text);

	Model::file = file;
	data = getData();

	// Recursively load every mesh reachable from the active scene's root nodes.
	const unsigned int sceneIndex = JSON.value("scene", 0u);
	const json& rootNodes = JSON.at("scenes").at(sceneIndex).at("nodes");
	if (rootNodes.empty())
		throw std::runtime_error("The selected glTF scene has no root nodes");

	for (const json& rootNode : rootNodes)
		traverseNode(rootNode.get<unsigned int>());

}

void Model::Draw(Shader& shader, Camera& camera) {
	// Each mesh keeps its own world matrix, computed once in traverseNode().
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		meshes[i].Mesh::Draw(shader, camera, matricesMeshes[i]);
	}
}

glm::vec3 Model::GetWorldCenter() const {
	glm::vec3 minimum(std::numeric_limits<float>::max());
	glm::vec3 maximum(std::numeric_limits<float>::lowest());
	bool hasVertices = false;

	for (std::size_t meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
	{
		const glm::mat4& transform = matricesMeshes.at(meshIndex);
		for (const Vertex& vertex : meshes[meshIndex].vertices)
		{
			const glm::vec3 worldPosition = glm::vec3(transform * glm::vec4(vertex.position, 1.0f));
			minimum = glm::min(minimum, worldPosition);
			maximum = glm::max(maximum, worldPosition);
			hasVertices = true;
		}
	}

	if (!hasVertices)
		throw std::runtime_error("Cannot calculate the center of a model with no vertices");

	return (minimum + maximum) * 0.5f;
}
void Model::loadMesh(unsigned int indMesh) {
	// Look up the accessor indices this primitive uses for each attribute.
	const json& primitive = JSON["meshes"][indMesh]["primitives"][0];
	unsigned int posAccInd = primitive["attributes"]["POSITION"];
	unsigned int normalAccInd = primitive["attributes"]["NORMAL"];
	unsigned int texAccInd = primitive["attributes"]["TEXCOORD_0"];
	unsigned int indAccInd = primitive["indices"];

	std::vector<float> posVec = getFloats(JSON["accessors"][posAccInd]);
	std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
	std::vector<float> normalVec = getFloats(JSON["accessors"][normalAccInd]);
	std::vector<glm::vec3> normals = groupFloatsVec3(normalVec);
	std::vector<float> texVec = getFloats(JSON["accessors"][texAccInd]);
	std::vector<glm::vec2> texUVs = groupFloatsVec2(texVec);

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
	std::vector<GLuint> indices = getIndices(JSON["accessors"][indAccInd]);
	std::vector<Texture> textures = getTextures(primitive);

	// Combine the vertices, indices, and textures into a mesh
	meshes.push_back(Mesh(vertices, indices, textures));

}

void Model::traverseNode(unsigned int nextNode, glm::mat4 matrix) {
	// TODO: consider const reference
	// TODO: constant strings for JSON keys
	json node = JSON["nodes"][nextNode];

	glm::vec3 translation(0.0f);

	if (node.find("translation") != node.end()) {
		translation = glm::vec3(
			node["translation"][0],
			node["translation"][1],
			node["translation"][2]
		);
	}

	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	if (node.find("rotation") != node.end()) {
		// glTF stores quaternions as [x, y, z, w] (component order), but glm::quat's
		// constructor takes (w, x, y, z) - so we have to reorder on the way in:
		// node["rotation"][3] -> w, and [0],[1],[2] -> x,y,z respectively.
		rotation = glm::quat(
			node["rotation"][3],
			node["rotation"][0],
			node["rotation"][1],
			node["rotation"][2]
		);
	}

	glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
	if (node.find("scale") != node.end()) {
		scale = glm::vec3(
			node["scale"][0],
			node["scale"][1],
			node["scale"][2]
		);
	}

	glm::mat4 matNode = glm::mat4(1.0f);
	if (node.find("matrix") != node.end()) {
		if (node["matrix"].size() != 16)
			throw std::runtime_error("A glTF node matrix must contain exactly 16 values");

		float matValues[16];
		for (unsigned int i = 0; i < node["matrix"].size(); i++)
			matValues[i] = (node["matrix"][i]);
		matNode = glm::make_mat4(matValues);
	}

	// Initialize matrices
	glm::mat4 trans = glm::mat4(1.0f);
	glm::mat4 rot = glm::mat4(1.0f);
	glm::mat4 sca = glm::mat4(1.0f);

	// Use translation, rotation, and scale to change the initialized matrices
	trans = glm::translate(trans, translation);
	rot = glm::mat4_cast(rotation);
	sca = glm::scale(sca, scale);

	// Multiply all matrices together
	glm::mat4 matNextNode = matrix * matNode * trans * rot * sca;

	// Check if the node contains a mesh and if it does load it
	if (node.find("mesh") != node.end())
	{
		translationsMeshes.push_back(translation);
		rotationsMeshes.push_back(rotation);
		scalesMeshes.push_back(scale);
		matricesMeshes.push_back(matNextNode);

		loadMesh(node["mesh"]);
	}

	// Check if the node has children, and if it does, apply this function to them with the matNextNode
	if (node.find("children") != node.end())
	{
		for (unsigned int i = 0; i < node["children"].size(); i++)
			traverseNode(node["children"][i], matNextNode);
	}

}

// Reads the binary buffer referenced by the glTF's "buffers[0].uri" field.
// The URI is relative to the model file's own directory, not the CWD.
std::vector<unsigned char> Model::getData() {
	std::string bytesText;
	std::string uri = JSON["buffers"][0]["uri"];

	std::string fileStr = std::string(file);
	std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);
	bytesText = get_file_contents((fileDirectory + uri).c_str());

	std::vector<unsigned char> data(bytesText.begin(), bytesText.end());
	return data;

}

std::vector<float> Model::getFloats(json accessor) {
	std::vector<float> floatVec;

	unsigned int buffViewInd = accessor.at("bufferView");
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];

	json bufferView = JSON["bufferViews"][buffViewInd];
	// byteOffset is optional in glTF and defaults to 0. Reading it directly
	// throws type_error.302 on the many exporters that omit it.
	unsigned int byteOffset = bufferView.value("byteOffset", 0);

	// Interpret the type and store it into numPerVert
	unsigned int numPerVert;
	if (type == "SCALAR") numPerVert = 1;
	else if (type == "VEC2") numPerVert = 2;
	else if (type == "VEC3") numPerVert = 3;
	else if (type == "VEC4") numPerVert = 4;
	else throw std::invalid_argument("Type is invalid (not SCALAR, VEC2, VEC3, or VEC4)");


	// Go over all the bytes in the data at the correct place using the properties from above
	unsigned int beginningOfData = byteOffset + accByteOffset;
	unsigned int lengthOfData = count * 4 * numPerVert;
	if (beginningOfData > data.size() || lengthOfData > data.size() - beginningOfData)
		throw std::out_of_range("Vertex accessor reads past the end of the model buffer");

	for (unsigned int i = beginningOfData; i < beginningOfData + lengthOfData; i += 4)
	{
		unsigned char bytes[] = { data[i], data[i + 1], data[i + 2], data[i + 3] };
		float value;
		std::memcpy(&value, bytes, sizeof(float));
		floatVec.push_back(value);
	}

	return floatVec;

}

std::vector<GLuint> Model::getIndices(json accessor) {
	std::vector<GLuint> indices;

	unsigned int buffViewInd = accessor.at("bufferView");
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	unsigned int componentType = accessor["componentType"];

	json bufferView = JSON["bufferViews"][buffViewInd];
	// byteOffset is optional in glTF and defaults to 0 (see getFloats above).
	unsigned int byteOffset = bufferView.value("byteOffset", 0);

	unsigned int beginningOfData = byteOffset + accByteOffset;

	// Indices may be stored at one of three integer widths; reinterpret each
	// accordingly and normalise to GLuint.
	switch (componentType)
	{
	case GLTF_COMPONENT_UNSIGNED_INT:
		readIndicesAs<unsigned int>(data, beginningOfData, count, indices);
		break;

	case GLTF_COMPONENT_UNSIGNED_SHORT:
		readIndicesAs<unsigned short>(data, beginningOfData, count, indices);
		break;

	case GLTF_COMPONENT_UNSIGNED_BYTE:
		readIndicesAs<unsigned char>(data, beginningOfData, count, indices);
		break;

	default:
		throw std::invalid_argument(
			"Index componentType must be UNSIGNED_BYTE, UNSIGNED_SHORT, or UNSIGNED_INT");
	}
	
	return indices;

}

std::vector<Texture> Model::getTextures(const json& primitive) {
	std::vector<Texture> textures;

	const std::string fileDirectory =
		std::string(file).substr(0, std::string(file).find_last_of('/') + 1);

	auto imagePathFromTexture = [this](const json& textureInfo) -> std::string {
		if (textureInfo.find("index") == textureInfo.end())
			return {};

		const unsigned int textureIndex = textureInfo["index"];
		if (textureIndex >= JSON["textures"].size())
			return {};

		const json& texture = JSON["textures"][textureIndex];
		if (texture.find("source") == texture.end())
			return {};

		const unsigned int imageIndex = texture["source"];
		if (imageIndex >= JSON["images"].size())
			return {};

		return JSON["images"][imageIndex].value("uri", std::string{});
	};

	auto findImage = [this](const char* first, const char* second) -> std::string {
		for (const json& image : JSON["images"])
		{
			const std::string uri = image.value("uri", std::string{});
			if (uri.find(first) != std::string::npos || uri.find(second) != std::string::npos)
				return uri;
		}
		return {};
	};

	auto addTexture = [&](const std::string& path, const char* type) {
		if (path.empty())
			return;

		for (unsigned int i = 0; i < loadedTexName.size(); i++)
		{
			if (loadedTexName[i] == path)
			{
				Texture reusedTexture = loadedTex[i];
				reusedTexture.type = type;
				textures.push_back(reusedTexture);
				return;
			}
		}

		Texture texture((fileDirectory + path).c_str(), type, static_cast<GLuint>(loadedTex.size()));
		textures.push_back(texture);
		loadedTex.push_back(texture);
		loadedTexName.push_back(path);
	};

	std::string diffusePath;
	std::string specularPath;

	if (primitive.find("material") != primitive.end())
	{
		const unsigned int materialIndex = primitive["material"];
		if (materialIndex < JSON["materials"].size())
		{
			const json& material = JSON["materials"][materialIndex];
			const auto pbrIt = material.find("pbrMetallicRoughness");
			if (pbrIt != material.end())
			{
				const json& pbr = *pbrIt;
				const auto baseColorIt = pbr.find("baseColorTexture");
				if (baseColorIt != pbr.end())
					diffusePath = imagePathFromTexture(*baseColorIt);

				const auto metallicIt = pbr.find("metallicRoughnessTexture");
				if (metallicIt != pbr.end())
					specularPath = imagePathFromTexture(*metallicIt);
			}
		}
	}

	// Fall back for incomplete exports or assets that use semantic filenames.
	if (diffusePath.empty())
		diffusePath = findImage("baseColor", "diffuse");
	if (specularPath.empty())
		specularPath = findImage("metallicRoughness", "specular");

	addTexture(diffusePath, "diffuse");
	addTexture(specularPath, "specular");
	return textures;
}
// Zips the parallel positions/normals/UVs arrays into one Vertex per index.
// Vertex color is hard-coded to white since these meshes carry no per-vertex color data.
std::vector<Vertex> Model::assembleVertices(
	std::vector<glm::vec3> positions,
	std::vector<glm::vec3> normals,
	std::vector<glm::vec2> texUVs
) {
	if (positions.size() != normals.size() || positions.size() != texUVs.size())
		throw std::runtime_error("Position, normal, and texture-coordinate counts do not match");

	std::vector<Vertex> vertices;
	vertices.reserve(positions.size());
	for (int i = 0; i < positions.size(); i++) {

		vertices.push_back(
			Vertex
			{
				positions[i],
				normals[i],
				glm::vec3(1.0f, 1.0f, 1.0f),
				texUVs[i]

			}
		);

	}
	return vertices;

}

// Splits a flat float buffer into vec2 chunks, 2 floats each.
std::vector<glm::vec2> Model::groupFloatsVec2(std::vector<float> floatVec) {
	const unsigned int floatsPerVector = 2;

	std::vector<glm::vec2> vectors;

	assert(floatVec.size() % floatsPerVector == 0);

	for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector)
	{
		vectors.emplace_back(
			floatVec[i],
			floatVec[i + 1]
		);
	}

	return vectors;
}

// Splits a flat float buffer into vec3 chunks, 3 floats each.
std::vector<glm::vec3> Model::groupFloatsVec3(std::vector<float> floatVec) {
	const unsigned int floatsPerVector = 3;

	std::vector<glm::vec3> vectors;

	assert(floatVec.size() % floatsPerVector == 0);

	for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector)
	{
		vectors.emplace_back(
			floatVec[i],
			floatVec[i + 1],
			floatVec[i + 2]
		);
	}

	return vectors;
}