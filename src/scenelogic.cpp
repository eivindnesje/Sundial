#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <iostream>
#include <fmt/format.h>
#include "scenelogic.h"
#include "sceneGraph.hpp"
#include "utilities/timeutils.h"
#include "utilities/shader.hpp"
#include "utilities/modelLoader.hpp"
#include "utilities/textureLoader.hpp"
#include "utilities/shapes.h"
#include "utilities/glutils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Global scene pointers
SceneNode *rootNode = nullptr;
SceneNode *lightNode = nullptr; // Directional light (sun)
SceneNode *sunNode = nullptr;   // Visible sun
static const int numLights = 1;
int lightIndex = 0;
LightSource lightSources[numLights];

// Camera parameters
float cameraYaw = 0.0f;
float cameraPitch = 45.0f;
const float cameraRadius = 200.0f;

// Shadow mapping globals
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
static unsigned int shadowFBO = 0;
static unsigned int shadowMap = 0;

// Shaders
static Gloom::Shader *shader = nullptr;
static Gloom::Shader *shadowShader = nullptr;

CommandLineOptions options;

// Timing variables
static double totalElapsedTime = 0.0;
static double sceneElapsedTime = 0.0;

// Sun movement constants
static const float SIM_SECONDS_PER_REAL_HOUR = 5.0f; 
static const float FULL_DAY = 24.0f * SIM_SECONDS_PER_REAL_HOUR;


// Mouse control (initial center assumed; adjust as needed)
static double mouseSensitivity = 0.2;
static double lastMouseX = 800.0;
static double lastMouseY = 600.0;

// --- Mouse Callback ---
static void mouseCallback(GLFWwindow* window, double x, double y) {
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    double deltaX = x - lastMouseX;
    double deltaY = y - lastMouseY;
    cameraYaw   += mouseSensitivity * deltaX;
    cameraPitch += mouseSensitivity * deltaY;
    if(cameraPitch > 89.0f)  cameraPitch = 89.0f;
    if(cameraPitch < -89.0f) cameraPitch = -89.0f;
    glfwSetCursorPos(window, winWidth/2, winHeight/2);
    lastMouseX = winWidth/2;
    lastMouseY = winHeight/2;
}

