#version 330 core
out vec4 FragColor;

in float blockY;

void main() {
    float yNorm = clamp(blockY / 16.0, 0.0, 1.0); // Assuming max terrain height is ~64

    // From dark green (low) to yellow-white (high)
    vec3 lowColor = vec3(0.1, 0.3, 0.1);  // dark green
    vec3 highColor = vec3(1.0, 1.0, 0.7); // light yellowish

    vec3 finalColor = mix(lowColor, highColor, yNorm);
    FragColor = vec4(finalColor, 1.0);
}
