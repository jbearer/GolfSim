#version 330 core

uniform mat3 mvp;

layout(location = 0) in vec2 position;
layout(location = 2) in vec2 vertex_uv;
layout(location = 3) in uint vertex_cursor;

out vec2 uv;
flat out uint cursor;

void main()
{
    gl_Position.xyw = mvp*vec3(position, 1);
        // Assign 1 to the w-coordinate, since these are position vectors. We
        // are given a 3x3 MVP matrix, since we're dealing with 2-dimensional
        // points, so we have to apply the matrix before we add in the z-
        // coordinate. Hence we assign to the `xyw` swizzle of gl_Position
        // first, with the transformed 2D coordinates, and then we will assign a
        // constant z-value.
    gl_Position.z   = -1;
        // Text fields are 2-dimensional, so they should all have the same z-
        // coordinate, which is why the z coordinate is not part of the input
        // data. We will give all of our vertices a z-coordinate of -1, which
        // places them on the front clipping plane.

    // Pass-throughs to the fragment shader.
    uv = vertex_uv;
    cursor = vertex_cursor;
}
