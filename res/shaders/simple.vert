#version 430 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal_in;
layout(location = 2) in vec2 textureCoordinates_in;

uniform layout(location = 3) mat4 modelMatrix;
uniform layout(location = 4) mat4 MVP;
uniform layout(location = 5) mat3 normalMatrix;

out vec3 normal_out;
out vec2 textureCoordinates;
out vec3 fragPos;

void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    fragPos = worldPos.xyz;
    normal_out = normalize(normalMatrix * normal_in);
    textureCoordinates = textureCoordinates_in;
    gl_Position = MVP * vec4(position, 1.0);
}
