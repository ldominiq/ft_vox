//
// Created by lucas on 6/25/25.
//

#ifndef APP_HPP
#define APP_HPP

#include "Camera.hpp"
#include "Renderer.hpp"
#include "Chunk.hpp"
#include <GLFW/glfw3.h>

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
    void toggleDisplayMode();

    unsigned int VAO, VBO, EBO, shaderProgram, texture;

    enum class DisplayMode {
        Windowed,
        Fullscreen
    };
    DisplayMode displayMode = DisplayMode::Windowed;

    Camera* camera;
    GLFWmonitor* monitor;
    const GLFWvidmode* mode;
    Chunk* chunk;
    GLuint textureShader;
    GLuint gradientShader;
    GLuint* activeShader;   // pointer to the currently active shader program

    float lastX = 400, lastY = 300;
    bool firstMouse = true;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;


    bool wireframe = false;


    float lastTitleUpdate = 0.0f;
    int frameCount = 0;


    int windowedX = 100;
    int windowedY = 100;
    int windowedWidth = 1280;
    int windowedHeight = 720;

    bool useGradientShader = false;    
};

#endif //APP_HPP
