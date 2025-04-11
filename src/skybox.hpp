#ifndef SKYBOX_HPP
#define SKYBOX_HPP

#include <vector>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Gloom {

    class Shader;

    class Skybox {
    public:
        Skybox();
        ~Skybox();

        // Initializes the cubemap from 6 faces and loads the skybox shaders
        void init(const std::vector<std::string>& faces,
                  const std::string& shaderVertPath,
                  const std::string& shaderFragPath);

        // Renders the skybox
        void render(const glm::mat4& view, const glm::mat4& projection,  const glm::vec3 sunDir);

    private:
        unsigned int cubemapTexture;
        unsigned int VAO, VBO;
        Shader* shader;
    };

}

#endif
