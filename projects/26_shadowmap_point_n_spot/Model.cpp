#include "Model.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
	// glTF 2.0 accessor component types (spec 5.1.1 "accessor.componentType").
	// These values happen to be identical to the matching OpenGL enums.
	constexpr unsigned int GLTF_COMPONENT_BYTE           = 5120;
	constexpr unsigned int GLTF_COMPONENT_UNSIGNED_BYTE  = 5121;
	constexpr unsigned int GLTF_COMPONENT_SHORT          = 5122;
	constexpr unsigned int GLTF_COMPONENT_UNSIGNED_SHORT = 5123;
	constexpr unsigned int GLTF_COMPONENT_UNSIGNED_INT   = 5125;
	constexpr unsigned int GLTF_COMPONENT_FLOAT          = 5126;

	// glTF 2.0 JSON key names, factored out so each is spelled once.
	constexpr const char* GLTF_KEY_SCENE                       = "scene";
	constexpr const char* GLTF_KEY_SCENES                      = "scenes";
	constexpr const char* GLTF_KEY_NODES                       = "nodes";
	constexpr const char* GLTF_KEY_MESH                        = "mesh";
	constexpr const char* GLTF_KEY_MESHES                      = "meshes";
	constexpr const char* GLTF_KEY_PRIMITIVES                  = "primitives";
	constexpr const char* GLTF_KEY_ATTRIBUTES                  = "attributes";
	constexpr const char* GLTF_KEY_POSITION                    = "POSITION";
	constexpr const char* GLTF_KEY_NORMAL                      = "NORMAL";
	constexpr const char* GLTF_KEY_TEXCOORD_0                  = "TEXCOORD_0";
	constexpr const char* GLTF_KEY_INDICES                     = "indices";
	constexpr const char* GLTF_KEY_ACCESSORS                   = "accessors";
	constexpr const char* GLTF_KEY_TRANSLATION                 = "translation";
	constexpr const char* GLTF_KEY_ROTATION                    = "rotation";
	constexpr const char* GLTF_KEY_SCALE                       = "scale";
	constexpr const char* GLTF_KEY_MATRIX                      = "matrix";
	constexpr const char* GLTF_KEY_CHILDREN                    = "children";
	constexpr const char* GLTF_KEY_NAME                        = "name";
	constexpr const char* GLTF_KEY_BUFFERS                     = "buffers";
	constexpr const char* GLTF_KEY_URI                         = "uri";
	constexpr const char* GLTF_KEY_BUFFER_VIEW                 = "bufferView";
	constexpr const char* GLTF_KEY_BUFFER_VIEWS                = "bufferViews";
	constexpr const char* GLTF_KEY_COUNT                       = "count";
	constexpr const char* GLTF_KEY_BYTE_OFFSET                 = "byteOffset";
	constexpr const char* GLTF_KEY_TYPE                        = "type";
	constexpr const char* GLTF_KEY_COMPONENT_TYPE              = "componentType";
	constexpr const char* GLTF_KEY_INDEX                       = "index";
	constexpr const char* GLTF_KEY_TEXTURES                    = "textures";
	constexpr const char* GLTF_KEY_SOURCE                      = "source";
	constexpr const char* GLTF_KEY_IMAGES                      = "images";
	constexpr const char* GLTF_KEY_MATERIAL                    = "material";
	constexpr const char* GLTF_KEY_MATERIALS                   = "materials";
	constexpr const char* GLTF_KEY_PBR_METALLIC_ROUGHNESS      = "pbrMetallicRoughness";
	constexpr const char* GLTF_KEY_BASE_COLOR_TEXTURE          = "baseColorTexture";
	constexpr const char* GLTF_KEY_METALLIC_ROUGHNESS_TEXTURE  = "metallicRoughnessTexture";
	constexpr const char* GLTF_KEY_EXTENSIONS                  = "extensions";
	constexpr const char* GLTF_KEY_SPECULAR_GLOSSINESS         = "KHR_materials_pbrSpecularGlossiness";
	constexpr const char* GLTF_KEY_DIFFUSE_TEXTURE             = "diffuseTexture";
	constexpr const char* GLTF_KEY_SPECULAR_GLOSSINESS_TEXTURE = "specularGlossinessTexture";
	constexpr const char* GLTF_KEY_BASE_COLOR_FACTOR           = "baseColorFactor";
	constexpr const char* GLTF_KEY_DIFFUSE_FACTOR              = "diffuseFactor";

	// Reads 'count' tightly packed values of type T starting at 'beginningOfData'
	// and appends them as GLuints; sizeof(T) supplies the byte stride.
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

