#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D uTexture;
uniform vec3 uTextColor;

void main() {    
    // Extraer el canal RED del mapa de bits de FreeType para utilizarlo como canal alfa (transparencia)
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(uTexture, TexCoords).r);
    color = vec4(uTextColor, 1.0) * sampled;
}