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

// ── Linterna cónica (flashlight) ──────────────────────────────────────────────
uniform int   uFlashlightEnabled  = 0;
uniform vec3  uFlashlightDir      = vec3(0.0, 0.0, -1.0);
uniform float uFlashlightInnerCos = 0.97;   // cos(~14°) — núcleo brillante
uniform float uFlashlightOuterCos = 0.90;   // cos(~26°) — borde del cono
uniform float uFlashlightRange    = 60.0;   // distancia máxima de iluminación
uniform vec3  uFlashlightColor    = vec3(1.0, 1.0, 1.0);
uniform float uFlashlightIntensity= 1.0;

// Niebla radial (usada fuera del cono para oscurecer lo que queda atrás)
uniform int   uFogEnabled   = 0;
uniform float uFogDensity   = 0.0;
uniform vec3  uFogColor     = vec3(0.0, 0.0, 0.0);

void main() {
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCamPos - vWorldPos);

    vec3 base = uBaseColor;
    if (uHasTexture == 1) {
        base *= texture(uTexture0, vTexCoord).rgb;
    }

    vec3 lighting = uAmbientColor;

    // Luces direccionales estándar
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

    // ── Linterna cónica ────────────────────────────────────────────────────
    if (uFlashlightEnabled == 1) {
        vec3  toFrag   = vWorldPos - uCamPos;
        float dist     = length(toFrag);
        vec3  fragDir  = toFrag / dist;

        // Ángulo entre el rayo hacia el fragmento y la dirección de la linterna
        float cosTheta = dot(fragDir, normalize(uFlashlightDir));

        // Factor del cono: 1 en el centro, 0 fuera del borde exterior
        float coneFactor = smoothstep(uFlashlightOuterCos, uFlashlightInnerCos, cosTheta);

        // Atenuación por distancia (cuadrática con rango máximo)
        float atten = 1.0 / (1.0 + 0.01 * dist + 0.004 * dist * dist);
        atten *= clamp(1.0 - dist / uFlashlightRange, 0.0, 1.0);

        // Difuso desde la posición de la cámara
        vec3  lDir   = -fragDir;
        float diff   = max(dot(n, lDir), 0.0);
        vec3  h      = normalize(lDir + v);
        float spec   = pow(max(dot(n, h), 0.0), uShininess) * uSpecIntensity;

        lighting += (diff + spec) * uFlashlightColor * uFlashlightIntensity
                    * coneFactor * atten;
    }

    vec3 col = base * lighting;

    // Niebla radial para oscurecer lo que queda fuera del haz
    if (uFogEnabled == 1) {
        float dist2    = length(vWorldPos - uCamPos);
        float fogFactor = 1.0 - exp(-uFogDensity * uFogDensity * dist2 * dist2);
        col = mix(col, uFogColor, clamp(fogFactor, 0.0, 1.0));
    }

    fragColor = vec4(col, 1.0);
}
