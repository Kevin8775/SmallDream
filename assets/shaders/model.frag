#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vTexCoord;
out vec4 fragColor;

uniform vec3 uLightDir = normalize(vec3(-0.4, -1.0, -0.2));
uniform vec3 uBaseColor = vec3(0.82, 0.82, 0.86);
uniform sampler2D uTexture0;
uniform int uHasTexture = 0;

void main() {
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, -uLightDir), 0.15);
    vec3 base = uBaseColor;
    if (uHasTexture == 1) {
        base *= texture(uTexture0, vTexCoord).rgb;
    }
    vec3 col = base * diff;
    fragColor = vec4(col, 1.0);
}
