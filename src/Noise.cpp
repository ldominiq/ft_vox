#include "Noise.hpp"

Noise::Noise(unsigned seed) : seed(seed) {

}

Noise::~Noise() {

}

// Sample 2D Perlin noise at coordinates (x, y)
float Noise::perlin2D(float x, float y) {
    // Determine grid cell corner coordinates
    int x0 = static_cast<int>((floor(x)));
    int y0 = static_cast<int>((floor(y)));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // Compute Interpolation weights
    float sx = x - static_cast<float>(x0);
    float sy = y - static_cast<float>(y0);

    // Compute and interpolate top two corners
    float n0 = dotGridGradient(x0, y0, x, y);
    float n1 = dotGridGradient(x1, y0, x, y);
    float ix0 = interpolate(n0, n1, sx);

    // Compute and interpolate bottom two corners
    n0 = dotGridGradient(x0, y1, x, y);
    n1 = dotGridGradient(x1, y1, x, y);
    float ix1 = interpolate(n0, n1, sx);

    // Interpolate between the two previously interpolated values, now in y
    float value = interpolate(ix0, ix1, sy);

    return value;
}

// Fractal Brownian Motion (FBM) for 2D noise
float Noise::fractalBrownianMotion2D(float x, float y, int octaves, float lacunarity, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f; // Used for normalization
    
    for (int i = 0; i < octaves; ++i) {
        total += perlin2D(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;

        amplitude *= persistence; // Amplitude decay with each octave
        frequency *= lacunarity;  // Frequency growth with each octave
    }

    // Normalize the result to the range [-1, 1]
    return (total / maxValue);
}

// Computes the dot product between the gradient vector and the distance vector
float Noise::dotGridGradient(int ix, int iy, float x, float y) {
    // Get gradient vector from integer coordinates
    vector2 gradient = randomGradient(ix, iy);

    // Compute the distance vector
    float dx = x - static_cast<float>(ix);
    float dy = y - static_cast<float>(iy);

    // Compute the dot product
    return (dx * gradient.x + dy * gradient.y);

}

vector2 Noise::randomGradient(int ix, int iy) {
    // No precomputed gradients mean this works for any number of grid coordinates;
    const unsigned w = 8 * sizeof(unsigned);
    const unsigned s = w / 2;
    unsigned a = ix + seed;
    unsigned b = iy + seed * 31;

    a *= 3284157443;

    b ^= a << s | a >> (w - s);
    b *= 1911520717;

    a ^= b << s | b >> (w - s);
    a *= 2048419325;

    float random = a * (3.14159265 / ~(~0u >> 1)); // in [0, 2π]

    // Create the vector from the angle
    vector2 v;
    v.x = sin(random);
    v.y = cos(random);

    return v;
}

float Noise::interpolate(float a0, float a1, float w) {
    return (a1 - a0) * (3.0 - w * 2.0) * w * w + a0;
}