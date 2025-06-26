#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>

class Renderer {
public:
    Renderer();
    ~Renderer();

    void drawCube(const glm::vec3&position, GLuint shaderProgram);

private:
    GLuint VAO, VBO;
    void initCubeMesh();
};

#endif