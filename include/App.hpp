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
// glm for vector types used in lighting controls
#include <glm/vec3.hpp>
#include <memory>
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
    X(TOGGLE_DEBUG)			\
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


	void saveWorldOnExit();

	void debugWindow();


    unsigned int VAO, VBO, EBO, shaderProgram, texture;

    enum class DisplayMode {
        Windowed,
        Fullscreen
    };
    DisplayMode displayMode = DisplayMode::Windowed;

    std::unique_ptr<Camera> camera;
	GLFWmonitor* monitor;
    const GLFWvidmode* mode;

    std::unique_ptr<World> world;
    std::unique_ptr<Skybox> skybox;
    std::shared_ptr<Shader> textureShader;
    std::shared_ptr<Shader> gradientShader;
    std::shared_ptr<Shader> activeShader;   // pointer to the currently active shader program

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

    float renderDistance = 1000.0f; // Distance of the far clipping plane

    // Lighting parameters that can be tweaked via ImGui.  The direction
    // should be normalised each frame; colours are in [0,1].
    glm::vec3 lightDir  = glm::vec3(-0.5f, -1.0f, -0.3f);
    glm::vec3 lightColor = glm::vec3(1.0f);
    glm::vec3 ambientColor = glm::vec3(0.3f);

    // Variables for smoothing the FPS shown in the debug UI.  We maintain a
    // moving average of frame times over a sample buffer to reduce jitter.
    std::vector<float> fpsSamples;
    static const size_t fpsSampleCount = 60;
    float uiDisplayFPS = 0.0f;

    // Helper to query the current process’s resident set size (RSS) in bytes.
    // Used in the debug UI to show approximate memory usage.
    static size_t getCurrentRSS();

    // --- ImGui / UI state ---
    // Whether the debug overlay is interactive.  When true the mouse is
    // released and the debug window captures input; when false the window
    // remains visible but does not capture input.
    bool uiInteractive = false;
    // Internal flag to handle key debounce for toggling the interactive mode.
    bool uiToggleHeld = false;
	bool showDebugWindow = true;

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
