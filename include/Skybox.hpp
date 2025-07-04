#ifndef SKYBOX_HPP
#define SKYBOX_HPP

#include "Shader.hpp"
#include "stb_image.h"
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

class Skybox {
public:
    Skybox(const std::vector<std::string>& faces);

    void draw(const glm::mat4& view, const glm::mat4& projection);

private:
    GLuint skyboxVAO, skyboxVBO;
    GLuint cubemapTexture;
    Shader* shader;

    GLuint loadCubemap(const std::vector<std::string>& faces);
};

#endif