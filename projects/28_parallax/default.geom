#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in DATA
{
	vec3 normal;
	vec2 texCoord;
	mat4 projection;
	mat4 model;
	vec3 lightPos;
	vec3 camPos;
} data_in[];

out vec2 texCoord;
// All three are in TANGENT space, not world space. Named so the fragment shader
// cannot silently mix them with a world-space vector.
out vec3 fragPosTangent;
out vec3 lightPosTangent;
out vec3 camPosTangent;

// Builds the tangent frame per TRIANGLE and moves the lighting into it.
//
// The alternative is to send TBN to the fragment shader and rotate the sampled
// normal into world space per fragment. This direction is cheaper: the light and
// camera are rotated once per vertex here, and the sampled normal is then already
// in the space the lighting is done in, so the fragment shader uses it untouched.
//
// A geometry shader is what makes it possible at all -- a tangent needs two edges
// and two UV deltas, so it needs the whole triangle, which no vertex shader sees.
void main()
{
	// Edges of the triangle, and the UV deltas across those same edges.
	vec3 edge0 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 edge1 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec2 deltaUV0 = data_in[1].texCoord - data_in[0].texCoord;
	vec2 deltaUV1 = data_in[2].texCoord - data_in[0].texCoord;

	// Solving the 2x2 system that maps UV steps onto world-space steps. The
	// tangent is the world direction you move in when U increases, which is what
	// ties the map's red axis to the surface.
	float invDet = 1.0f / (deltaUV0.x * deltaUV1.y - deltaUV1.x * deltaUV0.y);

	vec3 tangent   = invDet * ( deltaUV1.y * edge0 - deltaUV0.y * edge1);
	vec3 bitangent = invDet * (-deltaUV1.x * edge0 + deltaUV0.x * edge1);

	vec3 T = normalize(vec3(data_in[0].model * vec4(tangent, 0.0f)));
	vec3 B = normalize(vec3(data_in[0].model * vec4(bitangent, 0.0f)));
	// The AUTHORED normal, not cross(edge1, edge0). That cross matches the
	// tutorial's XZ plane but yields -Z on this XY one, which flips the frame and
	// inverts the lighting. The vertex normal does not depend on winding.
	vec3 N = normalize(vec3(data_in[0].model * vec4(data_in[0].normal, 0.0f)));

	// Columns T, B, N map tangent -> world. Transposing inverts it for an
	// orthonormal basis, so this maps world -> tangent, which is the direction
	// the three positions below need.
	mat3 TBN = transpose(mat3(T, B, N));

	for (int i = 0; i < 3; i++)
	{
		gl_Position = data_in[i].projection * gl_in[i].gl_Position;

		texCoord = data_in[i].texCoord;

		fragPosTangent  = TBN * gl_in[i].gl_Position.xyz;
		lightPosTangent = TBN * data_in[i].lightPos;
		camPosTangent   = TBN * data_in[i].camPos;

		EmitVertex();
	}

	EndPrimitive();
}
