#include "Model.h"

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
		for (unsigned int i = 0; i < count; i++)
		{
			T value{};
			std::memcpy(&value, &data[beginningOfData + i * sizeof(T)], sizeof(T));
			indices.push_back(static_cast<GLuint>(value));
		}
	}
}

Model::Model(const char* file) {
	//Make a JSON object
	std::string text = get_file_contents(file);
	JSON = json::parse(text);

	Model::file = file;
	data = getData();

	traverseNode(0);

}

void Model::Draw(Shader& shader, Camera& camera) {
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		meshes[i].Mesh::Draw(shader, camera, matricesMeshes[i]);
	}
}

void Model::loadMesh(unsigned int indMesh) {
	unsigned int posAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["POSITION"];
	unsigned int normalAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["NORMAL"];
	unsigned int texAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["TEXCOORD_0"];
	unsigned int indAccInd = JSON["meshes"][indMesh]["primitives"][0]["indices"];

	std::vector<float> posVec = getFloats(JSON["acessors"][posAccInd]);
	std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
	std::vector<float> normalVec = getFloats(JSON["accessors"][normalAccInd]);
	std::vector<glm::vec3> normals = groupFloatsVec3(normalVec);
	std::vector<float> texVec = getFloats(JSON["accessors"][texAccInd]);
	std::vector<glm::vec2> texUVs = groupFloatsVec2(texVec);

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
	std::vector<GLuint> indices = getIndices(JSON["accessors"][indAccInd]);
	std::vector<Texture> textures = getTextures();

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

	unsigned int buffViewInd = accessor.value("bufferView", 1);
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];

	json bufferView = JSON["bufferView"][buffViewInd];
	unsigned int byteOffset = bufferView["byteOffset"];

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

	unsigned int buffViewInd = accessor.value("bufferView", 0);
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	unsigned int componentType = accessor["componentType"];

	json bufferView = JSON["bufferViews"][buffViewInd];
	unsigned int byteOffset = bufferView["byteOffset"];

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

	case GLTF_COMPONENT_SHORT:
		readIndicesAs<short>(data, beginningOfData, count, indices);
		break;

	default:
		throw std::invalid_argument(
			"Index componentType is invalid (not UNSIGNED_INT, UNSIGNED_SHORT, or SHORT)");
	}
	
	return indices;

}

