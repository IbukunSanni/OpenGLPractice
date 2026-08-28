#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 shadowMatrices[6];

out vec4 FragPos;

// Fills all six faces in one pass: 3 vertices in, the triangle re-emitted per
// face (hence 18 out). gl_Layer picks the face and is only writable here.
// FragPos carries world position through; gl_FragCoord has already lost it.
void main()
{
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face;
        for(int i = 0; i < 3; i++)
        {
            FragPos = gl_in[i].gl_Position;
            gl_Position = shadowMatrices[face] * FragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
