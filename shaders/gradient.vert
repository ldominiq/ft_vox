#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec3 aOffset;

out float blockY;

uniform mat4 view;
uniform mat4 projection;

void main() {
    vec3 worldPos = aPos + aOffset;
    gl_Position = projection * view * vec4(worldPos, 1.0);

    blockY = aOffset.y;
}
