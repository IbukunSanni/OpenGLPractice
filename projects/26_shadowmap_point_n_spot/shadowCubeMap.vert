#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;

// World space only -- no light matrix. Which of the six applies depends on the
// cube face, and only the geometry stage knows that.
void main()
{
    gl_Position = model * vec4(aPos, 1.0);
}
