#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<stb/stb_image.h>

#include"shaderClass.h"
#include"VAO.h"
#include"VBO.h"
#include"EBO.h"



// Each vertex (corner point) of our shape has 8 values:
//   - 3 for position (x, y, z) which tells the GPU where the point is in 3D space
//   - 3 for color (red, green, blue) ranging from 0.0 (none) to 1.0 (full)
//   - 2 for texture coordinates (u, v) used to map an image onto the shape
GLfloat vertices[] =
{ //       X      Y      Z        R     G     B       U     V
	-0.5f, -0.5f, 0.0f,     1.0f, 0.0f, 0.0f,	0.0f, 0.0f, // Lower left corner  (red)
	-0.5f,  0.5f, 0.0f,     0.0f, 1.0f, 0.0f,	0.0f, 1.0f, // Upper left corner  (green)
	 0.5f,  0.5f, 0.0f,     0.0f, 0.0f, 1.0f,	1.0f, 1.0f, // Upper right corner (blue)
	 0.5f, -0.5f, 0.0f,     1.0f, 1.0f, 1.0f,	1.0f, 0.0f  // Lower right corner (white)
};

// OpenGL draws shapes using triangles. To make a square, we need 2 triangles.
// Instead of duplicating vertex data, we reuse vertices by referring to their
// index (position) in the vertices array above: 0 = lower-left, 1 = upper-left, etc.
GLuint indices[] =
{
	0, 2, 1, // First triangle  (lower-left, upper-right, upper-left)
	0, 3, 2  // Second triangle (lower-left, lower-right, upper-right)
};


