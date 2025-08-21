#version 330 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D atlas;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

float near = 0.1;
float far  = 100.0;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main() {
    vec4 texColor = texture(atlas, TexCoord);

    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, -lightDir), 0.0);

    vec3 lighting = texColor.rgb * (ambientColor + lightColor * diff);

    //FragColor = texColor;
    FragColor = vec4(lighting, texColor.a); // Lighting
    //FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0); // Visualize normals
    //FragColor = vec4(TexCoord, 0.0, 1.0); // Visualize texture coordinates

    //float depth = LinearizeDepth(gl_FragCoord.z) / far; // Visualize depth buffer
    //FragColor = vec4(vec3(depth), 1.0);
}
