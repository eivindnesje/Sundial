#version 430 core
layout(location = 0) in vec3 position;
uniform mat4 modelMatrix;
uniform mat4 viewProjection;
out vec3 fragPos; // pass to fragment shader for distance calculations
void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    fragPos = position;  // Use the original object-space position (assumed centered at 0)
    gl_Position = viewProjection * worldPos;
}
