#version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    sampler2D emission;
    float shininess;
};

struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform float mixValue;

uniform vec3 viewPos;
uniform Material material;
uniform Light light;
uniform float time;

void main()
{
//    float f = (1 + gl_PrimitiveID % 4) / 5.;
    //FragColor = mix(texture(texture1, TexCoord), vec4(f, f, f , 1.0f), mixValue);
//     FragColor = texture(texture_diffuse1, TexCoord);

    // ambient
    vec3 ambient = light.ambient * texture(material.diffuse, TexCoord).rgb; // ambient material's color equal to the diffuse material's color

    // diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texture(material.diffuse, TexCoord).rgb; // sample from the texture to retrieve the fragment's diffuse color value

    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess * 128);
    vec3 specular = light.specular * spec * texture(material.specular, TexCoord).rgb;

    // emission
    vec3 emission = vec3(0.0);
    if (texture(material.specular, TexCoord).r == 0.0)   /*rough check for blackbox inside spec texture */
    {
    /*apply emission texture */
        emission = texture(material.emission, TexCoord).rgb;
        emission = emission * vec3(1.0, 0.0, 0.0) * 3.0;  /*multiply by white to get the color of the texture (no tinting) */

    /*some extra fun stuff with "time uniform" */
//        emission = texture(material.emission, TexCoord + vec2(0.0,time)).rgb;   /*moving */
//        emission = emission * (sin(time) * 0.5 + 0.5) * 2.0;                     /*fading */
    }

    vec3 result = ambient + diffuse + specular + emission;
    FragColor = vec4(result, 1.0);
}