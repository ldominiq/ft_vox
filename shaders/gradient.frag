#version 330 core

in float blockY;
out vec4 FragColor;

// Simple HSV → RGB conversion
vec3 hsv2rgb(vec3 c) {
    vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0,4.0,2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return c.z * mix(vec3(1.0), rgb, c.y);
}

void main() {
    float yNorm = clamp(blockY / 256.0, 0.0, 1.0);  // Adjust max height here
    vec3 hsv = vec3(yNorm, 0.8, 0.9); // hue from height, sat & value fixed
    vec3 color = hsv2rgb(hsv);
    FragColor = vec4(color, 1.0);
}

