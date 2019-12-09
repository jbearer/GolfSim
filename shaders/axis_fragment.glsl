#version 330 core

in vec3 frag_color;

out vec4 color;

void main()
{
    color = vec4(frag_color.xyz, 1 - gl_FragCoord.z);
        // We use 1 - z for the alpha channel, so that the axes get darker as
        // they move away from the camera. This makes it possible to visually
        // determine which way the coordinate system is oriented without playing
        // around with it dynamically. This also makes it possible to
        // distinguish between a left-handed coordinate system (which is a bug)
        // and a right-handed one just by looking.
}
