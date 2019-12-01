#version 330 core

in vec3 frag_color;

out vec4 color;


void main()
{
    color.xyz = frag_color;
    color.a = 0.75;
}
