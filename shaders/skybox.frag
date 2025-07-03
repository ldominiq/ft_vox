#version 330 core
in vec3 TexCoord;
out vec4 FragColor;

uniform samplerCube skybox;

void main() {
    FragColor = texture(skybox, TexCoord);
}