int main()
{
	// GLFW is a library that handles window creation and input (keyboard, mouse).
	// We need to initialize it before we can use any of its functions.
	glfwInit();

	// Tell GLFW we want to use OpenGL version 3.3.
	// The version is split into "major.minor", so major=3 and minor=3 means 3.3.
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// Use the CORE profile, which only includes modern OpenGL functions.
	// The alternative (COMPATIBILITY profile) includes old deprecated functions we don't need.
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Create a window that is 800x800 pixels with the title "YoutubeOpenGL".
	// This is the actual window that will appear on your screen.
	GLFWwindow* window = glfwCreateWindow(800, 800, "YoutubeOpenGL", NULL, NULL);
	// Always check if the window was created successfully.
	// If it wasn't (e.g., your GPU doesn't support OpenGL 3.3), we exit early.
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	// Make this window the one that all our OpenGL commands will draw to.
	// You could have multiple windows, so this tells OpenGL which one is "active".
	glfwMakeContextCurrent(window);

	// GLAD loads the actual OpenGL function pointers for your specific GPU.
	// Without this, calling any OpenGL function (like glViewport) would crash.
	gladLoadGL();
	// Set the area of the window where OpenGL will draw.
	// (0, 0) is the bottom-left corner, and (800, 800) is the top-right.
	// This should match your window size so the image fills the whole window.
	glViewport(0, 0, 800, 800);



	// A shader program tells the GPU how to process and display your geometry.
	// "default.vert" (vertex shader) positions each vertex on screen.
	// "default.frag" (fragment shader) determines the color of each pixel.
	Shader shaderProgram("projects/texture_test/default.vert", "projects/texture_test/default.frag");



	// A VAO (Vertex Array Object) stores the configuration of how vertex data is organized.
	// Think of it as a "recipe" that tells OpenGL how to read the vertex data.
	VAO VAO1;
	VAO1.Bind();

	// A VBO (Vertex Buffer Object) sends our vertex data (positions, colors, etc.) to the GPU.
	VBO VBO1(vertices, sizeof(vertices));
	// An EBO (Element Buffer Object) sends our index data to the GPU,
	// so OpenGL knows which vertices to connect into triangles.
	EBO EBO1(indices, sizeof(indices));

	// Tell the VAO how to interpret the data inside the VBO:
	//   Attribute 0 = position      (3 floats starting at offset 0)
	//   Attribute 1 = color         (3 floats starting at offset 3)
	//   Attribute 2 = tex coords    (2 floats starting at offset 6)
	// The stride (8 * sizeof(float)) is the total size of one vertex's data.
	VAO1.LinkAttrib(VBO1, 0, 3, GL_FLOAT, 8 * sizeof(float), (void*)0);
	VAO1.LinkAttrib(VBO1, 1, 3, GL_FLOAT, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	VAO1.LinkAttrib(VBO1, 2, 2, GL_FLOAT, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	// Unbind everything so we don't accidentally modify this setup later.
	VAO1.Unbind();
	VBO1.Unbind();
	EBO1.Unbind();

	// A "uniform" is a variable you can pass from your C++ code into a shader.
	// Here we get the location of a uniform called "scale" so we can set its value later.
	GLuint uniID = glGetUniformLocation(shaderProgram.ID, "scale");

	// --- Texture Loading ---
	// Load the image file from disk using stb_image.
	// stbi_load returns the pixel data along with the image's width, height,
	// and number of color channels (e.g., 3 for RGB, 4 for RGBA).
	int  widthImg, heightImg, numColCh;
	// Images are stored top-to-bottom, but OpenGL expects bottom-to-top.
	// This flips the image so it appears right-side up.
	stbi_set_flip_vertically_on_load(true);
	unsigned char* bytes = stbi_load("projects/texture_test/green_plane.jpg", &widthImg, &heightImg, &numColCh, 0);

	// Create a texture object on the GPU and bind it so we can configure it.
	// GL_TEXTURE0 is the first texture slot — shaders can sample from multiple slots.
	GLuint texture;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	// Set filtering modes: GL_NEAREST gives a sharp, pixelated look (no blurring).
	// MIN_FILTER is used when the texture is drawn smaller than its actual size.
	// MAG_FILTER is used when the texture is drawn larger than its actual size.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Set wrapping modes: GL_REPEAT tiles the texture if coordinates go beyond 0-1.
	// WRAP_S = horizontal direction, WRAP_T = vertical direction.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Upload the image data to the GPU as a 2D texture.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, widthImg, heightImg, 0, GL_RGB, GL_UNSIGNED_BYTE, bytes);
	// Generate smaller versions of the texture (mipmaps) for when the object is far away.
	glGenerateMipmap(GL_TEXTURE_2D);

	// The image data is now on the GPU, so we can free the CPU-side copy.
	stbi_image_free(bytes);
	// Unbind the texture so we don't accidentally modify it.
	glBindTexture(GL_TEXTURE_2D, 0);

	// Get the location of the "tex0" sampler uniform in our shader
	// so we can tell it which texture slot to read from.
	GLuint tex0Uni = glGetUniformLocation(shaderProgram.ID, "tex0");

	// The render loop: this runs every frame until the user closes the window.
	// Each iteration draws one frame of the image you see on screen.
	while (!glfwWindowShouldClose(window))
	{
		// Set the background color (dark blue) as RGBA values (0.0 to 1.0 each).
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		// Actually fill the screen with that background color, erasing the previous frame.
		glClear(GL_COLOR_BUFFER_BIT);
		// Activate our shader program so the GPU uses it for the upcoming draw call.
		shaderProgram.Activate();
		// Send the value 0.5 to the "scale" uniform in our shader.
		glUniform1f(uniID, 0.5f);
		// Bind our texture so the fragment shader can sample from it.
		glBindTexture(GL_TEXTURE_2D, texture);
		// Bind our VAO
		VAO1.Bind();

		// Draw 6 indices (2 triangles) using the data from our VAO and EBO.
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// OpenGL uses "double buffering": it draws to a hidden back buffer, then
		// swaps it to the screen. This prevents flickering or half-drawn frames.
		glfwSwapBuffers(window);
		// Check for user input (keyboard, mouse, window close button, etc.).
		glfwPollEvents();

	}



	// Clean up: free all the GPU resources we allocated (buffers, shaders).
	// This is like calling "delete" for GPU memory.
	VAO1.Delete();
	VBO1.Delete();
	EBO1.Delete();
	glDeleteTextures(1, &texture);
	shaderProgram.Delete();
	// Close the window and shut down GLFW.
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}