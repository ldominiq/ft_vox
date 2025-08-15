#ifndef NOISE_HPP
#define NOISE_HPP

#include <cmath>
#include <cstdint>
#include <iostream>

typedef struct {
    float x;
    float y;
} vector2;

typedef struct {
    float x;
    float y;
    float z;
} vector3;

class Noise {
public:
    Noise(unsigned seed = 1337);
    ~Noise();

    float perlin2D(float x, float y);
    float perlin3D(float x, float y, float z);
    float fractalBrownianMotion2D(float x, float y, int octaves, float lacunarity, float persistence);
    float getNoise(float x, float y);
    float getNoise(float x, float y, float z);

    void setSeed(unsigned seed) {
        mSeed = seed;
    }

    void setFrequency(float frequency) {
        mFrequency = frequency;
    }

private:
    unsigned mSeed;
    float mFrequency;

    float dotGridGradient(int ix, int iy, float x, float y);
    float dotGridGradient(int ix, int iy, int iz, float x, float y, float z);
    vector2 randomGradient(int ix, int iy);
    vector3 randomGradient(int ix, int iy, int iz);
    float interpolate(float a0,float a1, float w);
};

#endif // NOISE_HPP