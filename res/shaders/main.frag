#version 430 core

in vec3 normal_out;
in vec2 textureCoordinates;
in vec3 fragPos;

uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 cameraPos;

uniform sampler2D diffuseTexture;
uniform bool useTexture;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

out vec4 color;

void main()
{
    // Normalize the interpolated normal
    vec3 normal = normalize(normal_out);
    
    // Set up basic material parameters
    vec3 ambient = vec3(0.2);
    float shininess = 32.0;

    // Diffuse component (Lambertian)
    float diff = max(dot(normal, sunDir), 0.0);
    vec3 diffuse = sunColor * diff;

    // Specular component (Phong reflection)
    vec3 viewDir = normalize(cameraPos - fragPos);
    vec3 reflectDir = reflect(-sunDir, normal);
    float spec = 0.0;
    if (diff > 0.0)
    {
        spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    }
    vec3 specular = sunColor * spec;

    // Shadow mapping:
    // Transform fragment position into light space.
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Remap from [-1,1] to [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // Retrieve the depth of the closest surface from the shadow map
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    
    // Compute bias to avoid shadow acne. Adjust these parameters as needed.
    float bias = max(0.005 * (1.0 - dot(normal, sunDir)), 0.001);
    
    // Hard comparison: if the current depth (minus bias) is greater than the stored depth, the fragment is in shadow.
    float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;

    float sunIntensity = smoothstep(-0.1, 0.3, sunDir.y);

    // Combine the lighting components using the shadow factor
    vec3 lighting = ambient + (diffuse + specular) * shadow * sunIntensity;
 
    // If a diffuse texture is used, modulate the lighting result
    if (useTexture)
    {
        vec4 texColor = texture(diffuseTexture, textureCoordinates);
        lighting *= texColor.rgb;
    }
    
    color = vec4(lighting, 1.0);
}
