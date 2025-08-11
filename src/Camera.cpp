#include "Camera.hpp"
#include "App.hpp"

Camera::Camera(glm::vec3 position)
    : Position(position), WorldUp(0.0f, 1.0f, 0.0f),
      Yaw(45.0f), Pitch(0.0f), MovementSpeed(2.0f), MouseSensitivity(0.1f) {
    Front = glm::vec3(0.0f, 0.0f, -1.0f);
    updateCameraVectors();
	initWireframeCube();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::processKeyboard(const int direction, const float deltaTime) {
    const float velocity = MovementSpeed * deltaTime;

    // Minecraft'ish camera. Doens't move along the Y axis
    glm::vec3 horizontalFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));

    if (direction == FORWARD)
        Position += horizontalFront * velocity;
    if (direction == BACKWARD)
        Position -= horizontalFront * velocity;
    if (direction == LEFT)
        Position -= glm::normalize(glm::cross(horizontalFront, WorldUp)) * velocity;
    if (direction == RIGHT)
        Position += glm::normalize(glm::cross(horizontalFront, WorldUp)) * velocity;
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    if (Pitch > 89.0f)  Pitch = 89.0f;
    if (Pitch < -89.0f) Pitch = -89.0f;

    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}

bool Camera::getTargetedBlock(std::unique_ptr<World> &world, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance) {
    glm::vec3 rayOrigin = Position;
    glm::vec3 rayDir = glm::normalize(Front);

    glm::ivec3 blockPos = glm::floor(rayOrigin);

    glm::vec3 deltaDist = glm::abs(glm::vec3(1.0f) / rayDir);
    glm::ivec3 step;
    glm::vec3 sideDist;

    for (int i = 0; i < 3; ++i) {
        if (rayDir[i] < 0) {
            step[i] = -1;
            sideDist[i] = (rayOrigin[i] - blockPos[i]) * deltaDist[i];
        } else {
            step[i] = 1;
            sideDist[i] = (blockPos[i] + 1.0f - rayOrigin[i]) * deltaDist[i];
        }
    }

    float distanceTraveled = 0.0f;
    glm::ivec3 prevBlock = blockPos;

    while (distanceTraveled < maxDistance) {
        int axis;
        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) axis = 0;
            else                         axis = 2;
        } else {
            if (sideDist.y < sideDist.z) axis = 1;
            else                         axis = 2;
        }

        blockPos[axis] += step[axis];
        sideDist[axis] += deltaDist[axis];

        // Track face direction
        faceNormal = glm::ivec3(0);
        faceNormal[axis] = -step[axis];

		distanceTraveled = glm::min(glm::min(sideDist.x, sideDist.y), sideDist.z);

        // Check if this block exists in your world
        if (world && world->isBlockVisibleWorld(blockPos)) {
            hitBlock = blockPos;
            return true;
        }
    }

    return false;
}

void Camera::removeTargettedBlock(std::unique_ptr<World> &world)
{
	glm::ivec3 blockPos, faceNormal;
	if (getTargetedBlock(world, blockPos, faceNormal))
		world->setBlockWorld(blockPos, std::nullopt, BlockType::AIR);
}

void Camera::setTargettedBlock(std::unique_ptr<World> &world)
{
	glm::ivec3 blockPos, faceNormal;
	if (getTargetedBlock(world, blockPos, faceNormal))
		world->setBlockWorld(blockPos, faceNormal, BlockType::DIRT);
}

void Camera::initWireframeCube() {

    glm::vec3 vertices[8] = {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 1, 1},
        {0, 1, 1},
    };

    GLuint indices[] = {
        0,1, 1,2, 2,3, 3,0, // bottom face
        4,5, 5,6, 6,7, 7,4, // top face
        0,4, 1,5, 2,6, 3,7  // vertical lines
    };

    glGenVertexArrays(1, &wireframeVAO);
    glGenBuffers(1, &wireframeVBO);
    glGenBuffers(1, &wireframeEBO);

    glBindVertexArray(wireframeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, wireframeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireframeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Vertex layout: 3 floats per vertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0); // Unbind VAO

	blockWireframeShader = std::make_unique<Shader>("shaders/simpleWireframe.vert", "shaders/simpleWireframe.frag");
}

void Camera::drawWireframeSelectedBlockFace(std::unique_ptr<World> &world, glm::mat4 &view, glm::mat4 &projection) {

	glm::ivec3 blockPos, faceNormal;
	if (!getTargetedBlock(world, blockPos, faceNormal))
		return ;

	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(blockPos));

	blockWireframeShader->use();
	blockWireframeShader->setMat4("model", model);
	blockWireframeShader->setMat4("view", view);
	blockWireframeShader->setMat4("projection", projection);

    glBindVertexArray(wireframeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}
