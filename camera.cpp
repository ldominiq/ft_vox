#include "camera.h"

// constructor with vectors
Camera::Camera(c_math::vec3 position, c_math::vec3 up, float yaw, float pitch) : Position(position), Front(c_math::vec3(0.0f, 0.0f, -1.0f)), WorldUp(up), Yaw(yaw), Pitch(pitch), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
{
    updateCameraVectors();
}
// constructor with scalar values
Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(c_math::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
{
    Position = c_math::vec3(posX, posY, posZ);
    WorldUp = c_math::vec3(upX, upY, upZ);
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

// returns the view matrix calculated using Euler Angles and the LookAt Matrix
c_math::mat4 Camera::GetViewMatrix()
{
    return c_math::lookAt(Position, Position + Front, Up);
}

// processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
void Camera::ProcessKeyboard(Camera_Movement direction, double deltaTime)
{
    float velocity = MovementSpeed * deltaTime;
    if (direction == FORWARD)
        Position += Front * velocity;
    if (direction == BACKWARD)
        Position -= Front * velocity;
    if (direction == LEFT)
        Position -= Right * velocity;
    if (direction == RIGHT)
        Position += Right * velocity;

    //Position.y = 0.0f; // <-- this one-liner keeps the user at the ground level (xz plane) FPS
}

// processes input received from a mouse input system. Expects the offset value in both the x and y direction.
void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    // update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

// processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
void Camera::ProcessMouseScroll(float yoffset)
{
    Zoom -= (float)yoffset;
    if (Zoom < 20.0f)
        Zoom = 20.0f;
    if (Zoom > 90.0f)
        Zoom = 90.0f;
}

// calculates the front vector from the Camera's (updated) Euler Angles
void Camera::updateCameraVectors() {
    // calculate the new Front vector
    c_math::vec3 front;
    front.x = cos(c_math::radians(Yaw)) * cos(c_math::radians(Pitch));
    front.y = sin(c_math::radians(Pitch));
    front.z = sin(c_math::radians(Yaw)) * cos(c_math::radians(Pitch));
    Front = c_math::normalize(front);
    // also re-calculate the Right and Up vector
    Right = c_math::normalize(c_math::cross(Front, WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    Up    = c_math::normalize(c_math::cross(Right, Front));
}