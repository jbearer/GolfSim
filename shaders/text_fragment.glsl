#version 330 core

in vec2 uv;
flat in uint cursor;

out vec4 color;

uniform sampler2D font;
uniform vec4 fg_color;
uniform vec4 bg_color;

void main()
{
    float alpha = texture(font, uv).w;
        // The texture is black-and-white, so we only care about the alpha
        // channel, not the value.

    // For most fragments, we set the color by blending the foreground color
    // with alpha `alpha` and the background color with alpha `1 - alpha`. For
    // fragments within a character, `alpha` will be `1`, so character fragments
    // get the foreground color. Fragments outside a character will have alpha
    // `0`, and will get the background color.
    //
    // For fragments under the cursor (`cursor == 1`), we swap the alpha values
    // that we give to the foreground and background colors. This has the effect
    // of swapping the foreground and background color to indicate the position
    // of the cursor.
    color = (bool(cursor) ? 1-alpha : alpha  )*fg_color +
            (bool(cursor) ? alpha   : 1-alpha)*bg_color;
}
