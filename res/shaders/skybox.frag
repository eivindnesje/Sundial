#version 430 core
out vec4 FragColor;
in vec3 TexCoords;

uniform samplerCube skybox;
uniform vec3 sunDir;


float rand(vec3 co)
{
    return fract(sin(dot(co, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

void main()
{
    vec4 baseColor = texture(skybox, TexCoords);

    float t = smoothstep(-0.1, 0.4, sunDir.y);
    // Full brightness at day and 40% brightness at night.
    float skyBrightness = mix(0.4, 1.0, t);
    vec4 modulatedBase = baseColor * skyBrightness;

    float d = dot(normalize(TexCoords), normalize(sunDir));

    // Makes a solid sun with blurred edges
    float solidThresh = cos(radians(5.0));
    float blurThresh = cos(radians(10.0));
    float discBlend = smoothstep(blurThresh, solidThresh, d);
    float noise = rand(TexCoords * 10.0);
    float finalBlend = discBlend * (1.0 + 0.05 * (noise - 0.5));

    // At night make the sun invisible
    float discVisibility = smoothstep(-0.1, 0.1, sunDir.y);
    finalBlend *= discVisibility;

    // At sunset and sunrise, the color is more red
    vec3 sunColorDay = vec3(0.95, 0.76, 0.54);
    vec3 sunColorLate = vec3(0.95, 0.61, 0.16);
    vec3 sunColor = mix(sunColorLate, sunColorDay, t);
    
    vec4 sunColorFinal = vec4(sunColor, 1.0);

    // Blend the sky with the sun disc color using the computed blend factor
    vec4 finalColor = mix(modulatedBase, sunColorFinal, finalBlend);

    FragColor = finalColor;
}
