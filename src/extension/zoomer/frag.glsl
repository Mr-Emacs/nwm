#version 330 core

in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_texture;

void main() {
    // TODO: Show solid black for any UV coordinates outside [0, 1]
    // Improve it with better color
    if (any(lessThan(v_uv, vec2(0.0))) || any(greaterThan(v_uv, vec2(1.0)))) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    frag_color = texture(u_texture, v_uv);
}
