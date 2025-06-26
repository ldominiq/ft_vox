//
// Created by lucas on 6/25/25.
//

#ifndef APP_HPP
#define APP_HPP

#include "Camera.hpp"
#include "Renderer.hpp"

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
    void updateWindowTitle();

    unsigned int VAO, VBO, EBO, shaderProgram, texture;

    Camera* camera;
    float lastX = 400, lastY = 300;
    bool firstMouse = true;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    bool wireframe = false;

    float lastTitleUpdate = 0.0f;
    int frameCount = 0;
};

#endif //APP_HPP
