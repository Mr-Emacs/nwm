#version 330 core

layout(location = 0) in vec2 a_pos;

out vec2 v_uv;

uniform float u_scale;
uniform vec2  u_offset;

void main() {
    vec2 uv = a_pos * 0.5 + 0.5;

    // NOTE:X11 stores rows top to bottom OpenGL textures are bottom to top so flip Y
    uv.y = 1.0 - uv.y;
    v_uv = (uv - 0.5) / u_scale + 0.5 + u_offset;

    gl_Position = vec4(a_pos, 0.0, 1.0);
}
