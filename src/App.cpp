//
// Created by lucas on 6/25/25.
//

#include "App.hpp"

// ImGui includes
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <unistd.h> // for sysconf
#include <stdio.h>  // for FILE, fopen
#include <cstdlib>

GLFWwindow* window;

static unsigned int loadTexture(const char* path);

App::App(): VAO(0),
			VBO(0),
			EBO(0),

			shaderProgram(0),
			texture(0),

			camera(nullptr),
			monitor(nullptr),
			mode(nullptr),

            world(nullptr),
            skybox(nullptr),
            textureShader(nullptr),
            gradientShader(nullptr),
            activeShader(nullptr) {
    // Pre-allocate the FPS sample buffer to avoid reallocations at runtime
    fpsSamples.reserve(fpsSampleCount);
}

App::App(int seed): VAO(0),
			VBO(0),
			EBO(0),

			shaderProgram(0),
			texture(0),

			camera(nullptr),
			monitor(nullptr),
			mode(nullptr),

            world(nullptr),
            skybox(nullptr),
            textureShader(nullptr),
            gradientShader(nullptr),
            activeShader(nullptr),
			
			seed(seed) {
    // Pre-allocate the FPS sample buffer to avoid reallocations at runtime
    fpsSamples.reserve(fpsSampleCount);
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
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

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

	skybox = std::make_unique<Skybox>(faces);

	if (seed.has_value()) world = std::make_unique<World>(seed.value());
    else world = std::make_unique<World>();

    // Headless dump helper: if environment variable FTVOX_DUMP_BIOMES is set
    // call World::dumpBiomeMap and exit. This lets us quickly regenerate the
    // biome map after changes without interacting with the UI.
    if (std::getenv("FTVOX_DUMP_BIOMES")) {
        TerrainGenerationParams &p = world->getTerrainParams();
        int size = p.genSize;
        int down = p.downsample;
        std::string out = "build/biome_map.ppm";
        std::cout << "FTVOX_DUMP_BIOMES set — generating biome map: " << out << std::endl;
        world->dumpBiomeMap(0, 0, size, size, down, out);
        std::exit(0);
    }
    
    glEnable(GL_DEPTH_TEST);
    
    // enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // Mouse movement event handling
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 128.0f, 0.0f));
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, const double xpos, const double ypos) {
        static App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (!app) return;
        // Honour ImGui’s mouse capture: if the UI is being interacted with
        // (e.g. hovering/clicking in a window), do not rotate the camera.
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || app->uiInteractive) {
            return;
        }
        if (app->firstMouse) {
            app->lastX = xpos;
            app->lastY = ypos;
            app->firstMouse = false;
        }
        const float xoffset = static_cast<float>(xpos - app->lastX);
        const float yoffset = static_cast<float>(app->lastY - ypos); // Reversed: y-coordinates go from bottom to top
        app->lastX = xpos;
        app->lastY = ypos;
        app->camera->processMouseMovement(xoffset, yoffset);
    });
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // -------------------------------------------------------------------------
    // ImGui initialization
    // Create ImGui context and set up GLFW/OpenGL bindings.  We specify the
    // OpenGL version used by the backend (matching the context created above).
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Set a dark theme
    ImGui::StyleColorsDark();
    // Initialize ImGui for GLFW and OpenGL.  Pass the window pointer and GLSL
    // version string.  The GLSL version must match your context version.
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

	loadControlsFromFile();
}

void App::loadResources() {
    // Load shaders and textures

    textureShader = std::make_shared<Shader>("shaders/simple.vert", "shaders/simple.frag");
    gradientShader = std::make_shared<Shader>("shaders/gradient.vert", "shaders/gradient.frag");
    texture = loadTexture("assets/textures/textures.png");

    activeShader = textureShader;

    activeShader->use();
    activeShader->setInt("atlas", 0);
}

