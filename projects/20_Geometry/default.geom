#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

out vec3 Normal;
out vec3 color;
out vec2 texCoord;
// default.frag needs the world-space position for its view direction and
// light vectors. default.vert feeds this stage now instead of the fragment
// stage, so the geometry shader is the only place left that can supply it.
out vec3 crntPos;

in DATA
{
    vec3 Normal;
	vec3 color;
	vec2 texCoord;
    mat4 projection;
} data_in[];

// Pass-through: emits each triangle unchanged, projecting the world-space
// positions that default.vert now hands over.
void main()
{
    gl_Position = data_in[0].projection * gl_in[0].gl_Position;
    Normal = data_in[0].Normal;
    color = data_in[0].color;
    texCoord = data_in[0].texCoord;
    crntPos = vec3(gl_in[0].gl_Position);
    EmitVertex();

    gl_Position = data_in[1].projection * gl_in[1].gl_Position;
    Normal = data_in[1].Normal;
    color = data_in[1].color;
    texCoord = data_in[1].texCoord;
    crntPos = vec3(gl_in[1].gl_Position);
    EmitVertex();

    gl_Position = data_in[2].projection * gl_in[2].gl_Position;
    Normal = data_in[2].Normal;
    color = data_in[2].color;
    texCoord = data_in[2].texCoord;
    crntPos = vec3(gl_in[2].gl_Position);
    EmitVertex();

    EndPrimitive();
}
