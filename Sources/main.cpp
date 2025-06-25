#include <iostream>
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "Shader.hpp"
#include "camera.h"
#include "Model.hpp"
// leak detection windows
// #include <vld.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

void Clear();
void show_infos(GLFWwindow* window);

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

float mixValue = 1.0f;

const float		step = 0.01;
static int		textured = 0;

static int      autorotate = 0;

// camera
Camera camera(glm::vec3(3.0f, 0.0f, 3.0f));

// model
glm::mat4 model = glm::mat4(1.0f);
glm::mat4 light = glm::mat4(1.0f);

bool firstMouse = true;
bool focused = true;
float lastX =  (float)SCR_WIDTH / 2.0;
float lastY =  (float)SCR_HEIGHT / 2.0;

// timing for movement
float timeDiff = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

// Timing for fps and ms counter
double prevTime = 0.0;
double currentTime = 0.0;
double deltaTime;
unsigned int counter = 0;

// lighting
glm::vec3 lightPos(-1.0f, 0.0f, 1.0f);
bool flashlight = false;

int DEBUG = 1;

int main(int argc, char **argv)
{
    if (!DEBUG) {
        if (argc != 2)
        {
            std::cout << "Usage: ./scop [model.obj]" << std::endl;
            return -1;
        }
        //check if argv[1] is .obj
        std::string file = argv[1];
        std::string extension = file.substr(file.find_last_of('.') + 1);
        if (extension != "obj")
        {
            std::cout << "Invalid file extension. Please provide a .obj file." << std::endl;
            return -1;
        }

        // check if file exists
        std::ifstream f(file.c_str());
        if (!f.good())
        {
            std::cout << "File does not exist." << std::endl;
            return -1;
        }
    }

	// GLFW: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// required on macos
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

	// GLFW: window creation
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL(FT_VOX)", nullptr, nullptr);
	if (window == nullptr)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// GLAD: load all OpenGL function pointers
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
		return -1;
	}

    stbi_set_flip_vertically_on_load(true);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW); // counter-clockwise winding order

	// Build and compile our shader program
	// ------------------------------------
    std::cout << "Loading shaders..." << std::endl;
	Shader lightingShader("Shaders/lighting.vert", "Shaders/lighting.frag");
    Shader lightCubeShader("Shaders/light.vert", "Shaders/light.frag");

    std::cout << "Loading models..." << std::endl;
    Model ourModel("Models/box/cube.obj");

    Model lightModel("Models/box/cube.obj");

    Model groundCubes("Models/grass/Grass_Block.obj");

