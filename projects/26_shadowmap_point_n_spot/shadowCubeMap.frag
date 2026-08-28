#version 330 core
in vec4 FragPos;

uniform vec3 lightPos;
uniform float shadowFarPlane;

// Stores radial distance, not projected z. A point light has six frustums, so a
// z from one is meaningless in another; distance is the same scalar in all six.
// The divide normalises it into [0,1]; default.frag multiplies it back out.
// Writing gl_FragDepth costs early-z, which is why this pass is the expensive one.
void main()
{
    gl_FragDepth = length(FragPos.xyz - lightPos) / shadowFarPlane;
}
