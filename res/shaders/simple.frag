#version 430 core

in vec3 normal_out;
in vec2 textureCoordinates;
in vec3 fragPos;

struct Sun {
    vec3 direction;
    vec3 color;
};

uniform Sun sun;
uniform vec3 cameraPos;

uniform sampler2D diffuseTexture;
uniform bool useTexture;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

out vec4 color;

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float dither(vec2 uv) {
    return (rand(uv)*2.0 - 1.0) / 256.0;
}

void main()
{
    vec3 normal = normalize(normal_out);
    vec3 ambientLight = vec3(0.1);
    float shininess = 32.0;
    vec3 emission = vec3(0.0);
    
    // Directional light: use sun.direction.
    // Since the sun is far away, all rays are parallel.
    vec3 lightDir = normalize(sun.direction); // light direction points from the sun to the scene
    float diffuseIntensity = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = sun.color * diffuseIntensity;
    
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 viewDir = normalize(cameraPos - fragPos);
    float spec = pow(max(dot(reflectDir, viewDir), 0.0), shininess);
    vec3 specular = sun.color * spec;
    
    // Shadow mapping:
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float shadow = currentDepth - bias > closestDepth ? 0.5 : 1.0;
    
    vec3 litColor = ambientLight + (diffuse + specular) * shadow + emission;
    
    if (useTexture) {
        vec4 texColor = texture(diffuseTexture, textureCoordinates);
        litColor *= texColor.rgb;
    }
    
    float noise = dither(textureCoordinates);
    litColor += noise;
    color = vec4(litColor, 1.0);
}
