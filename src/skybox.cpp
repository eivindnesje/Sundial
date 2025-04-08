#include "skybox.hpp"
#include <stb_image.h> // May not even be needed for procedural shader.
#include "utilities/shader.hpp"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

namespace Gloom {

// Vertex data for a cube.
float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
            
    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
            
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
            
    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
            
    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,
            
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

Skybox::Skybox() : VAO(0), VBO(0), shader(nullptr) {}

Skybox::~Skybox() {
    if(shader) {
        shader->destroy();
        delete shader;
    }
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

// Initialize the procedural skybox by only compiling the shaders and creating the cube.
void Skybox::init(const std::string& shaderVertPath,
                  const std::string& shaderFragPath) {
    // Setup VAO and VBO for a cube.
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Create and compile the procedural skybox shader.
    shader = new Shader();
    shader->makeBasicShader(shaderVertPath, shaderFragPath);
}

void Skybox::render(const glm::mat4& view, const glm::mat4& projection,
                    float dayFactor, const glm::vec3& sunDir, const glm::vec3& moonDir) {
    // Change depth function so that skybox fragments always pass.
    glDepthFunc(GL_LEQUAL);
    // Optionally disable face culling if needed.
    glDisable(GL_CULL_FACE);
    
    shader->activate();

    // Remove translation from the view matrix.
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
    glUniformMatrix4fv(glGetUniformLocation(shader->get(), "view"), 1, GL_FALSE, glm::value_ptr(viewNoTrans));
    glUniformMatrix4fv(glGetUniformLocation(shader->get(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Pass uniforms for the procedural effects.
    glUniform3fv(glGetUniformLocation(shader->get(), "sunDir"), 1, glm::value_ptr(sunDir));
    glUniform3fv(glGetUniformLocation(shader->get(), "moonDir"), 1, glm::value_ptr(moonDir));
    glUniform1f(glGetUniformLocation(shader->get(), "dayFactor"), dayFactor);
    // Adjust overall brightness intensity.
    glUniform1f(glGetUniformLocation(shader->get(), "skyboxIntensity"), 0.5f);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    shader->deactivate();
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
}

} // namespace Gloom
