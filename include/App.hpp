//
// Created by lucas on 6/25/25.
//

#ifndef APP_HPP
#define APP_HPP

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Camera.hpp"
#include "Chunk.hpp"
#include "World.hpp"
#include "Skybox.hpp"
#include "Shader.hpp"
#include "stb_image.h"

#include <fstream>
#include <sstream>
#include <iostream>
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>

#define CONTROL_LIST 		\
    X(FORWARD)       		\
    X(BACKWARD)      		\
    X(LEFT)          		\
    X(RIGHT)         		\
    X(UP)            		\
    X(DOWN)             	\
    X(MOVE_FAST)        	\
    X(LEFT_CLICK)       	\
    X(TOGGLE_FULLSCREEN)	\
    X(TOGGLE_WIREFRAME)		\
    X(TOGGLE_SHADER)		\
    X(CLOSE_WINDOW)			\

enum controls {
#define X(name) name,
    CONTROL_LIST
#undef X
    CONTROL_COUNT
};


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

    void loadControlsDefaults();
	void saveControls(const char* filename = "controls.cfg");
	void loadControlsFromFile(const char* filename = "controls.cfg");

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
    World* world;
    Skybox* skybox;
    Shader* textureShader;
    Shader* gradientShader;
    Shader* activeShader;   // pointer to the currently active shader program

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

    bool useGradientShader = true;

    float renderDistance = 1000.0f; // Distance of the far clipping plane

	//keeps track of control GLFW values
    int controlsArray[CONTROL_COUNT];
	//keeps track of control names so they can be inserted/read from the .config file
	const char* controlNames[CONTROL_COUNT] = {
	#define X(name) #name,
		CONTROL_LIST
	#undef X
	};
};

#endif //APP_HPP