Model::Model(const char* file, unsigned int instancing,
	std::vector<glm::mat4> instanceMatrix,
	std::vector<std::string> skipNodeNames) {
	// Parse the glTF JSON and load its referenced binary buffer up front,
	// since every accessor lookup below reads out of these two members.
	std::string text = get_file_contents(file);
	JSON = json::parse(text);

	Model::file = file;
	// Assigned BEFORE traverseNode() below: that walk calls loadMesh(), which
	// constructs each Mesh -- and the Mesh constructor is where the instance
	// buffer is uploaded and the attribute divisors are set. Assign these
	// afterwards and every mesh is silently built as one non-instanced copy.
	Model::instancing = instancing;
	Model::instanceMatrix = instanceMatrix;
	// Same ordering requirement as the two above: traverseNode() reads this on
	// the way down, so it has to be in place before the walk starts.
	Model::skipNodeNames = skipNodeNames;
	data = getData();

	// Recursively load every mesh reachable from the active scene's root nodes.
	const unsigned int sceneIndex = JSON.value(GLTF_KEY_SCENE, 0u);
	const json& rootNodes = JSON.at(GLTF_KEY_SCENES).at(sceneIndex).at(GLTF_KEY_NODES);
	if (rootNodes.empty())
		throw std::runtime_error("The selected glTF scene has no root nodes");

	for (const json& rootNode : rootNodes)
		traverseNode(rootNode.get<unsigned int>());

}

void Model::ApplyTransform(const glm::mat4& m) {
	// Pre-multiplying the per-mesh node matrices is what makes this a
	// rebase: Draw() and GetWorldCenter() both read these, so neither
	// needs to know a transform was applied.
	for (glm::mat4& node : matricesMeshes)
		node = m * node;
}

void Model::Draw(Shader& shader, Camera& camera, const glm::mat4& worldTransform) {
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		// worldTransform LEFT of the node matrix: world space, not model space.
		meshes[i].Mesh::Draw(shader, camera, worldTransform * matricesMeshes[i]);
	}
}

void Model::Draw(
	Shader& shader,
	Camera& camera,
	glm::vec3 translation,
	glm::quat rotation,
	glm::vec3 scale) {
	// Each mesh keeps its own world matrix, computed once in traverseNode();
	// the T/R/S below are applied on top of it to place the whole model.
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		meshes[i].Mesh::Draw(shader, camera, matricesMeshes[i], translation, rotation, scale);
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
	const json& primitive = JSON[GLTF_KEY_MESHES][indMesh][GLTF_KEY_PRIMITIVES][0];
	unsigned int posAccInd = primitive[GLTF_KEY_ATTRIBUTES][GLTF_KEY_POSITION];
	unsigned int normalAccInd = primitive[GLTF_KEY_ATTRIBUTES][GLTF_KEY_NORMAL];
	unsigned int texAccInd = primitive[GLTF_KEY_ATTRIBUTES][GLTF_KEY_TEXCOORD_0];
	unsigned int indAccInd = primitive[GLTF_KEY_INDICES];

	std::vector<float> posVec = getFloats(JSON[GLTF_KEY_ACCESSORS][posAccInd]);
	std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
	std::vector<float> normalVec = getFloats(JSON[GLTF_KEY_ACCESSORS][normalAccInd]);
	std::vector<glm::vec3> normals = groupFloatsVec3(normalVec);
	std::vector<float> texVec = getFloats(JSON[GLTF_KEY_ACCESSORS][texAccInd]);
	std::vector<glm::vec2> texUVs = groupFloatsVec2(texVec);

	// Combine all the vertex components and also get the indices and textures
	std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
	std::vector<GLuint> indices = getIndices(JSON[GLTF_KEY_ACCESSORS][indAccInd]);
	std::vector<Texture> textures = getTextures(primitive);

	// Combine the vertices, indices, and textures into a mesh
	meshes.push_back(Mesh(vertices, indices, textures, instancing, instanceMatrix));

}

