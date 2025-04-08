// <scenelogic.cpp>
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

// New: Include the skybox header.
#include "skybox.hpp"

// Global scene pointers
SceneNode *rootNode = nullptr;
SceneNode *lightNode = nullptr; // Represents the sun (for lighting and shadows)
// (The sun is no longer rendered as a separate geometry)
  
static const int numLights = 1;
int lightIndex = 0;
LightSource lightSources[numLights];

// Camera parameters
float cameraYaw = 0.0f;
float cameraPitch = 45.0f;
const float cameraRadius = 200.0f;

glm::vec3 sunDir;
glm::vec3 moonDir;

// Shadow mapping globals
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
static unsigned int shadowFBO = 0;
static unsigned int shadowMap = 0;

// Shaders
static Gloom::Shader *modelShader = nullptr;
static Gloom::Shader *shadowShader = nullptr;

// Skybox pointer (procedural, animated)
static Gloom::Skybox* skybox = nullptr;

CommandLineOptions options;

// Timing variables
static double totalElapsedTime = 0.0;
static double sceneElapsedTime = 0.0;

// Sun movement constants
static const float SIM_SECONDS_PER_REAL_HOUR = 1.0f; 
static const float FULL_DAY = 24.0f * SIM_SECONDS_PER_REAL_HOUR;

// Mouse control
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
    // Process light nodes if needed.
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
        default:
            break;
    }
    for(auto child : node->children)
        updateNodeTransformations(child, node->modelMatrix, node->MVP);
}

// --- initScene ---
void initScene(GLFWwindow *window, CommandLineOptions sceneOptions) {
    options = sceneOptions;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);

    // Load the new model shader.
    modelShader = new Gloom::Shader();
    modelShader->makeBasicShader("../res/shaders/model.vert", "../res/shaders/model.frag");
    modelShader->activate();

    // Load the new shadow shader.
    shadowShader = new Gloom::Shader();
    shadowShader->makeBasicShader("../res/shaders/shadow.vert", "../res/shaders/shadow.frag");

    initShadowMap();

    // Create scene graph root.
    rootNode = createSceneNode();

    // Create directional light node (sun used for lighting/shadowing).
    lightNode = createSceneNode();
    lightNode->nodeType = POINT_LIGHT;
    lightNode->position = glm::vec3(0.0f, 100.0f, 50.0f); // Will be updated in updateFrame().
    lightNode->lightColor = glm::vec3(1.0f);
    rootNode->children.push_back(lightNode);

    // (Do not add any visible sun geometry.)

    // Load the sundial model as before.
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

    // Initialize procedural skybox.
    {
        skybox = new Gloom::Skybox();
        skybox->init("../res/shaders/skybox.vert", "../res/shaders/skybox.frag");
    }

    totalElapsedTime = sceneElapsedTime = getTimeDeltaSeconds();
    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;
}

