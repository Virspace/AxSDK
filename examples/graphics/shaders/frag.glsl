#version 460 core
out vec4 FragColor;

in vec3 FragPos;     // Input fragment position
in vec3 FragNormal;  // Input normal from vertex shader
in vec2 FragTexCoord; // Input texture coordinates

uniform vec3 LightPos;   // Light position in world space
uniform vec3 LightColor; // Color of the light
uniform vec3 color;      // Base color of the object
uniform sampler2D Texture; // Texture sampler

void main()
{
    // ambient
    float AmbientStrength = 0.1;
    vec3 Ambient = AmbientStrength * LightColor;

    // diffuse
    vec3 Norm = normalize(FragNormal);
    vec3 LightDir = normalize(LightPos - FragPos);
    float Diff = max(dot(Norm, LightDir), 0.0);
    vec3 Diffuse = Diff * LightColor;

    vec4 TexColor = texture(Texture, FragTexCoord);
    vec3 Result = (Ambient + Diffuse) * color;
    FragColor = TexColor * vec4(Result, 1.0);
}