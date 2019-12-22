#version 330 core

layout(location = 0) in vec3 position;
uniform mat4 mvp;
    // The model-view-projection matrix.

void main()
{
    gl_Position = mvp * vec4(position, 1);
}
