#include "Camera.h"

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
    // If W is pressed, move the camera forward in the direction it is currently facing.
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        Position += speed * Orientation;
    }

    // If A is pressed, move the camera left using the opposite of the camera's right direction.
    // cross(Orientation, Up) gives the camera's right direction.
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        Position -= speed * glm::normalize(glm::cross(Orientation, Up));
    }

    // If S is pressed, move the camera backward, opposite to the direction it is facing.
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        Position -= speed * Orientation;
    }

    // If D is pressed, move the camera right using the camera's right direction.
    // The right direction is calculated from the camera's forward direction and up direction.
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        Position += speed * glm::normalize(glm::cross(Orientation, Up));
    }

    // If Space is pressed, move the camera upward along the world's up direction.
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        Position += speed * Up;
    }

    // If Left Control is pressed, move the camera downward opposite to the world's up direction.
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    {
        Position -= speed * Up;
    }

    // If Left Shift is pressed, temporarily increase the camera movement speed.
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        speed = 0.4f;
    }

    // If Left Shift is released, reset the camera movement speed back to normal.
    else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE)
    {
        speed = 0.1f;
    }

    // If the left mouse button is held, enable mouse-look camera rotation.
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        // Hide the cursor while controlling the camera.
        // This makes the camera movement feel like a first-person camera.
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

        // On the first click, move the cursor to the center of the window.
        // width / 2 and height / 2 give the center point of the screen.
        // This prevents a sudden camera jump when the mouse is first clicked.
        if (firstClick)
        {
            glfwSetCursorPos(window, (width / 2), (height / 2));
            firstClick = false;
        }

        // Store the current mouse position.
        double mouseX;
        double mouseY;

        // Get the current mouse position inside the window.
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // Calculate vertical mouse movement.
        // If the mouse moves above or below the center, rotX changes.
        // This controls looking up and down.
        float rotX = sensitivity * (float)(mouseY - (height / 2)) / height;

        // Calculate horizontal mouse movement.
        // If the mouse moves left or right of the center, rotY changes.
        // This controls looking left and right.
        float rotY = sensitivity * (float)(mouseX - (width / 2)) / width;

        // Create a possible new orientation after rotating up or down.
        // The rotation axis is the camera's right direction.
        // Negative rotX is used so mouse movement feels natural.
        glm::vec3 newOrientation = glm::rotate(
            Orientation,
            glm::radians(-rotX),
            glm::normalize(glm::cross(Orientation, Up))
        );

        // Prevent the camera from rotating too far upward or downward.
        // This avoids flipping the camera upside down.
        if (abs(glm::angle(newOrientation, Up) - glm::radians(90.0f)) <= glm::radians(85.0f))
        {
            Orientation = newOrientation;
        }

        // Rotate the camera left or right around the world's up direction.
        Orientation = glm::rotate(
            Orientation,
            glm::radians(-rotY),
            Up
        );

        // Move the cursor back to the center after reading its movement.
        // This lets the next frame measure movement relative to the center again.
        glfwSetCursorPos(window, (width / 2), (height / 2));
    }

    // If the left mouse button is released, stop mouse-look mode.
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
    {
        // Show the cursor again.
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        // Reset firstClick so the cursor is centered again next time mouse-look starts.
        firstClick = true;
    }
}