// --- Shadow Map Initialization ---
static void initShadowMap() {
    glGenTextures(1, &shadowMap);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f,1.0f,1.0f,1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Shadow framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// --- updateNodeTransformations ---
// This function propagates transformation updates through the scene graph.
void updateNodeTransformations(SceneNode *node, glm::mat4 parentModel, glm::mat4 parentVP) {
    glm::mat4 transformationMatrix =
        glm::translate(glm::mat4(1.0f), node->position) *
        glm::translate(glm::mat4(1.0f), node->referencePoint) *
        glm::rotate(glm::mat4(1.0f), node->rotation.y, glm::vec3(0,1,0)) *
        glm::rotate(glm::mat4(1.0f), node->rotation.x, glm::vec3(1,0,0)) *
        glm::rotate(glm::mat4(1.0f), node->rotation.z, glm::vec3(0,0,1)) *
        glm::scale(glm::mat4(1.0f), node->scale) *
        glm::translate(glm::mat4(1.0f), -node->referencePoint);
    node->modelMatrix = parentModel * transformationMatrix;
    node->MVP = parentVP * transformationMatrix;
    switch(node->nodeType) {
        case GEOMETRY: break;
        case POINT_LIGHT:
            if(lightIndex < numLights) {
                glm::vec4 pos = node->modelMatrix * glm::vec4(0,0,0,1);
                lightSources[lightIndex].position = glm::vec3(pos);
                lightSources[lightIndex++].color = node->lightColor;
            }
            break;
        case SPOT_LIGHT: break;
    }
    for(auto child : node->children)
        updateNodeTransformations(child, node->modelMatrix, node->MVP);
}

// --- initScene ---
void initScene(GLFWwindow *window, CommandLineOptions sceneOptions) {
    options = sceneOptions;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/simple.vert", "../res/shaders/simple.frag");
    shader->activate();

    shadowShader = new Gloom::Shader();
    shadowShader->makeBasicShader("../res/shaders/shadow.vert", "../res/shaders/shadow.frag");

    initShadowMap();

    // Create scene graph root.
    rootNode = createSceneNode();

    // Create directional light node (the sun used for lighting).
    lightNode = createSceneNode();
    lightNode->nodeType = POINT_LIGHT;
    lightNode->position = glm::vec3(0.0f, 100.0f, 50.0f); // Initial value; updated in updateFrame.
    lightNode->lightColor = glm::vec3(1.0f);
    rootNode->children.push_back(lightNode);

    // Create visible sun node.
    sunNode = createSceneNode();
    Mesh sunMesh = generateSphere(1.0f, 20, 20);
    unsigned int sunVAO = generateBuffer(sunMesh);
    sunNode->vertexArrayObjectID = sunVAO;
    sunNode->VAOIndexCount = sunMesh.indices.size();
    sunNode->scale = glm::vec3(10.0f);
    rootNode->children.push_back(sunNode);

    // Load sundial model.
    std::string diffuseTexName;
    Mesh sundialMesh = loadOBJModel("../res/models/sundial.obj", "../res/models/", diffuseTexName);
    unsigned int sundialVAO = generateBuffer(sundialMesh);
    SceneNode *sundialNode = createSceneNode();
    sundialNode->vertexArrayObjectID = sundialVAO;
    sundialNode->VAOIndexCount = sundialMesh.indices.size();
    sundialNode->position = glm::vec3(0.0f);
    sundialNode->scale = glm::vec3(0.5f);
    sundialNode->rotation.x = glm::radians(-90.0f);
    if(!diffuseTexName.empty()){
        std::string texturePath = "../res/models/" + diffuseTexName;
        unsigned int tex = loadTexture(texturePath);
        sundialNode->textureID = tex;
        sundialNode->hasTexture = true;
    }
    rootNode->children.push_back(sundialNode);

    totalElapsedTime = sceneElapsedTime = getTimeDeltaSeconds();
    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;
}

// --- renderShadowScene ---
// Traverse the scene graph to render the depth map.
// Skip rendering the visible sun.
static void renderShadowScene(SceneNode *node, glm::mat4 parentModel) {
    if(node == sunNode)
        return;
    glm::mat4 model = parentModel *
        glm::translate(glm::mat4(1.0f), node->position) *
        glm::translate(glm::mat4(1.0f), node->referencePoint) *
        glm::rotate(glm::mat4(1.0f), node->rotation.y, glm::vec3(0,1,0)) *
        glm::rotate(glm::mat4(1.0f), node->rotation.x, glm::vec3(1,0,0)) *
        glm::rotate(glm::mat4(1.0f), node->rotation.z, glm::vec3(0,0,1)) *
        glm::scale(glm::mat4(1.0f), node->scale) *
        glm::translate(glm::mat4(1.0f), -node->referencePoint);
    glUniformMatrix4fv(glGetUniformLocation(shadowShader->get(), "modelMatrix"), 1, GL_FALSE, glm::value_ptr(model));
    if(node->nodeType == GEOMETRY && node->vertexArrayObjectID != -1) {
        glBindVertexArray(node->vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    for(auto child : node->children)
        renderShadowScene(child, model);
}

// --- updateFrame ---
void updateFrame(GLFWwindow *window) {
    double timeDelta = getTimeDeltaSeconds();
    totalElapsedTime += timeDelta;
    sceneElapsedTime += timeDelta;

    float angularSpeed = 2.0f * glm::pi<float>() / FULL_DAY;
    float angle = angularSpeed * sceneElapsedTime;

    // Sun moves on a circular path in the xy plane with a constant z offset
    float orbitRadius = 150.0f;
    float zOffset = 50.0f; // Constant positive z direction
    glm::vec3 sunPos(orbitRadius * cos(angle), orbitRadius * sin(angle), zOffset);
    lightNode->position = sunPos;

    // The sun faces the origin, so the direction is from sunPos to (0,0,0)
    glm::vec3 sunDir = glm::normalize(sunPos);

    
    glUniform3f(glGetUniformLocation(shader->get(), "sun.direction"), sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(glGetUniformLocation(shader->get(), "sun.color"),
                lightNode->lightColor.x, lightNode->lightColor.y, lightNode->lightColor.z);
    sunNode->position = lightNode->position;

    // Update camera.
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glm::vec3 center(0.0f);
    glm::vec3 cameraPos;
    cameraPos.x = center.x + cameraRadius * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
    cameraPos.y = center.y + cameraRadius * sin(glm::radians(cameraPitch));
    cameraPos.z = center.z + cameraRadius * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));
    glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0,1,0));
    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(winWidth)/float(winHeight), 0.1f, 350.f);
    glm::mat4 VP = projection * view;
    glm::mat4 identity = glm::mat4(1.0f);
    lightIndex = 0;
    updateNodeTransformations(rootNode, identity, VP);
    glUniform3fv(glGetUniformLocation(shader->get(), "cameraPos"), 1, glm::value_ptr(cameraPos));
    
}

// --- renderNode ---
static void renderNode(SceneNode *node) {
    if(node->nodeType == GEOMETRY && node->vertexArrayObjectID != -1) {
        bool isSunGeom = (node == sunNode);
        glUniform1i(glGetUniformLocation(shader->get(), "isSun"), isSunGeom ? 1 : 0);
        glBindVertexArray(node->vertexArrayObjectID);
        if(node->hasTexture && node->textureID != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, node->textureID);
            glUniform1i(glGetUniformLocation(shader->get(), "diffuseTexture"), 0);
            glUniform1i(glGetUniformLocation(shader->get(), "useTexture"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader->get(), "useTexture"), 0);
        }
        glUniformMatrix4fv(glGetUniformLocation(shader->get(), "modelMatrix"), 1, GL_FALSE, glm::value_ptr(node->modelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(shader->get(), "MVP"), 1, GL_FALSE, glm::value_ptr(node->MVP));
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(node->modelMatrix)));
        glUniformMatrix3fv(glGetUniformLocation(shader->get(), "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
        glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    for(auto child : node->children)
        renderNode(child);
}

// --- renderFrame ---
void renderFrame(GLFWwindow *window) {
    // Shadow Pass
    glm::mat4 lightProjection = glm::ortho(-150.0f, 150.0f, -150.0f, 150.0f, 1.0f, 400.0f);
    glm::mat4 lightView = glm::lookAt(lightNode->position, glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadowShader->activate();
    glUniformMatrix4fv(glGetUniformLocation(shadowShader->get(), "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    renderShadowScene(rootNode, glm::mat4(1.0f));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Main Render Pass
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glViewport(0, 0, winWidth, winHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader->activate();
    glUniformMatrix4fv(glGetUniformLocation(shader->get(), "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glUniform1i(glGetUniformLocation(shader->get(), "shadowMap"), 1);
    renderNode(rootNode);
}
