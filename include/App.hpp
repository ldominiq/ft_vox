//
// Created by lucas on 6/25/25.
//

#include "Camera.hpp"
#ifndef APP_HPP
#define APP_HPP

class App {
public:
    App();
    ~App();

    void run();

private:
    void init();
    void loadResources();
    void render();
    void cleanup();
    void processInput();

    unsigned int VAO, VBO, EBO, shaderProgram, texture;

    Camera* camera;
    float lastX = 400, lastY = 300;
    bool firstMouse = true;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    bool wireframe = false;
};

#endif //APP_HPP
