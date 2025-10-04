#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in vec4 tangent;

out vec3 fragPos;
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragTangent;
out vec3 fragBitangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    fragPos = vec3(model * vec4(position, 1.0));
    fragTexCoord = texCoord;

    // Transform to world space
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    fragNormal = normalize(normalMatrix * normal);
    fragTangent = normalize(normalMatrix * tangent.xyz);
    fragBitangent = normalize(cross(fragNormal, fragTangent) * tangent.w);

    gl_Position = projection * view * model * vec4(position, 1.0);
}