#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 uProjection;

void main() {
    // Multiplicación directa por la matriz ortográfica 2D pura sin transformaciones de modelo 3D
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    TexCoords = aTexCoords;
}