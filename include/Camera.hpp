#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "World.hpp"

class Camera {

	GLuint wireframeVAO, wireframeVBO, wireframeEBO;

	void initWireframeCube();
	std::unique_ptr<Shader> blockWireframeShader = nullptr;

public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw, Pitch;
    float MovementSpeed;
    float MouseSensitivity;

    explicit Camera(glm::vec3 position);

    glm::mat4 getViewMatrix() const;
    void processKeyboard(int direction, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset);
    void updateCameraVectors();
	
	bool getTargetedBlock(std::unique_ptr<World> &world, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance = 100); //faceNormal is currently unused
	void setTargettedBlock(std::unique_ptr<World> &world);
	void removeTargettedBlock(std::unique_ptr<World> &world);
	void drawWireframeSelectedBlockFace(std::unique_ptr<World> &world, glm::mat4 &view, glm::mat4 &projection);
};


#endif // CAMERA_HPP