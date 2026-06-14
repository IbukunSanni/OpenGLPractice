#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>  // glm::rotate() on vectors
#include <glm/gtx/vector_angle.hpp>   // glm::angle() — used to clamp vertical look

#include "shaderClass.h"

class Camera
{
public:
	// ── Position & orientation ──────────────────────────────────────────────

	// Camera position in world space
	glm::vec3 Position;

	// Direction the camera is currently pointing (unit vector, updated by mouse)
	glm::vec3 Orientation = glm::vec3(0.0f, 0.0f, -1.0f);

	// World-space up vector — kept constant so the horizon never tilts
	glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);

	// ── Input state ─────────────────────────────────────────────────────────

	// True until the first LMB click each drag; prevents a large position jump
	// caused by the cursor being far from the window centre on press.
	bool firstClick = true;

	// ── Viewport ────────────────────────────────────────────────────────────

	// Width and height of the window in pixels.
	// Stored so Matrix() can compute the aspect ratio and Inputs() can
	// compute normalised mouse deltas without needing the window each frame.
	int width;
	int height;

	// ── Tuning parameters ───────────────────────────────────────────────────

	// Translation speed in world-units per frame (held Shift doubles this)
	float speed = 0.1f;

	// Mouse-look sensitivity — higher values rotate faster for the same cursor movement
	float sensitivity = 100.0f;

	// ── Constructor ─────────────────────────────────────────────────────────

	// Stores the viewport size and initial world-space position.
	Camera(int width, int height, glm::vec3 position);

	// ── Methods ─────────────────────────────────────────────────────────────

	// Builds the combined (proj * view) matrix from the current Position /
	// Orientation and uploads it to the named uniform in the given shader.
	//   FOVdeg    — vertical field of view in degrees (typically 45)
	//   nearPlane — closest visible distance (e.g. 0.1)
	//   farPlane  — furthest visible distance (e.g. 100)
	//   shader    — the active Shader whose uniform will be written
	//   uniform   — name of the mat4 uniform to update (e.g. "camMatrix")
	void Matrix(float FOVdeg, float nearPlane, float farPlane, Shader& shader, const char* uniform);

	// Polls GLFW keyboard and mouse state then moves/rotates the camera:
	//   W/S          — forward / backward along Orientation
	//   A/D          — strafe left / right
	//   Space / Ctrl — fly up / down along world Up
	//   Left Shift   — sprint (speed × 4)
	//   LMB hold     — hides cursor and enables mouse look (drag to rotate)
	//   LMB release  — restores cursor and resets firstClick
	void Inputs(GLFWwindow* window);
};
