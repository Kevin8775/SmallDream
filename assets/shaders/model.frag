#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vTexCoord;
out vec4 fragColor;

uniform vec3 uBaseColor = vec3(0.82, 0.82, 0.86);
uniform sampler2D uTexture0;
uniform int uHasTexture = 0;

uniform vec3 uAmbientColor = vec3(0.18, 0.15, 0.12);

uniform vec3 uMainLightDir = vec3(0.0, 1.0, 0.3);
uniform vec3 uMainLightColor = vec3(1.0, 0.85, 0.55);
uniform float uMainLightIntensity = 1.0;

uniform vec3 uFillLightDir = vec3(-0.6, 0.1, 0.8);
uniform vec3 uFillLightColor = vec3(0.3, 0.4, 0.7);
uniform float uFillLightIntensity = 0.5;

uniform vec3 uCamPos = vec3(0.0);
uniform float uShininess = 24.0;
uniform float uSpecIntensity = 0.25;

void main() {
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCamPos - vWorldPos);

    vec3 base = uBaseColor;
    if (uHasTexture == 1) {
        base *= texture(uTexture0, vTexCoord).rgb;
    }

    vec3 lighting = uAmbientColor;

    vec3 l1 = normalize(uMainLightDir);
    float diff1 = max(dot(n, l1), 0.0);
    vec3 h1 = normalize(l1 + v);
    float spec1 = pow(max(dot(n, h1), 0.0), uShininess) * uSpecIntensity;
    lighting += (diff1 + spec1) * uMainLightColor * uMainLightIntensity;

    vec3 l2 = normalize(uFillLightDir);
    float diff2 = max(dot(n, l2), 0.0);
    vec3 h2 = normalize(l2 + v);
    float spec2 = pow(max(dot(n, h2), 0.0), uShininess) * uSpecIntensity * 0.4;
    lighting += (diff2 + spec2) * uFillLightColor * uFillLightIntensity;

    vec3 col = base * lighting;
    fragColor = vec4(col, 1.0);
}
