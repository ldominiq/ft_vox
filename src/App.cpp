//
// Created by lucas on 6/25/25.
//

#include "App.hpp"

GLFWwindow* window;

static unsigned int loadTexture(const char* path);

App::App(): VAO(0), VBO(0), EBO(0), shaderProgram(0), texture(0), camera(nullptr), monitor(nullptr), mode(nullptr),
            chunk(nullptr),
            world(nullptr),
            skybox(nullptr),
            textureShader(nullptr),
            gradientShader(nullptr),
            activeShader(nullptr) {
}

App::~App() { cleanup(); }

void App::init() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Monitor infos
    monitor = glfwGetPrimaryMonitor();
    mode = glfwGetVideoMode(monitor);

    window = glfwCreateWindow(windowedWidth, windowedHeight, "ft_vox", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, const int width, const int height) {
        (void)w;
        glViewport(0, 0, width, height);
    });

    glfwMakeContextCurrent(window);
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

    glfwGetFramebufferSize(window, &windowedWidth, &windowedHeight);

    const std::vector<std::string> faces = {
        "assets/skybox/right.bmp",  // +X
        "assets/skybox/left.bmp",   // -X
        "assets/skybox/top.bmp",    // +Y
        "assets/skybox/bottom.bmp", // -Y
        "assets/skybox/front.bmp",  // +Z
        "assets/skybox/back.bmp"    // -Z
    };

    skybox = new Skybox(faces);

    world = new World();
    
    glEnable(GL_DEPTH_TEST);
    
    // enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // Mouse movement event handling
    camera = new Camera(glm::vec3(-3.0f, 32.0f, 0.0f));
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, const double xpos, const double ypos) {
        static App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (app) {
            if (app->firstMouse) {
                app->lastX = xpos;
                app->lastY = ypos;
                app->firstMouse = false;
            }
            const float xoffset = xpos - app->lastX;
            const float yoffset = app->lastY - ypos; // Reversed: y-coordinates go from bottom to top
            
            app->lastX = xpos;
            app->lastY = ypos;
            
            app->camera->processMouseMovement(xoffset, yoffset);
        }
    });
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	loadControlsFromFile();
}

void App::loadResources() {
    // Load shaders and textures

    textureShader = new Shader("shaders/simple.vert", "shaders/simple.frag");
    gradientShader = new Shader("shaders/gradient.vert", "shaders/gradient.frag");
    texture = loadTexture("assets/textures/spritesheet3.png");

    activeShader = textureShader;

    activeShader->use();
    activeShader->setInt("texture1", 0);
}

void App::render() {

    while (!glfwWindowShouldClose(window)) {
        
        // Calculate delta time for frame rate
        const float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        updateWindowTitle();
        processInput();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        activeShader->use();

        // window aspect ratio
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        
        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(80.0f), aspect, 0.1f, renderDistance);
    

        // Set the uniform matrices in the shader
        activeShader->setMat4("view", view);
        activeShader->setMat4("projection", projection);
        
        world->updateVisibleChunks(camera->Position);
        world->render(activeShader);
        skybox->draw(camera->getViewMatrix(), projection);
		camera->drawWireframeSelectedBlockFace(world, view, projection);


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
	saveControls();
}

void App::loadControlsDefaults() {
	controlsArray[FORWARD]				= GLFW_KEY_W;
	controlsArray[BACKWARD]        		= GLFW_KEY_S;
	controlsArray[LEFT]					= GLFW_KEY_A;
	controlsArray[RIGHT]				= GLFW_KEY_D;
    controlsArray[UP]					= GLFW_KEY_E;
    controlsArray[DOWN]					= GLFW_KEY_Q;
    controlsArray[LEFT_CLICK]			= GLFW_MOUSE_BUTTON_LEFT;
    controlsArray[TOGGLE_FULLSCREEN]	= GLFW_KEY_F11;
    controlsArray[TOGGLE_WIREFRAME]		= GLFW_KEY_F1;
    controlsArray[TOGGLE_SHADER]		= GLFW_KEY_F2;
    controlsArray[MOVE_FAST]			= GLFW_KEY_LEFT_SHIFT;
    controlsArray[CLOSE_WINDOW]			= GLFW_KEY_ESCAPE;
}

void App::loadControlsFromFile(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        loadControlsDefaults();
        return;
    }

    // Initialize defaults first
    loadControlsDefaults();

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyName;
        int keyValue;
        if (!(iss >> keyName >> keyValue)) continue;

        for (int i = 0; i < CONTROL_COUNT; ++i) {
            if (keyName == controlNames[i]) {
                controlsArray[i] = keyValue;
                break;
            }
        }
    }
}

