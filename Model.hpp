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
#include "./Libraries/include/stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


class Model {
public:
    explicit Model(std::string const &path, bool gamma = false);

    void Draw(Shader &shader);

private:
    bool gammaCorrection;
    std::vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    std::vector<Mesh> meshes;
    std::string directory;

    void loadModel(std::string const &path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);

    std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName);

    static unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma);
};

#endif //OPENGL_MODEL_HPP
