//
// Created by lucas on 6/25/25.
//

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

    unsigned int VAO, VBO, EBO, shaderProgram, texture;
};

#endif //APP_HPP
