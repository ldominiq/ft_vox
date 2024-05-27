#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform float mixValue;

void main()
{
    float f = (1 + gl_PrimitiveID % 4) / 5.;
    FragColor = mix(texture(texture1, TexCoord), vec4(f, f, f , 1.0f), mixValue);
}