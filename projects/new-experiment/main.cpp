#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>

#include"../shaderClass.h"
#include"../VAO.h"
#include"../VBO.h"
#include"../EBO.h"

// TODO: Add your vertices here
GLfloat vertices[] = {
    // Add your vertex data
};

// TODO: Add your indices here  
GLuint indices[] = {
    // Add your index data
};

int main()
{
    // Initialize GLFW
    glfwInit();

    // OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 800, "New OpenGL Experiment", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Load GLAD
    gladLoadGL();
    glViewport(0, 0, 800, 800);

    // TODO: Load your shaders
    // Shader shaderProgram("your_vertex.vert", "your_fragment.frag");

    // TODO: Set up your VAO, VBO, EBO
    
    // Main render loop
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // TODO: Add your rendering code here
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}