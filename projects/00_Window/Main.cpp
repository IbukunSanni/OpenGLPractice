#include <glad/glad.h>   // Load all OpenGL function pointers
#include <GLFW/glfw3.h>  // GLFW for window and input handling
#include <iostream>

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW for OpenGL version 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create a window (1920x1080 resolution)
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Test Window", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Make the OpenGL context current for this window
    glfwMakeContextCurrent(window);

    // Load OpenGL functions with GLAD
    gladLoadGL();

    // Set viewport size (matches window size here)
    glViewport(0, 0, 1920, 1080);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Close window if space bar is pressed
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Clear background with solid color (#2F2C40)
        glClearColor(47.0f / 255.0f, 44.0f / 255.0f, 64.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for input events
        glfwPollEvents();
    }

    // Clean up and close
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
