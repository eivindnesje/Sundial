#version 430 core

layout(location = 0) in vec3 normal_in;
layout(location = 1) in vec2 textureCoordinates;
layout(location = 2) in vec3 fragPos;

struct LightSource {
    vec3 position;
    vec3 color;
};

uniform LightSource lights[3];
uniform int  numLights; 
uniform vec3 cameraPos;

uniform float attConst;   // la
uniform float attLinear;  // lb
uniform float attQuad;    // lc

out vec4 color;

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898,78.233))) * 43758.5453);
}

float dither(vec2 uv) {
    return (rand(uv)*2.0 - 1.0) / 256.0;
}

void main()
{
    vec3 normal = normalize(normal_in);

    vec3 ambientLight  = vec3(0.1);
    float shininess    = 32.0;

    vec3 emission = vec3(0.0);

    vec3 totalDiffuse  = vec3(0.0);
    vec3 totalSpecular = vec3(0.0);

    for (int i = 0; i < numLights; i++)
    {
        float shadowFraction = 1;
        vec3 toLight = lights[i].position - fragPos;


        vec3 lightDirection = normalize(lights[i].position - fragPos);
        float diffuseIntensity = max(dot(normal, lightDirection), 0.0);

        float distance = length(lights[i].position - fragPos);
        float attenuation = 1.0 / (attConst + attLinear * distance + attQuad * distance * distance);

        diffuseIntensity *= attenuation * shadowFraction;

        totalDiffuse += lights[i].color  * diffuseIntensity;

        if (diffuseIntensity > 0.0) {
            vec3 reflectedLightDirection = reflect(-lightDirection, normal);
            vec3 surfaceEyeDirection = normalize(cameraPos - fragPos);

            float specularIntensity = max(dot(reflectedLightDirection, surfaceEyeDirection), 0.0);
            specularIntensity = pow(specularIntensity, shininess);
            specularIntensity *= attenuation * shadowFraction;

            totalSpecular += lights[i].color * specularIntensity;
        }
    }

    vec3 finalColor = ambientLight + totalDiffuse + totalSpecular + emission;
    color = vec4(finalColor, 1.0);
    
    float noise = dither(textureCoordinates);
    color.rgb += noise;
}
