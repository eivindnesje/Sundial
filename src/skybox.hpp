#ifndef SKYBOX_HPP
#define SKYBOX_HPP

#include <vector>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Gloom {

    class Shader; // Forward declaration from your shader.hpp

    class Skybox {
    public:
        Skybox();
        ~Skybox();

        // Initializes the cubemap from 6 faces and loads the skybox shaders.
        // 'faces' should contain the file paths to the cubemap images in the following order:
        // Right, Left, Top, Bottom, Front, Back.
        void init(const std::vector<std::string>& faces,
                  const std::string& shaderVertPath,
                  const std::string& shaderFragPath);

        // Renders the skybox.
        // The view matrix should have the translation removed (this is done internally).
        void render(const glm::mat4& view, const glm::mat4& projection);

    private:
        unsigned int cubemapTexture;
        unsigned int VAO, VBO;
        Shader* shader;
    };

}

#endif
