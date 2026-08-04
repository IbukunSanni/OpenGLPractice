#pragma once

#include <json/json.h>
#include "Mesh.h"

using json = nlohmann::json;

class Model {
public:
	// Loads in a model from a file and stores tha information in 'data', 'JSON', and 'file'
	Model(const char* file);

	void Draw(Shader& shader, Camera& camera);

private:
	// Variables for easy access
	const char* file;
	std::vector<unsigned char> data;
	json JSON;


};