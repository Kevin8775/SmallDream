#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;

void main() {
    vec4 tex = texture(uTexture, vUV);
    fragColor = tex * uColor;
    if (fragColor.a < 0.01) discard;
}