// --- renderShadowScene ---
static void renderShadowScene(SceneNode *node, glm::mat4 parentModel) {
    // Skip non-model geometry if needed.
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

    // Compute sun position on a circular orbit in the xy-plane with a constant z offset.
    float orbitRadius = 150.0f;
    float zOffset = 50.0f;
    glm::vec3 sunPos(orbitRadius * cos(angle), orbitRadius * sin(angle), zOffset);
    lightNode->position = sunPos;

    // Compute sun direction (pointing from the sun toward the origin).
    sunDir = glm::normalize(sunPos);
    // Define moon direction as opposite to the sun.
    moonDir = -sunDir;

    // Compute dayFactor from the sun’s elevation (dot with world-up).
    float dayFactor = glm::clamp(glm::dot(sunDir, glm::vec3(0,1,0)), 0.0f, 1.0f);


    // Update model shader lighting uniforms.
    modelShader->activate();
    glUniform3fv(glGetUniformLocation(modelShader->get(), "sunDir"), 1, glm::value_ptr(sunDir));
    // Set sunColor (you can adjust intensity as needed).
    glUniform3f(glGetUniformLocation(modelShader->get(), "sunColor"), 1.0f, 0.95f, 0.9f);
    glUniform3fv(glGetUniformLocation(modelShader->get(), "moonDir"), 1, glm::value_ptr(moonDir));
    glUniform3f(glGetUniformLocation(modelShader->get(), "moonColor"), 0.6f, 0.65f, 0.8f);
    // Base ambient light.
    glUniform3f(glGetUniformLocation(modelShader->get(), "baseAmbient"), 0.2f, 0.2f, 0.25f);

    // Update camera.
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glm::vec3 center(0.0f);
    glm::vec3 cameraPos;
    cameraPos.x = center.x + cameraRadius * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
    cameraPos.y = center.y + cameraRadius * sin(glm::radians(cameraPitch));
    cameraPos.z = center.z + cameraRadius * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));
    glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0, 1, 0));
    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(winWidth)/float(winHeight), 0.1f, 350.f);
    glm::mat4 VP = projection * view;
    glm::mat4 identity = glm::mat4(1.0f);
    lightIndex = 0;
    updateNodeTransformations(rootNode, identity, VP);
    glUniform3fv(glGetUniformLocation(modelShader->get(), "cameraPos"), 1, glm::value_ptr(cameraPos));

    // Set shadow parameters.
    // (Also set any material properties like shininess.)
    glUniform1f(glGetUniformLocation(modelShader->get(), "shininess"), 32.0f);
}

// --- renderNode --- 
static void renderNode(SceneNode *node) {
    if(node->nodeType == GEOMETRY && node->vertexArrayObjectID != -1) {
        glUniform1i(glGetUniformLocation(modelShader->get(), "useTexture"),
                    (node->hasTexture && node->textureID != 0) ? 1 : 0);
        if(node->hasTexture && node->textureID != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, node->textureID);
            glUniform1i(glGetUniformLocation(modelShader->get(), "diffuseTexture"), 0);
        }
        glUniformMatrix4fv(glGetUniformLocation(modelShader->get(), "modelMatrix"),
                           1, GL_FALSE, glm::value_ptr(node->modelMatrix));
        // Compute and set the normal matrix.
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(node->modelMatrix)));
        glUniformMatrix3fv(glGetUniformLocation(modelShader->get(), "normalMatrix"),
                           1, GL_FALSE, glm::value_ptr(normalMatrix));
        // Also set the model's MVP if used.
        glBindVertexArray(node->vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    for(auto child : node->children)
        renderNode(child);
}

void renderFrame(GLFWwindow *window) {
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glm::vec3 center(0.0f);
    glm::vec3 cameraPos;
    cameraPos.x = center.x + cameraRadius * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
    cameraPos.y = center.y + cameraRadius * sin(glm::radians(cameraPitch));
    cameraPos.z = center.z + cameraRadius * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));
    glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0, 1, 0));
    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(winWidth) / float(winHeight), 0.1f, 350.f);
    
    // --- Shadow Pass ---
    glm::mat4 lightProjection = glm::ortho(-150.0f, 150.0f, -150.0f, 150.0f, 1.0f, 400.0f);
    glm::mat4 lightView = glm::lookAt(lightNode->position, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadowShader->activate();
    glUniformMatrix4fv(glGetUniformLocation(shadowShader->get(), "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    renderShadowScene(rootNode, glm::mat4(1.0f));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Main Render Pass ---
    glViewport(0, 0, winWidth, winHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    modelShader->activate();
    glUniformMatrix4fv(glGetUniformLocation(modelShader->get(), "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glUniform1i(glGetUniformLocation(modelShader->get(), "shadowMap"), 1);
    renderNode(rootNode);

    // --- Procedural Skybox Render Pass ---
    // The skybox is rendered last with depth function modifications.
    // In addition, we pass the current dayFactor and light directions.
    float dayFactor = glm::clamp(glm::dot(sunDir, glm::vec3(0, 1, 0)), 0.0f, 1.0f);
    skybox->render(view, projection, dayFactor, sunDir, moonDir);

}
