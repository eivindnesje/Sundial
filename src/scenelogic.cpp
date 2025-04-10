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
#include "skybox.hpp"

// Global scene pointers
SceneNode *rootNode;
SceneNode *lightNode;
SceneNode *sundialNode;

// Camera parameters
static glm::vec3 cameraPos;
float cameraYaw = 0.0f;
float cameraPitch = 25.0f;
const float cameraRadius = 200.0f;
static bool revolvingMode = false;
static float fixedRevolvePitch = cameraPitch; 

// Shadow mapping
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
static unsigned int shadowFBO = 0;
static unsigned int shadowMap = 0;

// Shaders
static Gloom::Shader *shader;
static Gloom::Shader *shadowShader;

// Skybox
static Gloom::Skybox* skybox;

// Options
CommandLineOptions options;

// Timing variables
static double elpasedTime = 0.0;

// Simulation speed
static const float SIM_SECONDS_PER_REAL_HOUR = 2.0f;
static const float FULL_DAY = 24.0f * SIM_SECONDS_PER_REAL_HOUR;

// Sun direction
static glm::vec3 sunDir = glm::vec3(0.0f);

// Matrices
static glm::mat4 view;
static glm::mat4 projection;
static glm::mat4 lightSpaceMatrix;

// Mouse control
static double mouseSensitivity = 0.2;
static double lastMouseX = 800.0;
static double lastMouseY = 600.0;


static void mouseCallback(GLFWwindow* window, double x, double y) {
    // Skip if revolving mode
    if (revolvingMode) return;

    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);

    double deltaX = x - lastMouseX;
    double deltaY = y - lastMouseY;

    // Moves the camera in a "sphere" around the origo
    cameraYaw   += mouseSensitivity * deltaX;
    cameraPitch += mouseSensitivity * deltaY;
    if(cameraPitch > 89.0f)  cameraPitch = 89.0f;
    if(cameraPitch < -89.0f) cameraPitch = -89.0f;

    glfwSetCursorPos(window, winWidth/2, winHeight/2);
    lastMouseX = winWidth/2;
    lastMouseY = winHeight/2;
}


static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // Toggle revolving mode on left button press.
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        
        revolvingMode = !revolvingMode;
        if(revolvingMode) {
            // On switching to revolving mode, capture the current pitch so the height stays fixed.
            fixedRevolvePitch = cameraPitch;
        }
        else {
            // Reset mouse position for manual camera control
            int winWidth, winHeight;
            glfwGetWindowSize(window, &winWidth, &winHeight);
            glfwSetCursorPos(window, winWidth/2, winHeight/2);
            lastMouseX = winWidth/2;
            lastMouseY = winHeight/2;
        }
    }
}


