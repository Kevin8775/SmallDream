#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vTexCoord;
out vec4 fragColor;

uniform vec3 uBaseColor = vec3(0.82, 0.82, 0.86);
uniform sampler2D uTexture0;
uniform int uHasTexture = 0;
uniform vec3 uTintColor = vec3(1.0);
// Recorte alfa (alphaMode MASK). 0 = sin recorte (comportamiento por defecto).
uniform float uAlphaCutoff = 0.0;

uniform bool uFogEnabled = false;
uniform float uFogDensity = 0.02;
uniform vec3 uFogColor = vec3(0.8, 0.85, 0.9);

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

// ── Linterna cónica (spotlight). Desactivada por defecto (no afecta a otras escenas). ──
uniform int   uFlashlightEnabled = 0;
uniform vec3  uFlashPos      = vec3(0.0);
uniform vec3  uFlashDir      = vec3(0.0, 0.0, -1.0);  // hacia donde apunta el haz
uniform float uFlashInnerCos = 0.97;                  // cos del ángulo interno (núcleo)
uniform float uFlashOuterCos = 0.90;                  // cos del ángulo externo (borde suave)
uniform vec3  uFlashColor    = vec3(1.5, 1.42, 1.2);
uniform float uFlashRange    = 80.0;

void main() {
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCamPos - vWorldPos);

    vec3 base = uBaseColor;
    if (uHasTexture == 1) {
        vec4 texel = texture(uTexture0, vTexCoord);
        if (texel.a < uAlphaCutoff) discard;   // descarta téxeles transparentes (MASK)
        base *= texel.rgb;
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

    if (uFlashlightEnabled == 1) {
        vec3 toFrag = vWorldPos - uFlashPos;
        float dist  = length(toFrag);
        vec3 Ldir   = toFrag / max(dist, 0.0001);          // del foco al fragmento
        float theta = dot(Ldir, normalize(uFlashDir));     // 1 = justo al frente
        // Borde suave del cono entre el ángulo externo e interno
        float cone  = clamp((theta - uFlashOuterCos) / max(uFlashInnerCos - uFlashOuterCos, 0.0001), 0.0, 1.0);
        // Atenuación por distancia
        float atten = clamp(1.0 - dist / uFlashRange, 0.0, 1.0);
        atten *= atten;
        float beam  = cone * atten;
        float diffF = max(dot(n, -Ldir), 0.0);
        vec3  hF    = normalize(-Ldir + v);
        float specF = pow(max(dot(n, hF), 0.0), uShininess) * uSpecIntensity;
        lighting += uFlashColor * beam * (0.35 + 0.65 * diffF + specF);
    }

    vec3 col = base * lighting * uTintColor;
    if (uFogEnabled) {
        float d = length(vWorldPos - uCamPos);
        float fogFactor = 1.0 - exp(-uFogDensity * uFogDensity * d * d);
        col = mix(col, uFogColor, clamp(fogFactor, 0.0, 1.0));
    }
    fragColor = vec4(col, 1.0);
}