void Model::traverseNode(unsigned int nextNode, glm::mat4 matrix) {
	const json& node = JSON[GLTF_KEY_NODES][nextNode];

	// Dropped before the transform is even built: a skipped node takes its whole
	// subtree with it, since a child's placement is only meaningful relative to a
	// parent that is being drawn.
	if (!skipNodeNames.empty())
	{
		const std::string nodeName = node.value(GLTF_KEY_NAME, std::string{});
		for (const std::string& skipped : skipNodeNames)
		{
			if (!nodeName.empty() && nodeName == skipped)
				return;
		}
	}

	glm::vec3 translation(0.0f);

	if (node.find(GLTF_KEY_TRANSLATION) != node.end()) {
		translation = glm::vec3(
			node[GLTF_KEY_TRANSLATION][0],
			node[GLTF_KEY_TRANSLATION][1],
			node[GLTF_KEY_TRANSLATION][2]
		);
	}

	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	if (node.find(GLTF_KEY_ROTATION) != node.end()) {
		// glTF stores quaternions as [x, y, z, w], but glm::quat's constructor
		// takes (w, x, y, z), so the components are reordered on the way in.
		rotation = glm::quat(
			node[GLTF_KEY_ROTATION][3],
			node[GLTF_KEY_ROTATION][0],
			node[GLTF_KEY_ROTATION][1],
			node[GLTF_KEY_ROTATION][2]
		);
	}

	glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
	if (node.find(GLTF_KEY_SCALE) != node.end()) {
		scale = glm::vec3(
			node[GLTF_KEY_SCALE][0],
			node[GLTF_KEY_SCALE][1],
			node[GLTF_KEY_SCALE][2]
		);
	}

	glm::mat4 matNode = glm::mat4(1.0f);
	if (node.find(GLTF_KEY_MATRIX) != node.end()) {
		if (node[GLTF_KEY_MATRIX].size() != 16)
			throw std::runtime_error("A glTF node matrix must contain exactly 16 values");

		float matValues[16];
		for (unsigned int i = 0; i < node[GLTF_KEY_MATRIX].size(); i++)
			matValues[i] = (node[GLTF_KEY_MATRIX][i]);
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
	if (node.find(GLTF_KEY_MESH) != node.end())
	{
		translationsMeshes.push_back(translation);
		rotationsMeshes.push_back(rotation);
		scalesMeshes.push_back(scale);
		matricesMeshes.push_back(matNextNode);

		loadMesh(node[GLTF_KEY_MESH]);
	}

	// Check if the node has children, and if it does, apply this function to them with the matNextNode
	if (node.find(GLTF_KEY_CHILDREN) != node.end())
	{
		for (unsigned int i = 0; i < node[GLTF_KEY_CHILDREN].size(); i++)
			traverseNode(node[GLTF_KEY_CHILDREN][i], matNextNode);
	}

}

// Reads the binary buffer referenced by the glTF's "buffers[0].uri" field.
// The URI is relative to the model file's own directory, not the CWD.
std::vector<unsigned char> Model::getData() {
	std::string bytesText;
	std::string uri = JSON[GLTF_KEY_BUFFERS][0][GLTF_KEY_URI];

	std::string fileStr = std::string(file);
	std::string fileDirectory = fileStr.substr(0, fileStr.find_last_of('/') + 1);
	bytesText = get_file_contents((fileDirectory + uri).c_str());

	std::vector<unsigned char> data(bytesText.begin(), bytesText.end());
	return data;

}

std::vector<float> Model::getFloats(json accessor) {
	std::vector<float> floatVec;

	unsigned int buffViewInd = accessor.at(GLTF_KEY_BUFFER_VIEW);
	unsigned int count = accessor[GLTF_KEY_COUNT];
	unsigned int accByteOffset = accessor.value(GLTF_KEY_BYTE_OFFSET, 0);
	std::string type = accessor[GLTF_KEY_TYPE];

	json bufferView = JSON[GLTF_KEY_BUFFER_VIEWS][buffViewInd];
	// byteOffset is optional in glTF and defaults to 0. Reading it directly
	// throws type_error.302 on the many exporters that omit it.
	unsigned int byteOffset = bufferView.value(GLTF_KEY_BYTE_OFFSET, 0);

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

	unsigned int buffViewInd = accessor.at(GLTF_KEY_BUFFER_VIEW);
	unsigned int count = accessor[GLTF_KEY_COUNT];
	unsigned int accByteOffset = accessor.value(GLTF_KEY_BYTE_OFFSET, 0);
	unsigned int componentType = accessor[GLTF_KEY_COMPONENT_TYPE];

	json bufferView = JSON[GLTF_KEY_BUFFER_VIEWS][buffViewInd];
	// byteOffset is optional in glTF and defaults to 0 (see getFloats above).
	unsigned int byteOffset = bufferView.value(GLTF_KEY_BYTE_OFFSET, 0);

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
		if (textureInfo.find(GLTF_KEY_INDEX) == textureInfo.end())
			return {};

		const unsigned int textureIndex = textureInfo[GLTF_KEY_INDEX];
		if (textureIndex >= JSON[GLTF_KEY_TEXTURES].size())
			return {};

		const json& texture = JSON[GLTF_KEY_TEXTURES][textureIndex];
		if (texture.find(GLTF_KEY_SOURCE) == texture.end())
			return {};

		const unsigned int imageIndex = texture[GLTF_KEY_SOURCE];
		if (imageIndex >= JSON[GLTF_KEY_IMAGES].size())
			return {};

		return JSON[GLTF_KEY_IMAGES][imageIndex].value(GLTF_KEY_URI, std::string{});
	};

	// Last-resort guess by filename when the material declares no texture.
	// Counts matches so it can warn: with more than one candidate every mesh
	// silently receives the SAME image, so the failure mode is a wrong
	// texture rather than a missing one -- much harder to spot.
	// A flat colour, shaped as a 1x1 texture. A material with no map is not
	// broken -- glTF lets it carry only a baseColorFactor, and the spec defaults
	// that to white when absent. Handing the shader one pixel of that colour lets
	// the whole pipeline stay texture-based with no branch and no extra uniform.
	auto solidColorTexture = [](const json& factor, const char* type, GLuint slot) {
		unsigned char px[4] = { 255, 255, 255, 255 };
		if (factor.is_array() && factor.size() >= 3)
		{
			for (unsigned int c = 0; c < 4 && c < factor.size(); c++)
			{
				const float v = factor[c].get<float>();
				px[c] = static_cast<unsigned char>(glm::clamp(v, 0.0f, 1.0f) * 255.0f);
			}
		}
		return Texture(px, 1, 1, type, slot);
	};

	// The classic missing-texture checker. Generated rather than loaded, so there
	// is no asset that can itself go missing, and deliberately ugly: this should
	// never be mistaken for someone's intended material.
	auto checkerTexture = [](const char* type, GLuint slot) {
		constexpr int N = 16, CELL = 4;
		unsigned char px[N * N * 4];
		for (int y = 0; y < N; y++)
		{
			for (int x = 0; x < N; x++)
			{
				const bool light = ((x / CELL) + (y / CELL)) % 2 == 0;
				unsigned char* q = px + (y * N + x) * 4;
				q[0] = q[1] = q[2] = light ? 230 : 90;
				q[3] = 255;
			}
		}
		return Texture(px, N, N, type, slot, false);
	};

	// Texture units follow the material ROLE, not a running count. Mesh::Draw
	// binds exactly one diffuse and one specular per mesh and names them
	// diffuse0 / specular0, so two units is all a mesh can ever use.
	//
	// This was loadedTex.size() -- a counter running across every mesh in the
	// model. With 25 meshes and 10 materials, nearly all of which generate a
	// fallback specular, it climbed past 20: past the 16 fragment texture units
	// GL 3.3 guarantees, and over whatever else the frame had already bound to
	// those units. The shadow map on unit 2 was the visible casualty -- drawing
	// the car overwrote it, so the floor sampled a 1x1 black fallback as depth
	// and its shadow disappeared for exactly as long as the car was on screen.
	auto slotForType = [](const char* type) -> GLuint {
		return std::string(type) == "specular" ? 1u : 0u;
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
				// The cache stores the GL object; the unit belongs to the role it is
				// being reused AS, which may differ from the role it was loaded for.
				reusedTexture.unit = slotForType(type);
				textures.push_back(reusedTexture);
				return;
			}
		}

		Texture texture((fileDirectory + path).c_str(), type, slotForType(type));
		textures.push_back(texture);
		loadedTex.push_back(texture);
		loadedTexName.push_back(path);
	};

	std::string diffusePath;
	std::string specularPath;
	// Absent means white, per the glTF spec's default for baseColorFactor.
	json baseColorFactor;

	if (primitive.find(GLTF_KEY_MATERIAL) != primitive.end())
	{
		const unsigned int materialIndex = primitive[GLTF_KEY_MATERIAL];
		if (materialIndex < JSON[GLTF_KEY_MATERIALS].size())
		{
			const json& material = JSON[GLTF_KEY_MATERIALS][materialIndex];
			const auto pbrIt = material.find(GLTF_KEY_PBR_METALLIC_ROUGHNESS);
			if (pbrIt != material.end())
			{
				const json& pbr = *pbrIt;
				const auto baseColorIt = pbr.find(GLTF_KEY_BASE_COLOR_TEXTURE);
				if (baseColorIt != pbr.end())
					diffusePath = imagePathFromTexture(*baseColorIt);

				const auto factorIt = pbr.find(GLTF_KEY_BASE_COLOR_FACTOR);
				if (factorIt != pbr.end())
					baseColorFactor = *factorIt;

				const auto metallicIt = pbr.find(GLTF_KEY_METALLIC_ROUGHNESS_TEXTURE);
				if (metallicIt != pbr.end())
					specularPath = imagePathFromTexture(*metallicIt);
			}

			// Sketchfab and many FBX->glTF converters emit
			// KHR_materials_pbrSpecularGlossiness instead of the core PBR block,
			// putting the albedo map under extensions. Without this every material
			// reads as textureless and falls through to the filename guess above,
			// which hands every mesh the same image.
			if (diffusePath.empty())
			{
				const auto extIt = material.find(GLTF_KEY_EXTENSIONS);
				if (extIt != material.end())
				{
					const auto sgIt = extIt->find(GLTF_KEY_SPECULAR_GLOSSINESS);
					if (sgIt != extIt->end())
					{
						const auto diffIt = sgIt->find(GLTF_KEY_DIFFUSE_TEXTURE);
						if (diffIt != sgIt->end())
							diffusePath = imagePathFromTexture(*diffIt);

						const auto dfIt = sgIt->find(GLTF_KEY_DIFFUSE_FACTOR);
						if (dfIt != sgIt->end() && baseColorFactor.is_null())
							baseColorFactor = *dfIt;

						const auto specIt = sgIt->find(GLTF_KEY_SPECULAR_GLOSSINESS_TEXTURE);
						if (specIt != sgIt->end())
							specularPath = imagePathFromTexture(*specIt);
					}
				}
			}
		}
	}

	// Resolution order. The filename guess that used to live here is gone on
	// purpose: it turned "no data" into confidently wrong data, painting one
	// model's only image across every mesh that lacked a map of its own.
	//
	//   1. a declared texture that loads          -> use it
	//   2. no texture but a colour factor         -> 1x1 of that colour
	//   3. a texture declared that will not load  -> checker + warning
	const std::size_t before = textures.size();
	addTexture(diffusePath, "diffuse");

	if (textures.size() == before)
	{
		if (!diffusePath.empty())
		{
			std::cout << "[Model] WARNING: material declares diffuse texture \""
				<< diffusePath << "\" but it could not be loaded; using the "
				<< "missing-texture checker." << std::endl;
			textures.push_back(checkerTexture("diffuse", slotForType("diffuse")));
		}
		else
		{
			// Untextured by design: solid-colour material, not a failure.
			textures.push_back(solidColorTexture(baseColorFactor, "diffuse",
				slotForType("diffuse")));
		}
		loadedTex.push_back(textures.back());
		loadedTexName.push_back(std::string());
	}

	const std::size_t beforeSpec = textures.size();
	addTexture(specularPath, "specular");
	if (textures.size() == beforeSpec)
	{
		// No specular map. Black means "no specular contribution" rather than
		// leaving specular0 unbound, which samples whatever happens to be there.
		const json black = json::array({ 0.0f, 0.0f, 0.0f, 1.0f });
		textures.push_back(solidColorTexture(black, "specular",
			slotForType("specular")));
		loadedTex.push_back(textures.back());
		loadedTexName.push_back(std::string());
	}

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