static void initShadowMap() {
    glGenTextures(1, &shadowMap);
    // Bind the newly created texture as a 2D texture
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    
    // Setting the texture as a depthmap
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // Configure the texture wrapping mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    // Border color used when texture coordinates fall outside [0,1]
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Generate a FBO for rendering the shadow map
    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    
    // Attach the shadow map texture as the depth attachment for the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
    
    // Disabling both drawing and reading color buffers
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    // Check if the framebuffer is complete; if not, output an error.
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Shadow framebuffer not complete!" << std::endl;
    
    // Unbinding
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initScene(GLFWwindow *window, CommandLineOptions sceneOptions) {
    options = sceneOptions;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    // Enable revolving mode if autorotation is activated
    if(options.autorotate) {
        revolvingMode = true;
    }
    
    // Load and activate the main shader pair
    shader = new Gloom::Shader();
    shader->makeBasicShader("../res/shaders/main.vert", "../res/shaders/main.frag");
    shader->activate();

    // Load the shadow mapping shader pair
    shadowShader = new Gloom::Shader();
    shadowShader->makeBasicShader("../res/shaders/shadow.vert", "../res/shaders/shadow.frag");

    // Initialize the shadowmap
    initShadowMap();

    rootNode = createSceneNode();

    // Create a scene node for the lightnode (sunlight)
    lightNode = createSceneNode();
    lightNode->nodeType = POINT_LIGHT;
    lightNode->position = glm::vec3(0.0f, 100.0f, 50.0f);
    lightNode->lightColor = glm::vec3(1.0f);
    rootNode->children.push_back(lightNode);

    // Load the sundial model
    std::string diffuseTexName;
    Mesh sundialMesh = loadOBJModel("../res/models/sundial.obj", "../res/models/", diffuseTexName);
    // Generate and configure the VAO for the sundial
    unsigned int sundialVAO = generateBuffer(sundialMesh);

    // Create a scene node for the sundial model
    sundialNode = createSceneNode();
    sundialNode->vertexArrayObjectID = sundialVAO;
    sundialNode->VAOIndexCount = sundialMesh.indices.size();
    sundialNode->position = glm::vec3(0.0f);
    sundialNode->scale = glm::vec3(0.5f);
    sundialNode->rotation.x = glm::radians(-90.0f);

    // If a diffuse texture was loaded, assign it to the sundial node.
    if (!diffuseTexName.empty()) {
        std::string texturePath = "../res/textures/" + diffuseTexName;
        unsigned int tex = loadTexture(texturePath);
        sundialNode->textureID = tex;
        sundialNode->hasTexture = true;
    }
    // Attach the sundial node to the scene graph.
    rootNode->children.push_back(sundialNode);


    // Define the file paths for the six faces of the skybox.
    std::vector<std::string> skyFaces = {
        "../res/textures/skybox/right.jpg",
        "../res/textures/skybox/left.jpg",
        "../res/textures/skybox/top.jpg",
        "../res/textures/skybox/bottom.jpg",
        "../res/textures/skybox/front.jpg",
        "../res/textures/skybox/back.jpg"
    };
    // Create and initialize the skybox.
    skybox = new Gloom::Skybox();
    skybox->init(skyFaces, "../res/shaders/skybox.vert", "../res/shaders/skybox.frag");
    
    elpasedTime = getTimeDeltaSeconds();
    
    std::cout << fmt::format("Initialized scene with {} SceneNodes.", totalChildren(rootNode)) << std::endl;
}


void updateNodeTransformations(SceneNode *node, glm::mat4 parentModel, glm::mat4 parentVP) {
    // Build the local transformation matrix for the current node:
    // 1. Translate by the node's position
    // 2. Translate by the node's reference point (used for pivot adjustments)
    // 3. Rotate around the Y axis using the node's rotation.y
    // 4. Rotate around the X axis using the node's rotation.x
    // 5. Rotate around the Z axis using the node's rotation.z
    // 6. Scale the node uniformly or non-uniformly as specified
    // 7. Translate back by the negative reference point to retain pivot correctness
    glm::mat4 transformationMatrix =
        glm::translate(glm::mat4(1.0f), node->position) *
        glm::translate(glm::mat4(1.0f), node->referencePoint) *
        glm::rotate(glm::mat4(1.0f), node->rotation.y, glm::vec3(0, 1, 0)) *
        glm::rotate(glm::mat4(1.0f), node->rotation.x, glm::vec3(1, 0, 0)) *
        glm::rotate(glm::mat4(1.0f), node->rotation.z, glm::vec3(0, 0, 1)) *
        glm::scale(glm::mat4(1.0f), node->scale) *
        glm::translate(glm::mat4(1.0f), -node->referencePoint);
        
    // Combine parent's model matrix with the current node's transformation matrix.
    node->modelMatrix = parentModel * transformationMatrix;
    
    // Compute Model View Projection matrix
    node->MVP = parentVP * transformationMatrix;

    // Recursively update transformations for each child of the current node.
    for(auto child : node->children)
        updateNodeTransformations(child, node->modelMatrix, node->MVP);
}

// --- updateFrame ---
void updateFrame(GLFWwindow *window) {
    double timeDelta = getTimeDeltaSeconds();
    elpasedTime += timeDelta;

    float angularSpeed = 2.0f * glm::pi<float>() / FULL_DAY;
    float angle = angularSpeed * elpasedTime;

    // Sun moves on a circular path in the xy plane with a constant z offset
    float orbitRadius = 200.0f;
    float zOffset = 70.0f;
    glm::vec3 sunPos(orbitRadius * cos(angle), orbitRadius * sin(angle), zOffset);
    lightNode->position = sunPos;
    
    // The sun faces the origin, so the direction is from sunPos to (0,0,0)
    sunDir = glm::normalize(sunPos);

    // Update camera parameters as before.
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glm::vec3 center(0.0f);

    if(revolvingMode) {
        cameraYaw += 5.0f * (float)timeDelta;  
    }
    float effectivePitch = revolvingMode ? fixedRevolvePitch : cameraPitch;

    cameraPos.x = center.x + cameraRadius * cos(glm::radians(effectivePitch)) * sin(glm::radians(cameraYaw));
    cameraPos.y = center.y + cameraRadius * sin(glm::radians(effectivePitch));
    cameraPos.z = center.z + cameraRadius * cos(glm::radians(effectivePitch)) * cos(glm::radians(cameraYaw));

    view = glm::lookAt(cameraPos, center, glm::vec3(0,1,0));
    projection = glm::perspective(glm::radians(80.0f), float(winWidth) / float(winHeight), 0.1f, 350.f);
    glm::mat4 VP = projection * view;
    glm::mat4 identity = glm::mat4(1.0f);
    
    updateNodeTransformations(rootNode, identity, VP);

    float orthoSize = 150.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1.0f, 400.0f);
    glm::vec3 lightUp = (fabs(sunDir.y) < 0.999f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                        : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::mat4 lightView = glm::lookAt(lightNode->position, glm::vec3(0, 0, 0), lightUp);
    lightSpaceMatrix = lightProjection * lightView;
}


static void renderShadowScene(SceneNode *node) {
    // ShadowMVP uses the light-space matrix, which transforms world coordinates to the light's view
    glm::mat4 shadowMVP = lightSpaceMatrix * node->modelMatrix;
    glUniformMatrix4fv(glGetUniformLocation(shadowShader->get(), "MVP"), 
                       1, GL_FALSE, glm::value_ptr(shadowMVP));
    
    if(node->nodeType == GEOMETRY && node->vertexArrayObjectID != -1) {
        glBindVertexArray(node->vertexArrayObjectID);
        glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    
    for(auto child : node->children)
        renderShadowScene(child);
}


static void renderNode(SceneNode *node) {

    if(node->nodeType == GEOMETRY && node->vertexArrayObjectID != -1) {
         glBindVertexArray(node->vertexArrayObjectID);
        
        // Set texture uniforms for this node
        if (node->hasTexture && node->textureID != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, node->textureID);
            glUniform1i(glGetUniformLocation(shader->get(), "diffuseTexture"), 0);
            glUniform1i(glGetUniformLocation(shader->get(), "useTexture"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader->get(), "useTexture"), 0);
        }
        
        // Set the matrices for the node
        glUniformMatrix4fv(glGetUniformLocation(shader->get(), "modelMatrix"), 
                            1, GL_FALSE, glm::value_ptr(node->modelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(shader->get(), "MVP"), 
                            1, GL_FALSE, glm::value_ptr(node->MVP));
        
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(node->modelMatrix)));
        glUniformMatrix3fv(glGetUniformLocation(shader->get(), "normalMatrix"), 
                            1, GL_FALSE, glm::value_ptr(normalMatrix));
        
        glDrawElements(GL_TRIANGLES, node->VAOIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    for(auto child : node->children)
        renderNode(child);
}


void renderFrame(GLFWwindow *window) {

    // Shadow mapping render
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    shadowShader->activate();

    // Render the scene from the light's perspective. This writes depth values
    renderShadowScene(rootNode);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Main render
    int winWidth, winHeight;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    glViewport(0, 0, winWidth, winHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->activate();
    // Light-space matrix used for shadow mapping calculations
    glUniformMatrix4fv(glGetUniformLocation(shader->get(), "lightSpaceMatrix"),
                    1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glUniform3fv(glGetUniformLocation(shader->get(), "sunDir"), 1, glm::value_ptr(sunDir));
    glUniform3fv(glGetUniformLocation(shader->get(), "sunColor"), 1, glm::value_ptr(lightNode->lightColor));
    glUniform3fv(glGetUniformLocation(shader->get(), "cameraPos"), 1, glm::value_ptr(cameraPos));

    // Bind the shadow map texture to texture unit 1.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    // Inform the shader that the shadow map is available on texture unit 1.
    glUniform1i(glGetUniformLocation(shader->get(), "shadowMap"), 1);

    // Render scene objects (in this case only the sundial) using their vertex data and transformations.
    renderNode(rootNode);

    // Skybox Render
    skybox->render(view, projection, sunDir);

    shader->activate();
}