void App::saveControls(const char* filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return; // handle errors as you want

    for (int i = 0; i < CONTROL_COUNT; ++i) {
        file << controlNames[i] << " " << controlsArray[i] << "\n";
    }

	file << "\n\n# see 'https://www.glfw.org/docs/latest/group__keys.html' for key values" << '\n';
}

void App::processInput() {
    static bool f11Held = false;
    static bool f1Held = false;
    static bool f2Held = false;
	static bool leftMousePressedLastFrame = false;

	//reload chunk. F3 + A;
	if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS &&
    	glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		for (auto chunk : world->getRenderedChunks())
			chunk->updateChunk();
		return;
	}

    // Toggle Fullscreen
    if (glfwGetKey(window, controlsArray[TOGGLE_FULLSCREEN]) == GLFW_PRESS && !f11Held) {
        toggleDisplayMode();
        f11Held = true;
    }
    if (glfwGetKey(window, controlsArray[TOGGLE_FULLSCREEN]) == GLFW_RELEASE) {
        f11Held = false;
    }

    // Toggle Wireframe Mode
    if (glfwGetKey(window, controlsArray[TOGGLE_WIREFRAME]) == GLFW_PRESS && !f1Held) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        f1Held = true;
    }
    if (glfwGetKey(window, controlsArray[TOGGLE_WIREFRAME]) == GLFW_RELEASE) {
        f1Held = false;
    }

    // Toggle Shader
    if (glfwGetKey(window, controlsArray[TOGGLE_SHADER]) == GLFW_PRESS && !f2Held) {
        useGradientShader = !useGradientShader;
        activeShader = useGradientShader ? textureShader : gradientShader;
        f2Held = true;
    }
    if (glfwGetKey(window, controlsArray[TOGGLE_SHADER]) == GLFW_RELEASE) {
        f2Held = false;
    }

    // Movement
    if (glfwGetKey(window, controlsArray[FORWARD]) == GLFW_PRESS)
        camera->processKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, controlsArray[BACKWARD]) == GLFW_PRESS)
        camera->processKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, controlsArray[LEFT]) == GLFW_PRESS)
        camera->processKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, controlsArray[RIGHT]) == GLFW_PRESS)
        camera->processKeyboard(RIGHT, deltaTime);

    // Move up/down
    if (glfwGetKey(window, controlsArray[UP]) == GLFW_PRESS)
        camera->Position.y += 1.0f;
    if (glfwGetKey(window, controlsArray[DOWN]) == GLFW_PRESS)
        camera->Position.y -= 1.0f;

    // Move faster
    if (glfwGetKey(window, controlsArray[MOVE_FAST]) == GLFW_PRESS)
        camera->MovementSpeed = 50.0f;
    if (glfwGetKey(window, controlsArray[MOVE_FAST]) == GLFW_RELEASE)
        camera->MovementSpeed = 10.0f;

	int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if (state == GLFW_PRESS && !leftMousePressedLastFrame)
		camera->removeTargettedBlock(world);
	leftMousePressedLastFrame = (state == GLFW_PRESS);

    // Exit
    if (glfwGetKey(window, controlsArray[CLOSE_WINDOW]) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);


	bool cursorDisabled = true;
	bool kWasPressed = false;

	// In your main loop:
	// if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
	// 	if (!kWasPressed) {
	// 		cursorDisabled = !cursorDisabled;  // Toggle state
	// 		glfwSetInputMode(window, GLFW_CURSOR,
	// 			cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	// 		kWasPressed = true;
	// 	}
	// } else {
	// 	kWasPressed = false;  // Reset once key is released
	// }

}


void App::updateWindowTitle() {
    const float currentFrame = glfwGetTime();
    frameCount++;

    if (currentFrame - lastTitleUpdate >= 0.5f) {
        const float fps = frameCount / (currentFrame - lastTitleUpdate);
        const float msPerFrame = 1000.0f / fps;

        const std::string title = "OpenGL (ft_vox) - " +
            std::to_string(static_cast<int>(fps)) + " FPS / " +
            std::to_string(msPerFrame).substr(0, 5) + " ms";

        glfwSetWindowTitle(window, title.c_str());

        lastTitleUpdate = currentFrame;
        frameCount = 0;
    }

}

void App::toggleDisplayMode() {
    if (displayMode == DisplayMode::Windowed) {
        // Save windowed size and position
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        // Go to fullscreen (exclusive)
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        displayMode = DisplayMode::Fullscreen;

    } else {
        // Back to windowed
        glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
        glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
        displayMode = DisplayMode::Windowed;
    }
}

static unsigned int loadTexture(const char* path) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (data) {
        const GLenum format = ch == 4 ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture\n";
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    stbi_image_free(data);
    
    return texID;
}
