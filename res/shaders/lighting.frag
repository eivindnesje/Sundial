#version 430 core

in vec3 normal_out;
in vec2 textureCoordinates;
in vec3 fragPos;

uniform vec3 sunDir;      // NEW: Sun direction computed in our app.
uniform vec3 sunColor;    // NEW: Sun color.
uniform vec3 cameraPos;

uniform sampler2D diffuseTexture;
uniform bool useTexture;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

out vec4 color;

void main()
{
    vec3 normal = normalize(normal_out);
    vec3 ambientLight = vec3(0.1);
    float shininess = 32.0;
    vec3 emission = vec3(0.0);
    
    // Use the new sun direction.
    vec3 lightDir = normalize(sunDir);
    float diffuseIntensity = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = sunColor * diffuseIntensity;
    
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 viewDir = normalize(cameraPos - fragPos);
    float spec = pow(max(dot(reflectDir, viewDir), 0.0), shininess);
    vec3 specular = sunColor * spec;
    
    // Shadow mapping.
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float shadow = currentDepth - bias > closestDepth ? 0.5 : 1.0;
    
    vec3 litColor = ambientLight + (diffuse + specular) * shadow + emission;
    
    if (useTexture)
    {
        vec4 texColor = texture(diffuseTexture, textureCoordinates);
        litColor *= texColor.rgb;
    }
    
    // Optional: add a tiny dithering noise to smooth gradients.
    float noise = (fract(sin(dot(textureCoordinates, vec2(12.9898, 78.233))) * 43758.5453)*2.0 - 1.0)/256.0;
    litColor += noise;
    
    color = vec4(litColor, 1.0);
}
