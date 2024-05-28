#include <iostream>
#include "Libraries/include/glad/glad.h"
#include "Libraries/include/GLFW/glfw3.h"

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
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

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
        std::string extension = file.substr(file.find_last_of(".") + 1);
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
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL(SCOP)", nullptr, nullptr);
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

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

	// Build and compile our shader program
	// ------------------------------------
    std::cout << "Loading shaders..." << std::endl;
	Shader ourShader("../../Shaders/3.3.shader.vs", "../../Shaders/3.3.shader.fs");
    Shader lightShader("../../Shaders/light.vert", "../../Shaders/light.frag");

    Model ourModel("Models/42.obj");

    Model lightModel("Models/teapot2.obj");

    if (!ourModel.loaded)
    {
        glfwTerminate();
        return -1;
    }
//    Model reference = ourModel;

    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
    model = glm::scale(model, glm::vec3(0.5f, .5f, .5f));	// it's a bit too big for our scene, so scale it down

    light = glm::translate(light, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
    light = glm::scale(light, glm::vec3(0.5f, .5f, .5f));	// it's a bit too big for our scene, so scale it down


    // removes VSync (breaks model movement because deltaTime is different)
    // glfwSwapInterval(0);

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

        ourShader.setFloat("mixValue", mixValue);
        ourShader.use();
        ourShader.setVec3("objectColor", 1.0f, 0.5f, 0.31f);
        ourShader.setVec3("lightColor",  1.0f, 1.0f, 1.0f);

        if (!textured && mixValue < 1.)
            mixValue = mixValue + step > 1.0 ? 1.0 : mixValue + step;
        else if (textured && mixValue > 0.)
            mixValue = mixValue - step < 0.0 ? 0.0 : mixValue - step;

        // pass projection matrix to shader (as projection matrix rarely changes there's no need to do this per frame)
        glm::mat4 projection    = glm::mat4(1.0f);
        projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        ourShader.setMat4("projection", projection);

        // camera/view transformation
        glm::mat4 view = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first

        view = camera.GetViewMatrix();
        ourShader.setMat4("view", view);

        if (autorotate)
            model = glm::rotate(model, glm::radians(0.1f), glm::vec3(0.0f, 1.0f, 0.0f));

        // render the loaded model
        ourShader.setMat4("model", model);
        ourModel.Draw(ourShader);

        // render the reference model
//        glm::mat4 model2 = glm::mat4(1.0f);
//        model2 = glm::translate(model2, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
//        model2 = glm::scale(model2, glm::vec3(0.5f, .5f, .5f));	// it's a bit too big for our scene, so scale it down
//        ourShader.setMat4("model", model2);
        //reference.Draw(ourShader);

        lightShader.use();
        lightShader.setMat4("projection", projection);
        lightShader.setMat4("view", view);
        light = glm::mat4(1.0f);
        light = glm::translate(light, lightPos);
        light = glm::scale(light, glm::vec3(0.2f)); // a smaller cube
        lightShader.setMat4("model", light);
        lightModel.Draw(lightShader);

        // render the light cube

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
}

void processInput(GLFWwindow *window)
{
    // Handle window close
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Handle wireframe, points, and fill modes
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        glPointSize(2.0f);
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
	(void) window;
    //    1. Calculate the mouse's offset since the last frame.
    //    2. Add the offset values to the camera's yaw and pitch values.
    //    3. Add some constraints to the minimum/maximum pitch values.
    //    4. Calculate the direction vector.

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
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
        std::string newTitle = "OpenGL(SCOP) - " + FPS + "FPS / " + ms + "ms --- FOV: " + FOV + "°";
        glfwSetWindowTitle(window, newTitle.c_str());
        prevTime = currentTime;
        counter = 0;
    }
}
