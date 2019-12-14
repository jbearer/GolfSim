#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 vert_color;
layout(location = 4) in vec3 vert_normal;
uniform mat4 mvp;
    // The model-view-projection matrix.

out vec4 frag_color;
out vec3 frag_normal;

void main()
{
    gl_Position = mvp * vec4(position, 1);
    frag_color = vert_color;
    frag_normal = vert_normal;
        // Unlike the position output, which we multiplied by MVP to convert
        // from terrain-space to clip-space, the normal vector stays in terrain-
        // space. This is allowed because the normal vector is only used to
        // compare with the light position, which means as long as both of those
        // vectors are in the same space, we are fine, and sure enough the light
        // position is given in terrain space. This trick saves us some
        // computation, and it also avoids some of the tricky problems that come
        // up when trying to apply a transformation matrix to normal vectors.
}