//    if (!ourModel.loaded)
//    {
//        glfwTerminate();
//        return -1;
//    }
//    Model reference = ourModel;

    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
    model = glm::scale(model, glm::vec3(0.5f, .5f, .5f));	// it's a bit too big for our scene, so scale it down

    // removes VSync (breaks model movement because deltaTime is different)
    // glfwSwapInterval(0);

    // populate ground model without space
    // this is a 100x100 grid of cubes, each cube is 1x1x1
    std::vector<glm::mat4> groundModels;
    for (int i = -100; i < 100; ++i) {
        for (int j = -100; j < 100; ++j) {
            glm::mat4 groundModel = glm::mat4(1.0f);
            groundModel = glm::translate(groundModel, glm::vec3(i, -5.0f, j));
            groundModel = glm::scale(groundModel, glm::vec3(0.5f)); // scale it down to fit the scene
            groundModel = glm::rotate(groundModel, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            groundModels.push_back(groundModel);
        }
    }

    // populate list of models
    std::vector<glm::mat4> models;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
                // move the model spaced between -10 and 10
                model = glm::translate(model, glm::vec3(-1.0f + i, -1.0f + j, -1.0f + k));
                model = glm::scale(model, glm::vec3(0.2f));
                models.push_back(model);
            }
        }
    }

    // positions of the point lights
    glm::vec3 pointLightPositions[] = {
            glm::vec3( 0.0f,  0.0f,  2.0f),
            glm::vec3( 2.0f, -1.5f, -2.0f),
            glm::vec3(-2.0f,  1.0f, -2.0f)
    };

    glm::vec3 pointLightColors[] = {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0, 1.0)
    };

    float radius = 2.0f;
    float speed = 2.0f;

    // render loop
	while(!glfwWindowShouldClose(window))
	{
        // DEBUG
        show_infos(window);

        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        timeDiff = currentFrame - lastFrame;
        lastFrame = currentFrame;

		// input
		processInput(window);
        glfwSetKeyCallback(window, key_callback);

		// rendering commands here
		glClearColor(0.16f, 0.16f, 0.16f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // also clear the depth buffer now!

        lightingShader.setFloat("mixValue", mixValue);


        lightingShader.use();
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setFloat("material.shininess", 0.5f);
        lightingShader.setFloat("time", glfwGetTime());

        // directional light (sunlight)
       lightingShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
       lightingShader.setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
       lightingShader.setVec3("dirLight.diffuse", 0.4f, 0.4f, 0.4f);
       lightingShader.setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);
        // point light 1
        lightingShader.setVec3("pointLights[0].position", pointLightPositions[0]);
        lightingShader.setVec3("pointLights[0].ambient", pointLightColors[0].x * 0.1,  pointLightColors[0].y * 0.1,  pointLightColors[0].z * 0.1);
        lightingShader.setVec3("pointLights[0].diffuse", pointLightColors[0].x,  pointLightColors[0].y,  pointLightColors[0].z);
        lightingShader.setVec3("pointLights[0].specular", pointLightColors[0].x,  pointLightColors[0].y,  pointLightColors[0].z);
        lightingShader.setFloat("pointLights[0].constant", 1.0f);
        lightingShader.setFloat("pointLights[0].linear", 0.09f);
        lightingShader.setFloat("pointLights[0].quadratic", 0.032f);
        // point light 2
        lightingShader.setVec3("pointLights[1].position", pointLightPositions[1]);
        lightingShader.setVec3("pointLights[1].ambient", pointLightColors[1].x * 0.1,  pointLightColors[1].y * 0.1,  pointLightColors[1].z * 0.1);
        lightingShader.setVec3("pointLights[1].diffuse", pointLightColors[1].x,  pointLightColors[1].y,  pointLightColors[1].z);
        lightingShader.setVec3("pointLights[1].specular", pointLightColors[1].x,  pointLightColors[1].y,  pointLightColors[1].z);
        lightingShader.setFloat("pointLights[1].constant", 1.0f);
        lightingShader.setFloat("pointLights[1].linear", 0.09f);
        lightingShader.setFloat("pointLights[1].quadratic", 0.032f);
        // point light 3
        lightingShader.setVec3("pointLights[2].position", pointLightPositions[2]);
        lightingShader.setVec3("pointLights[2].ambient", pointLightColors[2].x * 0.1,  pointLightColors[2].y * 0.1,  pointLightColors[2].z * 0.1);
        lightingShader.setVec3("pointLights[2].diffuse", pointLightColors[2].x,  pointLightColors[2].y,  pointLightColors[2].z);
        lightingShader.setVec3("pointLights[2].specular", pointLightColors[2].x,  pointLightColors[2].y,  pointLightColors[2].z);
        lightingShader.setFloat("pointLights[2].constant", 1.0f);
        lightingShader.setFloat("pointLights[2].linear", 0.09f);
        lightingShader.setFloat("pointLights[2].quadratic", 0.032f);
        // spotLight

        lightingShader.setVec3("spotLight.position", camera.Position);
        lightingShader.setVec3("spotLight.direction", camera.Front);
        if (flashlight) {
            lightingShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
            lightingShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        }
        else {
            lightingShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("spotLight.diffuse", 0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("spotLight.specular", 0.0f, 0.0f, 0.0f);
        }
        lightingShader.setFloat("spotLight.constant", 1.0f);
        lightingShader.setFloat("spotLight.linear", 0.09f);
        lightingShader.setFloat("spotLight.quadratic", 0.032f);
        lightingShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        lightingShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));


        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);

        if (autorotate)
            model = glm::rotate(model, glm::radians(0.1f), glm::vec3(1.0f, 1.0f, 0.0f));

        // render the loaded model
        lightingShader.setMat4("model", model);
        ourModel.Draw(lightingShader);

        // render the loaded model
        for (unsigned int i = 0; i < models.size(); i++)
        {
            if (autorotate)
                models[i] = glm::rotate(models[i], glm::radians(0.1f), glm::vec3(1.0f, 1.0f, 0.0f));
            lightingShader.setMat4("model", models[i]);
            ourModel.Draw(lightingShader);
        }

        // render the ground cubes
        for (unsigned int i = 0; i < groundModels.size(); i++)
        {
            lightingShader.setMat4("model", groundModels[i]);
            groundCubes.Draw(lightingShader);
        }


        // render light source model
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);

        for (unsigned int i = 0; i < 3; i++)
        {
            float angle = currentFrame * speed + i * glm::radians(120.0f);
            pointLightPositions[i] = glm::vec3(radius * cos(angle), pointLightPositions[i].y, radius * sin(angle));

            light = glm::mat4(1.0f);
            light = glm::translate(light, pointLightPositions[i]);
            light = glm::scale(light, glm::vec3(0.1f)); // Make it a smaller cube
            lightCubeShader.setVec3("cubeColor", pointLightColors[i]);

            lightCubeShader.setMat4("model", light);
            lightModel.Draw(lightCubeShader);
        }



        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	(void) window;
    glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void) window;
	(void) scancode;
	(void) mods;

    if (key == GLFW_KEY_H && action == GLFW_PRESS)
        autorotate = !autorotate;

    // Handle texture on/off
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS && (mixValue == 0. || mixValue == 1.))
        textured = !textured;

    if (key == GLFW_KEY_L && action == GLFW_PRESS)
        flashlight = !flashlight;
}