void App::render() {

    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time for frame rate
        const float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Maintain a moving average of the last N frame times for a stable
        // FPS display.  Push the current frame time and pop the oldest if
        // the buffer is full.  Then compute the average delta and convert
        // to FPS.  If the buffer is empty (first frame) we simply use the
        // current frame rate.
        fpsSamples.push_back(deltaTime);
        if (fpsSamples.size() > fpsSampleCount) {
            fpsSamples.erase(fpsSamples.begin());
        }
        float avgDelta = 0.0f;
        for (float dt : fpsSamples) {
            avgDelta += dt;
        }
        if (!fpsSamples.empty()) {
            avgDelta /= static_cast<float>(fpsSamples.size());
            uiDisplayFPS = avgDelta > 0.0f ? 1.0f / avgDelta : 0.0f;
        } else {
            uiDisplayFPS = 0.0f;
        }

        // Start a new ImGui frame.  We do this **before** handling input so that
        // ImGui’s internal state (WantCaptureMouse/WantCaptureKeyboard) is up
        // to date for this frame.  Even if the UI is currently hidden we
        // create a new frame to ensure ImGui updates its internal state and
        // resets input capture flags.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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

        // Update lighting uniforms from the adjustable state.  Normalize the
        // direction so that it remains a unit vector after editing.
        glm::vec3 dirNorm = glm::normalize(lightDir);
        activeShader->setVec3("lightDir", dirNorm);
        activeShader->setVec3("lightColor", lightColor);
        activeShader->setVec3("ambientColor", ambientColor);

        world->updateVisibleChunks(camera->Position, camera->Front);
        world->render(activeShader);
        skybox->draw(camera->getViewMatrix(), projection);
        camera->drawWireframeSelectedBlockFace(world, view, projection);

        if (showDebugWindow) {
            //ImGui::ShowDemoWindow();
            debugWindow();
        }

        // Finalize the ImGui frame and draw it.  Even if the overlay is
        // non-interactive the draw data will be present, so draw it always.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers and poll events (keys pressed, mouse movement, etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void App::debugWindow() {
        // Build the ImGui UI.  We always draw the debug overlay.  When
        // uiInteractive is false we disable input on the window, allowing
        // the player to interact with the game while the overlay remains
        // visible.  When uiInteractive is true the window captures input and
        // the mouse is released.
        {
            auto& params = world->getTerrainParams();

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
            if (!uiInteractive) {
                flags |= ImGuiWindowFlags_NoInputs;
                // Make the overlay slightly transparent when not interactive
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
            }
            ImGui::Begin("Debug Window", nullptr, flags);
            // Display smoothed FPS and frame time
            ImGui::Text("FPS: %.1f (%.3f ms)", uiDisplayFPS, uiDisplayFPS > 0.0f ? 1000.0f / uiDisplayFPS : 0.0f);
            // Display camera coordinates
            ImGui::Text("Camera Position: x=%.2f y=%.2f z=%.2f", camera->Position.x, camera->Position.y, camera->Position.z);

            ImGui::Text("World SEED: %i", params.seed);

            // Additional metrics: number of loaded chunks and approximate memory usage
            if (world) {
                const size_t visibleChunks = world->getRenderedChunkCount();
                const size_t totalChunks   = world->getTotalChunkCount();
                ImGui::Text("Chunks: %zu visible / %zu total", visibleChunks, totalChunks);
            }
            // Display memory usage in megabytes.  We call a static helper to
            // obtain the current resident set size (RSS).
            {
                const size_t memBytes = getCurrentRSS();
                const double memMB = memBytes / (1024.0 * 1024.0);
                ImGui::Text("Memory: %.2f MB", memMB);
            }

            ImGui::Separator();

            if (ImGui::CollapsingHeader("Teleportation")) {
                // Teleport player
                ImGui::Text("Teleport Player");
                static float tmpX = 0;
                static float tmpY = 100;
                static float tmpZ = 0;
                ImGui::InputFloat("X", &tmpX);
                ImGui::InputFloat("Y", &tmpY);
                ImGui::InputFloat("Z", &tmpZ);
                if (ImGui::Button("Teleport")) {
                    camera->Position = glm::vec3(tmpX, tmpY, tmpZ);
                }
            }

            ImGui::Separator();

            if (ImGui::CollapsingHeader("World Generation")) {
                // Terrain params
                ImGui::Text("Terrain Parameters");
                ImGui::SliderFloat("mountainBoost", &params.mountainBoost, 0.0f, 3.0f);
                ImGui::SliderFloat("PVBoost", &params.PVBoost, 0.0f, 3.0f);
                ImGui::SliderInt("minCliffElevation", &params.minCliffElevation, 0, 100);
                ImGui::SliderFloat("smoothingStrength", &params.smoothingStrength, 0.0f, 1.0f);
                ImGui::SliderFloat("cliffSlopeThreshold", &params.cliffSlopeThreshold, 0.0f, 2.0f);
                
                ImGui::SliderFloat("riverThreshold", &params.riverThreshold, 0.001f, 0.01f);
                ImGui::SliderFloat("riverStrength", &params.riverStrength, 0.0f, 1.0f);

                /* shoreSmoothRadius (default 8) controls horizontal extent of smoothing; increase for bigger beaches.
                    shoreSlopeFactor controls how many vertical blocks per column distance the ramp gains (2.0 is gentle).
                    shoreSmoothStrength (0..1) controls how strongly heights are pulled toward the ramp.*/
                ImGui::SliderInt("shoreSmoothRadius", &params.shoreSmoothRadius, 0, 16);
                ImGui::SliderFloat("shoreSlopeFactor", &params.shoreSlopeFactor, 0.0f, 4.0f);
                ImGui::SliderFloat("shoreSmoothStrength", &params.shoreSmoothStrength, 0.0f, 1.0f);
            }
            

            ImGui::Separator();

            if (ImGui::CollapsingHeader("Heightmap")) {
                // Create heightmap image
                ImGui::Text("Heightmap Generation");
                ImGui::InputInt("Size (ex. 100)", &params.genSize);
                ImGui::InputInt("Downsample (ex. 8)", &params.downsample);
                if (ImGui::Button("Generate Heightmap")) {
                    if (world) {
                        world->dumpHeightmap(0, 0, params.genSize, params.genSize, params.downsample, "heightmap.ppm");
                    }
                }
                if (ImGui::Button("Generate Biome Map")) {
                    if (world) {
                        world->dumpBiomeMap(0, 0, params.genSize, params.genSize, params.downsample, "biome_map.ppm");
                    }
                }
            }

            ImGui::Separator();

            // Wireframe toggle
            if (ImGui::Checkbox("Wireframe", &wireframe)) {
                glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            }
            // Shader toggle (texture vs gradient).  We update activeShader accordingly.
            if (ImGui::Checkbox("Use Gradient Shader", &useGradientShader)) {
                activeShader = useGradientShader ? gradientShader : textureShader;
            }
            // Changing this will update the far clipping plane.
            ImGui::SliderFloat("Clipping plane Distance", &renderDistance, 100.0f, 2000.0f);
            // Adjust the chunk loading radius.  Casting to int and back avoids
            // accidental type issues in the setter.  We clamp the range to a
            // reasonable minimum and maximum.
            if (world) {
                int radius = static_cast<int>(world->getLoadRadius());
                if (ImGui::SliderInt("Chunk Load Radius", &radius, 4, 32)) {
                    world->setLoadRadius(radius);
                }
            }

            // Adjust the maximum number of chunks being generated at the same time.
            // Lower values produce smoother frame rates but slower world loading.
            if (world) {
                int maxGen = static_cast<int>(world->getMaxConcurrentGeneration());
                if (ImGui::SliderInt("Generation Concurrency", &maxGen, 1, 8)) {
                    world->setMaxConcurrentGeneration(static_cast<std::size_t>(maxGen));
                }
            }

            // Lighting controls: direction and colours.  The direction vector
            // components are clamped to [-1,1]; colours use a colour picker.
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Lighting")) {
                ImGui::Text("Lighting Controls");
                ImGui::SliderFloat3("Light Direction", &lightDir.x, -1.0f, 1.0f);
                ImGui::ColorEdit3("Light Colour", &lightColor.x);
                ImGui::ColorEdit3("Ambient Colour", &ambientColor.x);
            }


            ImGui::End();
            if (!uiInteractive) {
                ImGui::PopStyleVar();
            }
        }
}

