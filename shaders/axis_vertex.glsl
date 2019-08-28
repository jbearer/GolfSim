#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 vert_color;
uniform mat4 mvp;
    // The model-view-projection matrix.

out vec3 frag_color;

void main()
{
    gl_Position = mvp * vec4(position, 1);
    frag_color = vert_color;
}
