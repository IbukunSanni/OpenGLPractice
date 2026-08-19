#version 330 core

// Locations must match the glVertexAttribPointer calls for rectVAO in Main.cpp.
layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inTexCoords;

out vec2 texCoords;

void main()
{
    // The quad is authored directly in NDC (-1..1), so no camera or model
    // matrix is applied; z is 0 and w is 1 to skip the perspective divide.
    gl_Position = vec4(inPos.x, inPos.y, 0.0, 1.0);
    texCoords = inTexCoords;
}