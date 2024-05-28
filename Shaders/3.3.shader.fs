#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture2;
uniform float mixValue;

uniform vec3 lightColor;
uniform vec3 objectColor;
uniform vec3 lightPos;

void main()
{
    float f = (1 + gl_PrimitiveID % 4) / 5.;
    //FragColor = mix(texture(texture1, TexCoord), vec4(f, f, f , 1.0f), mixValue);

    // ambiant light
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // diffuse light
    // when doing lighting calculations, make sure you always normalize the relevant vectors to ensure they're actual unit vectors
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 result = (ambient + diffuse) * objectColor;
//     FragColor = vec4(result, 1.0);
    FragColor = texture(texture_diffuse1, TexCoord);

    //FragColor = vec4(lightColor * objectColor, 1.0f);
}