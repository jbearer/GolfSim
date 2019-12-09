#version 330 core

uniform uint mesh;
    // Is this fragment part of the mesh, or just a normal face fragment?

out vec4 color;

void main()
{
    color = bool(mesh) ? vec4(0.2, 0.2, 0.2, 1) : vec4(1, 1, 1, 1);
        // Color the mesh lines dark gray, and the rest of the terrain white for
        // now.
}
