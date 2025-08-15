#include "Noise.hpp"

Noise::Noise(unsigned seed) : mSeed(seed), mFrequency(0.01f) {

}

Noise::~Noise() {

}

float Noise::getNoise(float x, float y) {
    return perlin2D(x, y);
}

float Noise::getNoise(float x, float y, float z) {
    x *= 0.1f;
    y *= 0.1f;
    z *= 0.1f;

    const float R3 = (float)(2.0 / 3.0);
    float r = (x + y + z) * R3; // Rotation, not skew
    x = r - x;
    y = r - y;
    z = r - z;

    return perlin3D(x, y, z);
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

// Sample 3D Perlin noise at coordinates (x, y, z)
float Noise::perlin3D(float x, float y, float z) {
    // Determine grid cell corner coordinates
    int x0 = static_cast<int>(floor(x));
    int y0 = static_cast<int>(floor(y));
    int z0 = static_cast<int>(floor(z));
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    int z1 = z0 + 1;

    // Compute Interpolation weights
    float sx = x - static_cast<float>(x0);
    float sy = y - static_cast<float>(y0);
    float sz = z - static_cast<float>(z0);

    // Compute and interpolate front two corners
    float n000 = dotGridGradient(x0, y0, z0, x, y, z);
    float n100 = dotGridGradient(x1, y0, z0, x, y, z);
    float n010 = dotGridGradient(x0, y1, z0, x, y, z);
    float n110 = dotGridGradient(x1, y1, z0, x, y, z);

    float n001 = dotGridGradient(x0, y0, z1, x, y, z);
    float n101 = dotGridGradient(x1, y0, z1, x, y, z);
    float n011 = dotGridGradient(x0, y1, z1, x, y, z);
    float n111 = dotGridGradient(x1, y1, z1, x, y, z);

    // interpolate along x for each pair of front and back corners
    float ix00 = interpolate(n000, n100, sx);
    float ix01 = interpolate(n001, n101, sx);
    float ix10 = interpolate(n010, n110, sx);
    float ix11 = interpolate(n011, n111, sx);

    // interpolate along y for the two interpolated x values
    float iy0 = interpolate(ix00, ix10, sy);
    float iy1 = interpolate(ix01, ix11, sy);

    // Interpolate between the two previously interpolated values, now in z
    float value = interpolate(iy0, iy1, sz);
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

float Noise::dotGridGradient(int ix, int iy, int iz, float x, float y, float z) {
    // Get gradient vector from integer coordinates
    vector3 gradient = randomGradient(ix, iy, iz);

    // Compute the distance vector
    float dx = x - static_cast<float>(ix);
    float dy = y - static_cast<float>(iy);
    float dz = z - static_cast<float>(iz);

    // Compute the dot product
    return (dx * gradient.x + dy * gradient.y + dz * gradient.z);

}

vector2 Noise::randomGradient(int ix, int iy) {
    // No precomputed gradients mean this works for any number of grid coordinates;
    const unsigned w = 8 * sizeof(unsigned);
    const unsigned s = w / 2;
    unsigned a = ix + mSeed;
    unsigned b = iy + mSeed * 31;

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

vector3 Noise::randomGradient(int ix, int iy, int iz) {
    // No precomputed gradients mean this works for any number of grid coordinates;
    const unsigned w = 8 * sizeof(unsigned);
    const unsigned s = w / 2;
    unsigned a = ix + mSeed;
    unsigned b = iy + mSeed * 31;
    unsigned c = iz + mSeed * 7;

    a *= 3284157443;

    b ^= a << s | a >> (w - s);
    b *= 1911520717;

    c ^= b << s | b >> (w - s);
    c *= 2048419325;

    // Convert hash to two random floats in [0,1]
    float rnd1 = (a & 0xFFFFFF) / float(0xFFFFFF);
    float rnd2 = (b & 0xFFFFFF) / float(0xFFFFFF);

    // Spherical coordinates
    float theta = rnd1 * 2.0f * M_PI; // azimuth
    float phi   = acos(2.0f * rnd2 - 1.0f); // inclination

    // Create the vector from the angle
    vector3 v;
    v.x = sin(phi) * cos(theta);
    v.y = sin(phi) * sin(theta);
    v.z = cos(phi);

    return v;
}

float Noise::interpolate(float a0, float a1, float w) {
    return (a1 - a0) * (3.0 - w * 2.0) * w * w + a0;
}