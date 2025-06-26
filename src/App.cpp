//
// Created by lucas on 6/25/25.
//

#include "App.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image/stb_image.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

GLFWwindow* window;
Renderer* renderer;

static unsigned int loadShader(const char* vertPath, const char* fragPath);
static unsigned int loadTexture(const char* path);

App::App() {}
App::~App() { cleanup(); }

void App::init() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    window = glfwCreateWindow(1280, 720, "ft_vox", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    renderer = new Renderer();
    
    glEnable(GL_DEPTH_TEST);
    
    // enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Mouse movement event handling
    camera = new Camera(glm::vec3(0.0f, 2.0f, 0.0f));
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) {
        static App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(w));
        if (app) {
            if (app->firstMouse) {
                app->lastX = xpos;
                app->lastY = ypos;
                app->firstMouse = false;
            }
            float xoffset = xpos - app->lastX;
            float yoffset = app->lastY - ypos; // Reversed: y-coordinates go from bottom to top
            
            app->lastX = xpos;
            app->lastY = ypos;
            
            app->camera->processMouseMovement(xoffset, yoffset);
        }
    });
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    

}

void App::loadResources() {
    // Load shaders and textures

    shaderProgram = loadShader("shaders/simple.vert", "shaders/simple.frag");
    texture = loadTexture("assets/textures/dirt.png");

    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
}

void App::render() {

    while (!glfwWindowShouldClose(window)) {
        
        // Calculate delta time for frame rate
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        updateWindowTitle();
        processInput();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUseProgram(shaderProgram);

        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(80.0f), 1280.0f / 720.0f, 0.1f, 160.0f);
    

        // Set the uniform matrices in the shader
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        for (int x = -8; x <= 8;x++){
            for (int z = -8; z <= 8; z++){
                renderer->drawCube(glm::vec3(x, 0, z), shaderProgram);
            }
        }

        // Swap buffers and poll events (keys pressed, mouse movement, etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void App::run() {
    init();
    loadResources();
    render();
}

void App::cleanup() {
    glfwTerminate();
}

void App::processInput(){
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera->processKeyboard(Camera_Movement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera->processKeyboard(Camera_Movement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera->processKeyboard(Camera_Movement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera->processKeyboard(Camera_Movement::RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void App::updateWindowTitle() {
    float currentFrame = glfwGetTime();
    frameCount++;

    if (currentFrame - lastTitleUpdate >= 0.5f) {
        float fps = frameCount / (currentFrame - lastTitleUpdate);
        float msPerFrame = 1000.0f / fps;

        std::string title = "OpenGL (ft_vox) - " +
            std::to_string((int)fps) + " FPS / " +
            std::to_string(msPerFrame).substr(0, 5) + " ms";

        glfwSetWindowTitle(window, title.c_str());

        lastTitleUpdate = currentFrame;
        frameCount = 0;
    }

}

static unsigned int loadShader(const char* vertPath, const char* fragPath) {
    std::ifstream vFile(vertPath), fFile(fragPath);
    std::stringstream vStream, fStream;
    vStream << vFile.rdbuf();
    fStream << fFile.rdbuf();
    std::string vStr = vStream.str(), fStr = fStream.str();
    const char* vCode = vStr.c_str();
    const char* fCode = fStr.c_str();

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vCode, NULL);
    glCompileShader(vert);

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fCode, NULL);
    glCompileShader(frag);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    glDeleteShader(vert);
    glDeleteShader(frag);

    return prog;
}

static unsigned int loadTexture(const char* path) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (data) {
        GLenum format = ch == 4 ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture\n";
    }
    stbi_image_free(data);
    return texID;
}
