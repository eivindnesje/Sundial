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
    // --- Base Sky and Overall Dimming ---
    vec4 baseColor = texture(skybox, TexCoords);

    // t is the interpolation for sunsetting and rising, 1 at day and 0 at night
    float t = smoothstep(-0.1, 0.4, sunDir.y);
    // Darken the sky at night: full brightness at day and 40% brightness at night.
    float skyBrightness = mix(0.4, 1.0, t);
    vec4 modulatedBase = baseColor * skyBrightness;

    // --- Sun Disc Angular Blend ---
    // Calculate the cosine of the angle between this fragment direction and the sun direction.
    float d = dot(normalize(TexCoords), normalize(sunDir));
    // Define thresholds (in cosine-space):
    // Fragments with d >= cos(5°) are in the solid core.
    // Fragments with d <= cos(10°) show no sun.
    float solidThresh = cos(radians(5.0));
    float blurThresh  = cos(radians(10.0));
    // Use smoothstep to produce a smooth, narrow transition.
    float discBlend = smoothstep(blurThresh, solidThresh, d);
    // Add a touch of noise for a slight organic feel.
    float noise = rand(TexCoords * 10.0);
    float finalBlend = discBlend * (1.0 + 0.05 * (noise - 0.5));

    // --- Prevent Sun Disc from Blending at Night ---
    // Compute a factor to ensure that the sun disc does not appear when sun is low.
    // For example, if sunDir.y is below 0.0 (or near 0), we set discVisibility to 0.
    float discVisibility = smoothstep(-0.1, 0.1, sunDir.y);
    finalBlend *= discVisibility;

    // --- Dynamic Sun Color Transition ---
    // Define color stops for the sun disc.
    // • At day (t = 1): bright yellow.
    // • Around midday-to-dusk (t ~ 0.5): an orange hue.
    // • At night (t = 0): black.
    vec3 sunColorDay   = vec3(0.95, 0.76, 0.54); // Bright yellow
    vec3 sunColorMid   = vec3(0.95, 0.61, 0.16); // Orange-ish
    vec3 sunColor = mix(sunColorMid, sunColorDay, t);
    
    vec4 sunColorFinal = vec4(sunColor, 1.0);

    // --- Final Composition ---
    // Blend the darkened sky with the sun disc color using the computed blend factor.
    vec4 finalColor = mix(modulatedBase, sunColorFinal, finalBlend);

    FragColor = finalColor;
}
