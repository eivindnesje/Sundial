#version 430 core

in vec3 vPos;
out vec4 FragColor;

uniform vec3 sunDir;       // Direction to the sun.
uniform vec3 moonDir;      // Direction to the moon.
uniform float dayFactor;   // 1.0 = full day, 0.0 = full night.
uniform float skyboxIntensity; // Overall intensity (e.g., 0.5)

void main() {
    vec3 dir = normalize(vPos);
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    
    // Define gradient colors.
    vec3 dayTop = vec3(0.2, 0.5, 0.9);
    vec3 dayHorizon = vec3(0.8, 0.6, 0.3);
    vec3 nightTop = vec3(0.02, 0.02, 0.1);
    vec3 nightHorizon = vec3(0.1, 0.1, 0.2);
    
    vec3 dayColor = mix(dayHorizon, dayTop, t);
    vec3 nightColor = mix(nightHorizon, nightTop, t);
    vec3 baseColor = mix(nightColor, dayColor, dayFactor);
    
    // Sun glow – appears when looking toward the sun.
    float sunGlow = smoothstep(0.995, 0.98, dot(dir, sunDir)) * dayFactor;
    vec3 sunDisc = vec3(1.0, 0.9, 0.7) * sunGlow;
    
    // Moon glow – appears when the day is fading.
    float moonGlow = smoothstep(0.995, 0.98, dot(dir, moonDir)) * (1.0 - dayFactor);
    vec3 moonDisc = vec3(0.9, 0.9, 1.0) * moonGlow;
    
    vec3 finalColor = (baseColor + sunDisc + moonDisc) * skyboxIntensity;
    FragColor = vec4(finalColor, 1.0);
}
