#version 430 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 ShadowCoord;

uniform vec3 baseAmbient;   // Base ambient light (e.g., vec3(0.2))
uniform vec3 sunDir;        // Direction TO the sun (normalized; note light comes from -sunDir)
uniform vec3 sunColor;      // Sun light color (and intensity)
uniform vec3 moonDir;       // Direction TO the moon (normalized; for our system we set moonDir = -sunDir)
uniform vec3 moonColor;     // Moon light color (usually lower intensity)
uniform vec3 cameraPos;     // For specular calculations.

uniform sampler2D diffuseTexture;
uniform bool useTexture;

uniform sampler2D shadowMap; // Shadow map from the sun's perspective.
uniform float shininess;     // Specular exponent.

out vec4 FragColor;

//
// A simple shadow calculation using perspective division and bias.
//
float ShadowCalculation(vec4 shadowCoord, vec3 normal, vec3 lightDir) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    // Bias reduces shadow acne.
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    float shadow = currentDepth - bias > closestDepth ? 0.5 : 1.0;
    if(projCoords.z > 1.0)
        shadow = 1.0;
    return shadow;
}

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(cameraPos - FragPos);

    // Ambient term.
    vec3 ambient = baseAmbient;

    // Diffuse and specular for the sun.
    float diffSun = max(dot(norm, -sunDir), 0.0);
    vec3 reflectSun = reflect(sunDir, norm);
    float specSun = pow(max(dot(viewDir, reflectSun), 0.0), shininess);
    
    // Diffuse and specular for the moon.
    float diffMoon = max(dot(norm, -moonDir), 0.0);
    vec3 reflectMoon = reflect(moonDir, norm);
    float specMoon = pow(max(dot(viewDir, reflectMoon), 0.0), shininess);

    // Only the sun casts shadows.
    float shadow = ShadowCalculation(ShadowCoord, norm, -sunDir);

    // Combine diffuse and specular contributions.
    vec3 diffuse = sunColor * diffSun * shadow + moonColor * diffMoon;
    vec3 specular = (sunColor * specSun * shadow + moonColor * specMoon) * 0.2;
    
    vec3 lighting = ambient + diffuse + specular;

    vec3 objectColor = vec3(1.0);
    if(useTexture)
        objectColor = texture(diffuseTexture, TexCoords).rgb;
    
    FragColor = vec4(objectColor * lighting, 1.0);
}
