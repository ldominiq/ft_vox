//
// Created by Lucas on 19.12.2023.
//

#ifndef OPENGL_MESH_HPP
#define OPENGL_MESH_HPP

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Shader.hpp"
#include "Libraries/include/glad/glad.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
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
