#include "Camera.h"

#include <cmath>

namespace
{
    constexpr float ORBIT_ZERO_LENGTH_EPSILON = 0.0001f;
    constexpr float CAMERA_RIGHT_ANGLE_DEGREES = 90.0f;
    constexpr float CAMERA_PITCH_CLAMP_DEGREES = 85.0f;
    constexpr float CAMERA_SHIFT_SPEED = 0.2f;
    constexpr float CAMERA_BASE_SPEED = 0.05f;
    constexpr float MIN_CAMERA_FOCUS_DISTANCE = 0.2f;
}

Camera::Camera(int width, int height, glm::vec3 position)
{
    // Store the window width so the camera can calculate the aspect ratio.
    Camera::width = width;

    // Store the window height so the camera can calculate the aspect ratio.
    Camera::height = height;

    // Set the camera's starting position in the 3D world.
    Position = position;
}

void Camera::Matrix(float FOVdeg, float nearPlane, float farPlane, Shader& shader, const char* uniform)
{
    // Create an identity matrix for the view matrix.
    // The view matrix controls where the camera is and where it is looking.
    glm::mat4 view = glm::mat4(1.0f);

    // Create an identity matrix for the projection matrix.
    // The projection matrix controls perspective, FOV, and clipping distance.
    glm::mat4 projection = glm::mat4(1.0f);

    // Build the view matrix.
    // Position is where the camera is.
    // Position + Orientation is the point the camera is looking toward.
    // Up tells the camera which direction counts as "up."
    view = glm::lookAt(Position, Position + Orientation, Up);

    // Calculate the aspect ratio of the window.
    // This prevents the scene from looking stretched.
    auto aspect_ratio = (float)width / height;

    // Build the projection matrix.
    // FOVdeg controls how wide the camera can see.
    // aspect_ratio matches the camera to the window shape.
    // nearPlane and farPlane control the closest and farthest visible distances.
    projection = glm::perspective(
        glm::radians(FOVdeg),
        aspect_ratio,
        nearPlane,
        farPlane
    );

    // Send the final camera matrix to the shader.
    // The vertex shader uses this matrix to transform world coordinates into screen space.
    glUniformMatrix4fv(
        glGetUniformLocation(shader.ID, uniform),
        1,
        GL_FALSE,
        glm::value_ptr(projection * view)
    );
}

void Camera::Inputs(GLFWwindow* window)
{
    // Blender-style mouse controls:
    // - MMB drag: orbit
    // - Shift + MMB drag: pan
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
    {
        double mouseX;
        double mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (!middleMouseHeld)
        {
            middleMouseHeld = true;
            lastMouseX = mouseX;
            lastMouseY = mouseY;
        }

        float deltaX = static_cast<float>(mouseX - lastMouseX);
        float deltaY = static_cast<float>(mouseY - lastMouseY);

        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        {
            // Shift modifies MMB drag into panning.
            Pan(deltaX, deltaY);
        }
        else
        {
            // Plain MMB drag orbits around the current pivot.
            Orbit(deltaX, deltaY);
        }

        lastMouseX = mouseX;
        lastMouseY = mouseY;
    }
    else
    {
        middleMouseHeld = false;
    }

    // If W is pressed, move the camera forward in the direction it is currently facing.
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        Position += speed * Orientation;
        focusPoint += speed * Orientation;
    }

    // If A is pressed, move the camera left using the opposite of the camera's right direction.
    // cross(Orientation, Up) gives the camera's right direction.
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        glm::vec3 right = glm::normalize(glm::cross(Orientation, Up));
        Position -= speed * right;
        focusPoint -= speed * right;
    }

    // If S is pressed, move the camera backward, opposite to the direction it is facing.
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        Position -= speed * Orientation;
        focusPoint -= speed * Orientation;
    }

    // If D is pressed, move the camera right using the camera's right direction.
    // The right direction is calculated from the camera's forward direction and up direction.
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        glm::vec3 right = glm::normalize(glm::cross(Orientation, Up));
        Position += speed * right;
        focusPoint += speed * right;
    }

    // If Space is pressed, move the camera upward along the world's up direction.
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        Position += speed * Up;
        focusPoint += speed * Up;
    }

    // If Left Control is pressed, move the camera downward opposite to the world's up direction.
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    {
        Position -= speed * Up;
        focusPoint -= speed * Up;
    }

    // If Left Shift is pressed, temporarily increase the camera movement speed.
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        speed = CAMERA_SHIFT_SPEED;
    }

    // If Left Shift is released, reset the camera movement speed back to normal.
    else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE)
    {
        speed = CAMERA_BASE_SPEED;
    }

    Orientation = glm::normalize(focusPoint - Position);
}

void Camera::AttachToWindow(GLFWwindow* window)
{
    // Needed so the static scroll callback can find the Camera instance.
    glfwSetWindowUserPointer(window, this);
    // Mouse wheel drives zoom.
    glfwSetScrollCallback(window, Camera::ScrollCallback);
}

void Camera::Orbit(float deltaX, float deltaY)
{
    // Vector from pivot to camera position; rotating this vector performs orbit.
    glm::vec3 offset = Position - focusPoint;
    if (glm::length(offset) < ORBIT_ZERO_LENGTH_EPSILON)
    {
        offset = -Orientation;
    }

    glm::vec3 right = glm::normalize(glm::cross(Orientation, Up));
    // Horizontal mouse movement: yaw around world up.
    offset = glm::rotate(offset, glm::radians(-deltaX * orbitSensitivity), Up);

    // Vertical mouse movement: pitch around camera right.
    glm::vec3 candidate = glm::rotate(offset, glm::radians(-deltaY * orbitSensitivity), right);
    glm::vec3 candidateDirection = glm::normalize(-candidate);

    // Clamp pitch so camera never flips upside-down.
    if (abs(glm::angle(candidateDirection, Up) - glm::radians(CAMERA_RIGHT_ANGLE_DEGREES)) <= glm::radians(CAMERA_PITCH_CLAMP_DEGREES))
    {
        offset = candidate;
    }

    Position = focusPoint + offset;
    Orientation = glm::normalize(focusPoint - Position);
}

void Camera::Pan(float deltaX, float deltaY)
{
    // Scale pan speed by distance so movement feels consistent while zoomed.
    float distance = glm::length(focusPoint - Position);
    float scaledPan = panSensitivity * distance;

    glm::vec3 right = glm::normalize(glm::cross(Orientation, Up));
    // Screen-space pan: mouse X moves along camera right, mouse Y moves along world up.
    glm::vec3 panDelta = (-right * deltaX + Up * deltaY) * scaledPan;

    Position += panDelta;
    focusPoint += panDelta;
    Orientation = glm::normalize(focusPoint - Position);
}

void Camera::Zoom(float amount)
{
    // Dolly camera forward/back along viewing direction.
    float distance = glm::length(focusPoint - Position);
    float zoomAmount = amount * zoomSensitivity;

    // Keep a small minimum distance from pivot to avoid singularities.
    if (distance - zoomAmount < MIN_CAMERA_FOCUS_DISTANCE)
    {
        zoomAmount = distance - MIN_CAMERA_FOCUS_DISTANCE;
    }

    Position += Orientation * zoomAmount;
    Orientation = glm::normalize(focusPoint - Position);
}

void Camera::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)xoffset;
    // Recover active camera instance from window and apply wheel zoom.
    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera != nullptr)
    {
        camera->Zoom(static_cast<float>(yoffset));
    }
}
