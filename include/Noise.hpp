#ifndef NOISE_HPP
#define NOISE_HPP

#include <cmath>

typedef struct {
    float x;
    float y;
} vector2;

class Noise {
public:
    Noise(unsigned seed = 1337);
    ~Noise();

    float perlin2D(float x, float y);
    float fractalBrownianMotion2D(float x, float y, int octaves, float lacunarity, float persistence);

private:
    unsigned seed;
    float dotGridGradient(int ix, int iy, float x, float y);
    vector2 randomGradient(int ix, int iy);
    float interpolate(float a0,float a1, float w);
};

#endif // NOISE_HPP