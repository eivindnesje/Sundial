#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

uniform mat4 modelMatrix;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;     // Inverse transpose of modelMatrix.
uniform mat4 lightSpaceMatrix; // For shadow mapping.

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 ShadowCoord;

void main() {
    vec4 worldPos = modelMatrix * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = normalize(normalMatrix * aNormal);
    TexCoords = aTexCoords;
    ShadowCoord = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}
