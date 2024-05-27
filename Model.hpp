//
// Created by Lucas on 19.12.2023.
//

#ifndef OPENGL_MODEL_HPP
#define OPENGL_MODEL_HPP

#include <string>
#include <vector>
#include <fstream>
#include "Shader.hpp"
#include "Mesh.hpp"
#include "objloader.h"
#include "./Libraries/include/stb_image.h"


class Model {
public:
    explicit Model(const char *path);

    void Draw(Shader &shader);
    bool loaded{ false };
private:
    std::vector<Mesh> meshes;
    std::string directory;

    void loadModel(const char *path);

    unsigned int TextureFromFile();
};

#endif //OPENGL_MODEL_HPP