void App::run() {
    init();
    loadResources();
    render();
}

void App::cleanup() {

	saveWorldOnExit();

    // Shutdown ImGui before terminating GLFW
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

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
    controlsArray[TOGGLE_DEBUG]			= GLFW_KEY_TAB;
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
    static bool f1Held  = false;
    static bool f2Held  = false;
    static bool f4Held  = false;
    static bool tabHeld = false;
    static bool leftMousePressedLastFrame = false;
	static bool rightMousePressedLastFrame = false;

	//reload chunk. F3 + A;
	if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS &&
    	glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		for (auto &chunkPtr : world->getRenderedChunks())
		{
			if (auto chunk = chunkPtr.lock())
				chunk->buildMesh();
		}
		return;
	}

    // Show/Hide debug window
    if (glfwGetKey(window, controlsArray[TOGGLE_DEBUG]) == GLFW_PRESS && !tabHeld) {
        showDebugWindow = !showDebugWindow;
        tabHeld = true;

        if (!showDebugWindow && uiInteractive) {
            uiInteractive = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    if (glfwGetKey(window, controlsArray[TOGGLE_DEBUG]) == GLFW_RELEASE) {
        tabHeld = false;
    }

    // Toggle interactive mode with F4.  We debounce the key to avoid
    // multiple toggles per press.  When uiInteractive is true we release
    // the mouse and ImGui windows will capture input.  When false we
    // recapture the mouse and treat the debug window as an overlay only.
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS && !f4Held) {
        uiInteractive = !uiInteractive;
        f4Held = true;
        if (uiInteractive) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_RELEASE) {
        f4Held = false;
    }

    // Start by getting the ImGui IO structure.  We will respect its capture flags
    // when deciding whether to process game inputs.  Note: this call is valid
    // even if ImGui hasn’t been initialised in the current frame yet.
    ImGuiIO& io = ImGui::GetIO();

    // Determine whether game input should be suppressed.  When uiInteractive
    // is false the overlay is visible but non-interactive, so we never
    // suppress input.  When uiInteractive is true we honour ImGui’s capture
    // flags to decide whether to ignore keyboard or mouse events.
    const bool capturingKeyboard = uiInteractive && io.WantCaptureKeyboard;
    const bool capturingMouse    = uiInteractive && io.WantCaptureMouse;

    // Handle keyboard-based game actions when input isn’t captured.
    if (!capturingKeyboard) {
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

        // Toggle Shader (switch between texture and gradient shader).  When
        // useGradientShader is true we use gradientShader; otherwise we use
        // textureShader.
        if (glfwGetKey(window, controlsArray[TOGGLE_SHADER]) == GLFW_PRESS && !f2Held) {
            useGradientShader = !useGradientShader;
            activeShader = useGradientShader ? gradientShader : textureShader;
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
            camera->MovementSpeed = 100.0f;
        if (glfwGetKey(window, controlsArray[MOVE_FAST]) == GLFW_RELEASE)
            camera->MovementSpeed = 5.0f;
    }

    // Left click: remove targeted block.  Only handle this if the UI isn’t capturing the mouse.
	if (!capturingMouse) {
		// Left click: remove targeted block
		int leftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
		if (leftState == GLFW_PRESS && !leftMousePressedLastFrame)
			camera->removeTargettedBlock(world);
		leftMousePressedLastFrame = (leftState == GLFW_PRESS);

		// Right click: place targeted block
		int rightState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
		if (rightState == GLFW_PRESS && !rightMousePressedLastFrame)
			camera->setTargettedBlock(world);
		rightMousePressedLastFrame = (rightState == GLFW_PRESS);
	}

    // Exit (ESC).  Allow closing window even when ImGui doesn’t want keyboard.
    if (glfwGetKey(window, controlsArray[CLOSE_WINDOW]) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    stbi_image_free(data);
    
    return texID;
}

// static
// Returns the current process’s resident set size (RSS) in bytes.  On
// Linux this reads /proc/self/statm.  On other platforms this will
// return 0.  RSS is an approximation of the memory the process is
// currently using in RAM.
size_t App::getCurrentRSS() {
#ifdef __linux__
    long rss = 0L;
    long pageSize = 0L;
    FILE* fp = nullptr;
    fp = fopen("/proc/self/statm", "r");
    if (fp != nullptr) {
        /* Each entry in statm is a number of pages.  The second entry is
           the resident set size. */
        unsigned long dummy;
        if (fscanf(fp, "%lu %lu", &dummy, &rss) != 2) {
            rss = 0L;
        }
        fclose(fp);
    }
    pageSize = sysconf(_SC_PAGESIZE);
    return (size_t)rss * (size_t)pageSize;
#else
    return 0;
#endif
}

void App::saveWorldOnExit()
{
	//world->saveRegionsOnExit();
}