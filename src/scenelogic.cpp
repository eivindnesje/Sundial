#include <chrono>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <SFML/Audio/SoundBuffer.hpp>
#include <utilities/shader.hpp>
#include <glm/vec3.hpp>
#include <iostream>
#include <utilities/timeutils.h>
#include <utilities/mesh.h>
#include <utilities/shapes.h>
#include <utilities/glutils.h>
#include <SFML/Audio/Sound.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include "scenelogic.h"
#include "sceneGraph.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include "utilities/imageLoader.hpp"
#include "utilities/glfont.h"

enum KeyFrameAction
{
    BOTTOM,
    TOP
};

#include <timestamps.h>

double padPositionX = 0.5;
double padPositionZ = 0.5;

unsigned int currentKeyFrame = 0;
unsigned int previousKeyFrame = 0;

SceneNode *rootNode;
SceneNode *boxNode;
SceneNode *padNode;
SceneNode *lightNode;

static const int numLights = 3;
int lightIndex = 0;

LightSource lightSources[numLights];

float cameraYaw = 0.0f;    // Horizontal angle (longitude)
float cameraPitch = 0.0f;  // Vertical angle (latitude)
const float cameraRadius = 100.0f; // Distance from the center of the scene

// These are heap allocated, because they should not be initialised at the start of the program
Gloom::Shader *shader;

const glm::vec3 boxDimensions(180, 90, 90);
const glm::vec3 padDimensions(30, 3, 40);


CommandLineOptions options;

bool jumpedToNextFrame = false;
bool isPaused = false;

bool mouseLeftPressed = false;
bool mouseLeftReleased = false;
bool mouseRightPressed = false;
bool mouseRightReleased = false;

// Modify if you want the music to start further on in the track. Measured in seconds.
const float debug_startTime = 0;
double totalElapsedTime = debug_startTime;
double sceneElapsedTime = debug_startTime;

double mouseSensitivity = 0.2;
double lastMouseX = windowWidth / 2;
double lastMouseY = windowHeight / 2;
void mouseCallback(GLFWwindow *window, double x, double y)
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    // Compute mouse movement delta relative to the last stored position
    double deltaX = x - lastMouseX;
    double deltaY = y - lastMouseY;

    // Update camera angles using a sensitivity factor.
    // (Increase/decrease mouseSensitivity if needed.)
    cameraYaw   += mouseSensitivity * deltaX;
    cameraPitch += mouseSensitivity * deltaY;

    // Clamp cameraPitch to avoid flipping (e.g., restrict between -89 and +89 degrees)
    if(cameraPitch > 89.0f)  cameraPitch = 89.0f;
    if(cameraPitch < -89.0f) cameraPitch = -89.0f;

    // Reset mouse position to the center of the window and update last mouse values
    glfwSetCursorPos(window, windowWidth / 2, windowHeight / 2);
    lastMouseX = windowWidth / 2;
    lastMouseY = windowHeight / 2;
}

void initScene(GLFWwindow *window, CommandLineOptions sceneOptions)
{

    options = sceneOptions;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    shader->activate();

    // Create meshes
    Mesh pad = cube(padDimensions, glm::vec2(30, 40), true);
    Mesh box = cube(boxDimensions, glm::vec2(90), true, true);

    // Fill buffers
    unsigned int boxVAO = generateBuffer(box);
    unsigned int padVAO = generateBuffer(pad);

    // Construct scene
    rootNode = createSceneNode();
    boxNode = createSceneNode();
    padNode = createSceneNode();

    lightNode = createSceneNode();
    lightNode->nodeType = POINT_LIGHT;
    lightNode->position = glm::vec3(0.0f, 0.0f, -60.0f);
    lightNode->lightColor = glm::vec3(1.0, 1.0, 1.0);


    rootNode->children.push_back(boxNode);
    rootNode->children.push_back(padNode);
    rootNode->children.push_back(lightNode);


    boxNode->vertexArrayObjectID = boxVAO;
    boxNode->VAOIndexCount = box.indices.size();

    padNode->vertexArrayObjectID = padVAO;
    padNode->VAOIndexCount = pad.indices.size();

    getTimeDeltaSeconds();

    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;
}

