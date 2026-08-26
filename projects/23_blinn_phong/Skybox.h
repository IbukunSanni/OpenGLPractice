#pragma once

#include <array>
#include <string>

#include <glm/glm.hpp>

#include "Camera.h"
#include "shaderClass.h"

// A cubemap sky drawn as a unit cube around the camera.
//
// Owns its VAO/VBO/EBO and the cubemap texture and releases them in the
// destructor -- these used to be loose handles in Main.cpp that were never
// deleted at all.
class Skybox
{
public:
	// `faces` must be ordered right, left, top, bottom, front, back. That is
	// not a convention, it is the order the GL_TEXTURE_CUBE_MAP_* enums are
	// numbered in; see the loop in the .cpp.
	explicit Skybox(const std::array<std::string, 6>& faces);
	~Skybox();

	// Owns GL handles, so copying would double-delete them.
	Skybox(const Skybox&) = delete;
	Skybox& operator=(const Skybox&) = delete;

	// Draw AFTER the rest of the scene, so early-z can discard the fragments
	// already covered. `projection` comes from the caller so the sky and the
	// scene cannot disagree about fov/near/far.
	void Draw(Shader& shader, const Camera& camera, const glm::mat4& projection) const;

private:
	unsigned int vao = 0;
	unsigned int vbo = 0;
	unsigned int ebo = 0;
	unsigned int cubemap = 0;
};
