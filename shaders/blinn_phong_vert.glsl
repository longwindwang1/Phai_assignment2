#version 330 core

// Thanks Claude code
// Vertex shader for Blinn-Phong lighting
// moves each vertex to world space for lighting and to clip space for drawing

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos; // world space position
out vec3 Normal;  // world space normal

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    // normal matrix keeps normals perpendicular to the surface under non-uniform scale
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
