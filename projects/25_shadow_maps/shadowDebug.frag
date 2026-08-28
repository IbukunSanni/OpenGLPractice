#version 330 core

// Debug view of the shadow map. Pairs with framebuffer.vert, which supplies an
// NDC quad and its texture coordinates -- the same vertex stage the (currently
// unused) post-processing rectangle was written for.

out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D depthMap;

void main()
{
    // A depth texture stores its value in the red channel.
    float depth = texture(depthMap, texCoords).r;

    // Texels no caster covered still hold the clear value of exactly 1.0.
    // Tinting those instead of drawing them white is what makes the silhouette
    // readable: an orthographic depth map of a small scene occupies a very
    // narrow band of values, so a straight greyscale view is nearly uniform and
    // tells you almost nothing.
    if (depth >= 1.0)
    {
        FragColor = vec4(0.20, 0.05, 0.05, 1.0);
        return;
    }

    FragColor = vec4(vec3(depth), 1.0);
}
