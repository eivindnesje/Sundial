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

// *** NEW: Include our new skybox header ***
#include "skybox.hpp"

// Global scene pointers
SceneNode *rootNode = nullptr;
SceneNode *lightNode = nullptr; // Directional light (the only light, which drives both lighting and shadow mapping)

// For the light sources (if needed for further use)
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

// *** NEW: Skybox pointer ***
static Gloom::Skybox* skybox = nullptr;

CommandLineOptions options;

// Timing variables
static double totalElapsedTime = 0.0;
static double sceneElapsedTime = 0.0;

// Sun movement constants
static const float SIM_SECONDS_PER_REAL_HOUR = 1.0f;
static const float FULL_DAY = 24.0f * SIM_SECONDS_PER_REAL_HOUR;

// NEW: Global sun direction for the skybox animation (and for lighting/shadowing)
static glm::vec3 skyboxSunDir = glm::vec3(0.0f);

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
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
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

    // Load the lighting shader pair.
    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/lighting.vert", "../res/shaders/lighting.frag");
    shader->activate();

    // Load the shadow mapping shader pair.
    shadowShader = new Gloom::Shader();
    shadowShader->makeBasicShader("../res/shaders/shadow.vert", "../res/shaders/shadow.frag");

    initShadowMap();

    // Create scene graph root.
    rootNode = createSceneNode();

    // Create the directional light node.
    lightNode = createSceneNode();
    lightNode->nodeType = POINT_LIGHT;
    // Starting value (will be updated in updateFrame).
    lightNode->position = glm::vec3(0.0f, 100.0f, 50.0f);
    lightNode->lightColor = glm::vec3(1.0f);
    rootNode->children.push_back(lightNode);

    // Load the sundial model.
    std::string diffuseTexName;
    Mesh sundialMesh = loadOBJModel("../res/models/sundial.obj", "../res/models/", diffuseTexName);
    unsigned int sundialVAO = generateBuffer(sundialMesh);
    SceneNode *sundialNode = createSceneNode();
    sundialNode->vertexArrayObjectID = sundialVAO;
    sundialNode->VAOIndexCount = sundialMesh.indices.size();
    sundialNode->position = glm::vec3(0.0f);
    sundialNode->scale = glm::vec3(0.5f);
    sundialNode->rotation.x = glm::radians(-90.0f);
    if (!diffuseTexName.empty()) {
        std::string texturePath = "../res/models/" + diffuseTexName;
        unsigned int tex = loadTexture(texturePath);
        sundialNode->textureID = tex;
        sundialNode->hasTexture = true;
    }
    rootNode->children.push_back(sundialNode);

    // *** Initialize the Skybox ***
    {
        std::vector<std::string> skyFaces = {
            "../res/textures/skybox/right.jpg",
            "../res/textures/skybox/left.jpg",
            "../res/textures/skybox/top.jpg",
            "../res/textures/skybox/bottom.jpg",
            "../res/textures/skybox/front.jpg",
            "../res/textures/skybox/back.jpg"
        };
        skybox = new Gloom::Skybox();
        // (Ensure that your skybox class has been updated if you decide to pass sun direction.)
        skybox->init(skyFaces, "../res/shaders/skybox.vert", "../res/shaders/skybox.frag");
    }

    totalElapsedTime = sceneElapsedTime = getTimeDeltaSeconds();
    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;
}

// --- renderShadowScene ---
static void renderShadowScene(SceneNode *node, glm::mat4 parentModel) {
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

    // Compute day progress (in [0,1]) and animate the sun direction.
    double dayProgress = fmod(sceneElapsedTime, FULL_DAY) / FULL_DAY;
    float theta = 2.0f * glm::pi<float>() * (dayProgress);
    const float orbitRadius = 3.0f;
    const float zOffset = 1.0f;
    glm::vec3 sunPos(orbitRadius * cos(theta), orbitRadius * sin(theta), zOffset);
    skyboxSunDir = glm::normalize(sunPos);

    // Update the light node so that it comes from the same direction.
    // For directional lights, we set the light's position far along the negative sun direction.
    float lightDistance = 300.0f; // Adjust as needed
    lightNode->position = -skyboxSunDir * lightDistance;

    // Update camera parameters as before.
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glm::vec3 center(0.0f);
    glm::vec3 cameraPos;
    cameraPos.x = center.x + cameraRadius * cos(glm::radians(cameraPitch)) * sin(glm::radians(cameraYaw));
    cameraPos.y = center.y + cameraRadius * sin(glm::radians(cameraPitch));
    cameraPos.z = center.z + cameraRadius * cos(glm::radians(cameraPitch)) * cos(glm::radians(cameraYaw));
    glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0,1,0));
    glm::mat4 projection = glm::perspective(glm::radians(80.0f), float(winWidth) / float(winHeight), 0.1f, 350.f);
    glm::mat4 VP = projection * view;
    glm::mat4 identity = glm::mat4(1.0f);
    lightIndex = 0;
    updateNodeTransformations(rootNode, identity, VP);
    glUniform3fv(glGetUniformLocation(shader->get(), "cameraPos"), 1, glm::value_ptr(cameraPos));
}

// --- renderNode ---
static void renderNode(SceneNode *node) {
    if(node->nodeType == GEOMETRY && node->vertexArrayObjectID != -1) {
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
    // Expand the orthographic frustum (adjust the boundaries if needed for your scene)
    float orthoSize = 150.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1.0f, 400.0f);
    // Compute the light view matrix using an appropriate up vector.
    glm::vec3 lightUp = (fabs(skyboxSunDir.y) < 0.999f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                        : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::mat4 lightView = glm::lookAt(lightNode->position, glm::vec3(0, 0, 0), lightUp);
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

    shader->activate();
    // Update the lighting uniforms with our new sun direction and light color.
    glUniform3f(glGetUniformLocation(shader->get(), "sunDir"),
                skyboxSunDir.x, skyboxSunDir.y, skyboxSunDir.z);
    glUniform3f(glGetUniformLocation(shader->get(), "sunColor"),
                lightNode->lightColor.x, lightNode->lightColor.y, lightNode->lightColor.z);
    glUniformMatrix4fv(glGetUniformLocation(shader->get(), "lightSpaceMatrix"),
                       1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    // Bind the shadow map texture.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glUniform1i(glGetUniformLocation(shader->get(), "shadowMap"), 1);

    // Render scene objects (the sundial and others).
    renderNode(rootNode);

    // Render the skybox.
    // (If you update your skybox shader to take the sun direction as a parameter, call it here.)
    skybox->render(view, projection, skyboxSunDir);
}
