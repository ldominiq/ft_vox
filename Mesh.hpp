//
// Created by Lucas on 19.12.2023.
//

#ifndef OPENGL_MESH_HPP
#define OPENGL_MESH_HPP

#include <string>
#include <vector>
#include "Maths/c_math_inc.hpp"
#include "Shader.hpp"
#include "Libraries/include/glad/glad.h"

struct Vertex {
    c_math::vec3 Position;
    c_math::vec3 Normal;
    c_math::vec2 TexCoords;
};

struct Texture {
	int id;
    std::string type;
};

class Mesh {
public:
    // mesh data
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    Mesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, std::vector<Texture> &textures);

    void Draw(Shader &shader);
private:
    // render data
    unsigned int VAO, VBO, EBO;

    void setupMesh();
};


#endif //OPENGL_MESH_HPP
