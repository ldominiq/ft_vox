#version 330 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D atlas;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

void main() {
    vec4 texColor = texture(atlas, TexCoord);

    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, -lightDir), 0.0);

    vec3 lighting = texColor.rgb * (ambientColor + lightColor * diff);

    //FragColor = vec4(texColor.rgb * vLight, texColor.a);
    FragColor = vec4(lighting, texColor.a);
    //FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0); // Visualize normals
    //FragColor = vec4(TexCoord, 0.0, 1.0); // Visualize texture coordinates
}