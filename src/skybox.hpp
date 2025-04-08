#ifndef SKYBOX_HPP
#define SKYBOX_HPP

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Gloom {

    class Shader; // Forward declaration

    class Skybox {
    public:
        Skybox();
        ~Skybox();

        // Initializes the procedural skybox.
        // Now only the shader vertex and fragment file paths are needed.
        void init(const std::string& shaderVertPath,
                  const std::string& shaderFragPath);

        // Renders the skybox.
        // dayFactor is between 0 (night) and 1 (full day);
        // sunDir and moonDir are the normalized light directions.
        void render(const glm::mat4& view, const glm::mat4& projection,
                    float dayFactor, const glm::vec3& sunDir, const glm::vec3& moonDir);

    private:
        unsigned int VAO, VBO;
        Shader* shader;
    };

}

#endif
