#pragma once

#include <utilities/window.hpp>
#include "sceneGraph.hpp"

struct LightSource {
    glm::vec3 position;
    glm::vec3 color;
};

void initScene(GLFWwindow *window, CommandLineOptions options);
void updateFrame(GLFWwindow *window);
void renderFrame(GLFWwindow *window);
