#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 vDir;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

void main() {
    vDir = aPos;
    vec4 pos = uProjection * uView * uModel * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
