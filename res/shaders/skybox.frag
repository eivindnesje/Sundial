#version 430 core
out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube skybox;

// NEW: Uniform for the sun direction
uniform vec3 sunDir;

void main()
{
    // Sample the static cubemap texture.
    vec4 baseColor = texture(skybox, TexCoords);
    
    // Compute how close the current fragment direction is to the sun.
    // We use dot product between normalized vectors.
    float dotVal = dot(normalize(TexCoords), normalize(sunDir));
    
    // Create a sun disc effect using smoothstep to get a soft edge.
    float sunDisc = smoothstep(0.995, 1.0, dotVal);
    
    // Define a bright (golden) sun color.
    vec3 sunColor = vec3(1.0, 0.9, 0.6);
    
    // Mix the base color with the sun color.
    vec4 finalColor = mix(baseColor, vec4(sunColor, 1.0), sunDisc);
    
    // Optionally: Modulate overall brightness based on the sun's vertical position.
    // This creates a simple day–night dimming effect.
    float brightness = clamp((sunDir.y + 1.0) / 2.0, 0.2, 1.0);
    finalColor.rgb *= brightness;
    
    FragColor = finalColor;
}
