#ifndef SKYBOX_HPP
#define SKYBOX_HPP

#include "Shader.hpp"
#include "stb_image.h"
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

class Skybox {
public:
    explicit Skybox(const std::vector<std::string>& faces);

    void draw(const glm::mat4& view, const glm::mat4& projection) const;

private:
    GLuint skyboxVAO{}, skyboxVBO{};
    GLuint cubemapTexture;
    std::unique_ptr<Shader> shader;

    static GLuint loadCubemap(const std::vector<std::string>& faces);
};

#endif