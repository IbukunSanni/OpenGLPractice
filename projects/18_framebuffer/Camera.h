#pragma once
#define GLM_ENABLE_EXPERIMENTAL

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
	glm::mat4 cameraMatrix = glm::mat4(1.0f);


	// ── Input state ─────────────────────────────────────────────────────────

	// True until the first LMB click each drag; prevents a large position jump
	// caused by the cursor being far from the window centre on press.
	bool firstClick = true;

	// ── Viewport ────────────────────────────────────────────────────────────

	// Width and height of the window in pixels, used by Matrix() for the
	// aspect ratio and by Inputs() for normalised mouse deltas.
	int width;
	int height;

	// ── Tuning parameters ───────────────────────────────────────────────────

	// Translation speed in world-units per frame (held Shift roughly quadruples this)
	float speed = 0.01f;

	// Mouse-look sensitivity — higher values rotate faster for the same cursor movement
	float sensitivity = 100.0f;

	// ── Constructor ─────────────────────────────────────────────────────────

	// Stores the viewport size and initial world-space position.
	Camera(int width, int height, glm::vec3 position);

	// ── Methods ─────────────────────────────────────────────────────────────

	// Builds the combined (proj * view) matrix from the current Position /
	// Orientation, without uploading it anywhere (see Matrix() for that).
	void UpdateMatrix(float FOVdeg, float nearPlane, float farPlane);
	void Matrix(Shader& shader, const char* uniform);
	void SetView(const glm::vec3& position, const glm::vec3& direction);
	void LookAt(const glm::vec3& position, const glm::vec3& target);

	// Polls GLFW keyboard and mouse state then moves/rotates the camera:
	//   MMB drag         — orbit around the focus point
	//   Shift + MMB drag — pan
	//   Mouse wheel      — zoom (dolly in/out)
	//   W/S              — forward / backward along Orientation
	//   A/D              — strafe left / right
	//   Space / Ctrl     — fly up / down along world Up
	//   Left Shift       — sprint (speed × 4)
	void Inputs(GLFWwindow* window);

	// Registers callbacks and stores this camera on the GLFW window.
	void AttachToWindow(GLFWwindow* window);

private:
	// Point the camera orbits around (Blender-style pivot).
	glm::vec3 focusPoint = glm::vec3(0.0f, 0.0f, 0.0f);

	// Mouse state used for Blender-like controls.
	bool middleMouseHeld = false;
	double lastMouseX = 0.0;
	double lastMouseY = 0.0;

	// Tuning parameters for Blender-like controls.
	float orbitSensitivity = 0.15f;
	float panSensitivity = 0.002f;
	// Small so a single scroll notch stays fine-grained on ~1-unit-scale models.
	float zoomSensitivity = 0.05f;

	// Orbit camera around focusPoint using mouse delta.
	void Orbit(float deltaX, float deltaY);
	// Move both camera and focusPoint sideways/up-down in view space.
	void Pan(float deltaX, float deltaY);
	// Dolly camera toward/away from focusPoint.
	void Zoom(float amount);
	// GLFW scroll callback entry point (for mouse wheel zoom).
	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};

