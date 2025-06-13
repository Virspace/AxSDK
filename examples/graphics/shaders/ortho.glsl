#version 460 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec2 TexCoord;

out vec3 FragPos;
out vec3 FragNormal;
out vec2 FragTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // For orthographic projection, we don't need any special handling
    // The projection matrix already contains the orthographic transformation
    gl_Position = projection * view * model * vec4(Position, 1.0);

    // World space position for lighting calculations
    FragPos = vec3(model * vec4(Position, 1.0));

    // Transform normal to world space - using normal matrix to handle non-uniform scaling
    FragNormal = normalize(mat3(transpose(inverse(model))) * Normal);

    // Pass texture coordinates directly
    FragTexCoord = TexCoord;
}