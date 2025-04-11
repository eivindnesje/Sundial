#include "skybox.hpp"
#include <stb_image.h>
#include "utilities/shader.hpp"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

namespace Gloom {

// Vertex data for a cube used to draw the skybox
float skyboxVertices[] = {
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
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
            
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

static unsigned int loadCubemap(const std::vector<std::string>& faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    // Iterate through each image and assign it to the correct face of the cubemap texture
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        // Load the image data using stb_image
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            // Determine the image format based on the number of channels
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            // Specify the texture image for the corresponding face of the cubemap
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height,
                         0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    // Set texture parameters for filtering and wrapping.
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

Skybox::Skybox() : cubemapTexture(0), VAO(0), VBO(0), shader(nullptr) {}

Skybox::~Skybox()
{
    if(shader) {
        shader->destroy();
        delete shader;
    }
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void Skybox::init(const std::vector<std::string>& faces,
                  const std::string& shaderVertPath,
                  const std::string& shaderFragPath)
{
    cubemapTexture = loadCubemap(faces);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Upload the vertex data to the GPU
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Setup shader
    shader = new Shader();
    shader->makeBasicShader(shaderVertPath, shaderFragPath);
}

void Skybox::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3 sunDir)
{
    // Set the depth function to GL_LEQUAL to ensure the skybox is rendered behind all other geometry
    glDepthFunc(GL_LEQUAL);
    shader->activate();

    // Remove the translation from the view matrix to keep the skybox static relative to the camera
    glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
    // Send the modified view matrix and projection matrix to the shader
    glUniformMatrix4fv(glGetUniformLocation(shader->get(), "view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
    glUniformMatrix4fv(glGetUniformLocation(shader->get(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Send the sun direction to the shader for lighting effects
    glUniform3fv(glGetUniformLocation(shader->get(), "sunDir"), 1, glm::value_ptr(sunDir));
    
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glUniform1i(glGetUniformLocation(shader->get(), "skybox"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    shader->deactivate();
    glDepthFunc(GL_LESS);
}


} // namespace Gloom
