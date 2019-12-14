#version 330 core

uniform uint mesh;
    // Is this fragment part of the mesh, or just a normal face fragment?
uniform vec3 light_position;
    // The position of the light in terrain-space.
uniform vec4 light_color;

in vec4 frag_color;
in vec3 frag_normal;
    // The normal vector for the fragment, in terrain-space.
out vec4 color;

const float ambient_strength = 0.4;
    // The fraction of illumination which is due to ambient light (light that
    // has bounced off of one or more surfaces and essentially comes equally
    // from all directions) as opposed to diffuse light (light striking a
    // surface directly from a source).
    //
    // In a large, outdoor environment such as a golf course, a lot of the light
    // is ambient light.
const float diffuse_strength = 1 - ambient_strength;

void main()
{
    if (bool(mesh)) {
        color = vec4(0.2, 0.2, 0.2, 1);
        // Color the mesh lines dark gray, and don't apply any shading.
        return;
    }

    vec4 ambient_color = frag_color;
        // The portion of the final color due to ambient lighting of the
        // terrain. This simulates the way light bouncing off of nearby surfaces
        // can illuminate even surfaces which aren't facing the light, and so it
        // doesn't depend on the position or orientation of the fragment.

    float cos_theta = clamp(
        dot(normalize(frag_normal), normalize(light_position)), 0, 1);
        // Cosine of the angle between the light and the normal vector, clamped
        // to [0, 1]. This will be 0 when the light is perpendicular to the
        // surface (or even if the angle is obtuse) and 1 when it is parallel.
        // We will use it to scale the color, so that surfaces facing away from
        // the light will be darker.
    vec4 diffuse_color = frag_color*light_color*cos_theta;
        // The portion of the final color due to light striking the terrain
        // directly from the sun (possibly at an angle theta).

    color = ambient_strength*ambient_color + diffuse_strength*diffuse_color;
}
