#version 330 core

// Fragment shader for Blinn-Phong lighting
// Thanks Claude code
// C = (ka*Ia + kd*max(N.L, 0)) * Il + ks * Il * max(N.H, 0)^shininess

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;    // light position in world space
uniform vec3 viewPos;     // camera position in world space
uniform vec3 lightColor;  // Il: light color/intensity
uniform vec3 Ia;          // intensity of ambient light
uniform float ka;         // ambient reflectance
uniform vec3 kd;          // diffuse reflectance (model color)
uniform float ks;         // specular reflectance
uniform float shininess;  // alpha: strength of specular reflection

void main() {
    // interpolation changes the length, so normalize again
    vec3 N = normalize(Normal);
    // towards the light
    vec3 L = normalize(lightPos - FragPos);
    // towards the camera
    vec3 V = normalize(viewPos - FragPos);
    // half-vector between light and view directions (the Blinn part)
    vec3 H = normalize(L + V);

    // Ambient + Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 ambientDiffuse = (ka * Ia + kd * diff) * lightColor;

    // Specular
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = ks * lightColor * spec;

    // Combine all lighting components
    vec3 result = ambientDiffuse + specular;
    FragColor = vec4(result, 1.0);
}
