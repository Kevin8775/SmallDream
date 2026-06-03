#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform float uBlurAmount;

void main() {
    vec2 texel = vec2(1.0 / 1689.0, 1.0 / 917.0) * uBlurAmount;
    vec4 color = texture(uTexture, vUV) * 0.36;
    color += texture(uTexture, vUV + vec2(texel.x, 0.0)) * 0.16;
    color += texture(uTexture, vUV - vec2(texel.x, 0.0)) * 0.16;
    color += texture(uTexture, vUV + vec2(0.0, texel.y)) * 0.16;
    color += texture(uTexture, vUV - vec2(0.0, texel.y)) * 0.16;
    color += texture(uTexture, vUV + texel) * 0.04;
    color += texture(uTexture, vUV - texel) * 0.04;
    color += texture(uTexture, vUV + vec2(texel.x, -texel.y)) * 0.04;
    color += texture(uTexture, vUV + vec2(-texel.x, texel.y)) * 0.04;
    fragColor = color;
}
