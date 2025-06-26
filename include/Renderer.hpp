#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <vector>

class Renderer {
public:
    Renderer();
    ~Renderer();

    void drawInstances(const std::vector<glm::vec3>& positions, GLuint shaderProgram);

private:
    GLuint VAO, VBO, instanceVBO, EBO;
    void initCubeMesh();
};

#endif