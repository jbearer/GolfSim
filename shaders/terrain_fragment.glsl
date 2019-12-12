#version 330 core

uniform uint mesh;
    // Is this fragment part of the mesh, or just a normal face fragment?

in vec4 frag_color;
out vec4 color;

void main()
{
    color = bool(mesh) ? vec4(0.2, 0.2, 0.2, 1) : frag_color;
        // Color the mesh lines dark gray, and the rest of the terrain whatever
        // texture it is supposed to be.
}