void updateFrame(GLFWwindow *window)
{
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    double timeDelta = getTimeDeltaSeconds();


    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1))
    {
        mouseLeftPressed = true;
        mouseLeftReleased = false;
    }
    else
    {
        mouseLeftReleased = mouseLeftPressed;
        mouseLeftPressed = false;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2))
    {
        mouseRightPressed = true;
        mouseRightReleased = false;
    }
    else
    {
        mouseRightReleased = mouseRightPressed;
        mouseRightPressed = false;
    }

    totalElapsedTime += timeDelta;
    if (isPaused)
    {
        if (mouseRightReleased)
        {
            isPaused = false;
        }
    }
    else
    {
        sceneElapsedTime += timeDelta;
        if (mouseRightReleased)
        {
            isPaused = true;
        }
        // Get the timing for the beat of the song
        for (unsigned int i = currentKeyFrame; i < keyFrameTimeStamps.size(); i++)
        {
            if (sceneElapsedTime < keyFrameTimeStamps.at(i))
            {
                continue;
            }
            currentKeyFrame = i;
        }

        jumpedToNextFrame = currentKeyFrame != previousKeyFrame;
        previousKeyFrame = currentKeyFrame;

        double frameStart = keyFrameTimeStamps.at(currentKeyFrame);
        double frameEnd = keyFrameTimeStamps.at(currentKeyFrame + 1); // Assumes last keyframe at infinity

        double elapsedTimeInFrame = sceneElapsedTime - frameStart;
        double frameDuration = frameEnd - frameStart;
        double fractionFrameComplete = elapsedTimeInFrame / frameDuration;

        KeyFrameAction currentOrigin = keyFrameDirections.at(currentKeyFrame);
        KeyFrameAction currentDestination = keyFrameDirections.at(currentKeyFrame + 1);

    }

    // Define the center point of the scene around which the camera orbits.
    glm::vec3 center = glm::vec3(0, 0, 0);  // Adjust as needed

    // Compute camera position in spherical coordinates using cameraYaw and cameraPitch.
    // Note: glm::radians() converts degrees to radians.
    glm::vec3 cameraPos;
    cameraPos.x = center.x + cameraRadius * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
    cameraPos.y = center.y + cameraRadius * sin(glm::radians(cameraPitch));
    cameraPos.z = center.z + cameraRadius * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));

    // Build the view matrix using glm::lookAt.
    glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0, 1, 0));

    // Build the projection matrix as before.
    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(windowWidth) / float(windowHeight), 0.1f, 350.f);

    // Combine to form the view-projection matrix.
    glm::mat4 VP = projection * view;

    glm::mat4 identity = glm::mat4(1.0f);

    // Move and rotate various SceneNodes
    boxNode->position = {0, -10, -80};


    padNode->position = {
        boxNode->position.x - (boxDimensions.x / 2) + (padDimensions.x / 2) + (1 - 0.5f) * (boxDimensions.x - padDimensions.x),
        boxNode->position.y - (boxDimensions.y / 2) + (padDimensions.y / 2),
        boxNode->position.z - (boxDimensions.z / 2) + (padDimensions.z / 2) + (1 - 0.5f) * (boxDimensions.z - padDimensions.z)
    };


    lightIndex = 0;

    updateNodeTransformations(rootNode, identity, VP);

    for (int i = 0; i < numLights; i++) {
        GLint posLoc   = shader->getUniformFromName(fmt::format("lights[{}].position", i));
        GLint colorLoc = shader->getUniformFromName(fmt::format("lights[{}].color",    i));

        glm::vec3 p = lightSources[i].position;
        glm::vec3 c = lightSources[i].color;

        glUniform3f(posLoc, p.x, p.y, p.z);
        glUniform3f(colorLoc, c.x, c.y, c.z);
    }

    glUniform1i(glGetUniformLocation(shader->get(), "numLights"), numLights);
    glUniform3fv(glGetUniformLocation(shader->get(), "cameraPos"), 1, glm::value_ptr(cameraPos));


    float la = 0.001f;
    float lb = 0.001f;
    float lc = 0.001f;
    glUniform1f(glGetUniformLocation(shader->get(), "attConst"),  la);
    glUniform1f(glGetUniformLocation(shader->get(), "attLinear"), lb);
    glUniform1f(glGetUniformLocation(shader->get(), "attQuad"),   lc);

    
}

void updateNodeTransformations(SceneNode *node, glm::mat4 parentModel, glm::mat4 parentVP)
{
    glm::mat4 transformationMatrix =
        glm::translate(node->position) * glm::translate(node->referencePoint) * glm::rotate(node->rotation.y, glm::vec3(0, 1, 0)) * glm::rotate(node->rotation.x, glm::vec3(1, 0, 0)) * glm::rotate(node->rotation.z, glm::vec3(0, 0, 1)) * glm::scale(node->scale) * glm::translate(-node->referencePoint);

    node->modelMatrix = parentModel * transformationMatrix;
    node->MVP = parentVP * transformationMatrix;

    switch (node->nodeType)
    {
    case GEOMETRY:
        break;
    case POINT_LIGHT:
        if (lightIndex < numLights)
        {
            glm::vec4 pos = node->modelMatrix * glm::vec4(0, 0, 0, 1);
            lightSources[lightIndex].position = glm::vec3(pos);
            lightSources[lightIndex++].color = node->lightColor;
        }
        break;
    case SPOT_LIGHT:
        break;
    }

    for (SceneNode *child : node->children)
    {
        updateNodeTransformations(child, node->modelMatrix, node->MVP);
    }
}

void renderNode(SceneNode *node)
{

    switch (node->nodeType)
    {
    case GEOMETRY:
        if (node->vertexArrayObjectID != -1)
        {
            glBindVertexArray(node->vertexArrayObjectID);

            glUniformMatrix4fv(3, 1, GL_FALSE, glm::value_ptr(node->modelMatrix));
            glUniformMatrix4fv(4, 1, GL_FALSE, glm::value_ptr(node->MVP));

            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(node->modelMatrix)));
            glUniformMatrix3fv(5, 1, GL_FALSE, glm::value_ptr(normalMatrix));

            glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
        }
        break;
    case POINT_LIGHT:
        break;
    case SPOT_LIGHT:
        break;
    }

    for (SceneNode *child : node->children)
    {
        renderNode(child);
    }
}

void renderFrame(GLFWwindow *window)
{
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    renderNode(rootNode);
}
