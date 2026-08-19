#version 330 core

out vec4 FragColor;
in vec2 texCoords;

// The offscreen color attachment holding the scene pass; bound to unit 0.
uniform sampler2D screenTexture;

void main()
{
    // Straight copy for now — any post-processing (invert, blur, tonemap)
    // would operate on this sampled color before it is written out.
    FragColor = texture(screenTexture, texCoords);
}