void processInput(GLFWwindow *window)
{
    // Handle window close
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Handle wireframe, points, and fill modes
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(3.0f);
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glPointSize(8.0f);
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Handle camera movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, timeDiff);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, timeDiff);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, timeDiff);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, timeDiff);

    // Handle camera speed
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.MovementSpeed = 5.0f;
    else if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.MovementSpeed = 0.5f;
    else
        camera.MovementSpeed = 2.5f;

    // Handle model movements
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        // Move the model on the -X axis
        model = glm::translate(model, glm::vec3(-0.1f, 0.0f, 0.0f));
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        // Move the model on the X axis
        model = glm::translate(model, glm::vec3(0.1f, 0.0f, 0.0f));
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        // Move the model on the -Z axis
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.1f));
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        // Move the model on the Z axis
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.1f));
    }
    if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
    {
        // Move the model on the Y axis
        model = glm::translate(model, glm::vec3(0.0f, 0.1f, 0.0f));
    }
    if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
    {
        // Move the model on the -Y axis
        model = glm::translate(model, glm::vec3(0.0f, -0.1f, 0.0f));
    }

    // Handle model rotations
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
    {
        // Rotate the model counterclockwise around the Y axis
        model = glm::rotate(model, glm::radians(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        // Rotate the model clockwise around the Y axis
        model = glm::rotate(model, glm::radians(-1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // Handle model scaling
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
    {
        // Scale the model down
        model = glm::scale(model, glm::vec3(0.99f, 0.99f, 0.99f));
    }
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
    {
        // Scale the model up
        model = glm::scale(model, glm::vec3(1.01f, 1.01f, 1.01f));
    }

    // Reset model transformations
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
    {
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f, .5f, .5f));
    }

    // Reset camera position
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
    {
        camera.Position = glm::vec3(3.0f, 0.0f, 3.0f);
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    //    1. Calculate the mouse's offset since the last frame.
    //    2. Add the offset values to the camera's yaw and pitch values.
    //    3. Add some constraints to the minimum/maximum pitch values.
    //    4. Calculate the direction vector.

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    bool currentFocus = glfwGetWindowAttrib(window, GLFW_FOCUSED);

    // If focus changed, update cursor state
    if (currentFocus != focused) {
        focused = currentFocus;
        if (focused) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true; // reset to prevent jump on regain focus
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        return; // skip processing when focus just changed
    }

    if (!focused)
        return; // don't process movement if not focused

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);   
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	(void) window;
	(void) xoffset;
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void Clear()
{
#if defined _WIN32
    system("cls");
    //clrscr(); // including header file : conio.h
#elif defined (__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
    system("clear");
    //std::cout<< u8"\033[2J\033[1;1H"; //Using ANSI Escape Sequences
#elif defined (__APPLE__)
    system("clear");
#endif
}

void show_infos(GLFWwindow* window) {
    currentTime = glfwGetTime();
    deltaTime = currentTime - prevTime;
    counter++;
    // deltaTime >= 1.0 / 30.0 - to get a faster frame update
    if (deltaTime >= 1.0) {
        std::string FPS = std::to_string((1.0 / deltaTime) * counter);
        std::string ms = std::to_string((deltaTime / counter) * 1000);
        std::string FOV = std::to_string(camera.Zoom);
        std::string newTitle = "OpenGL(FT_VOX) - " + FPS + "FPS / " + ms + "ms --- FOV: " + FOV + "°";
        glfwSetWindowTitle(window, newTitle.c_str());
        prevTime = currentTime;
        counter = 0;
    }
}
