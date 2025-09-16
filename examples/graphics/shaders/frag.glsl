#version 450 core
out vec4 fragColor;

in vec3 fragPos;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragTangent;
in float fragHandedness;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 color;
uniform float materialAlpha;
uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform bool hasNormalMap;

void main()
{
    vec4 texColor = texture(diffuseTexture, fragTexCoord);
    vec3 normal = normalize(fragNormal);

    // Normal mapping using pre-calculated tangent space
    if (hasNormalMap) {
        vec3 normalMap = texture(normalTexture, fragTexCoord).rgb;
        normalMap = normalize(normalMap * 2.0 - 1.0);

        // Use pre-calculated tangent and compute bitangent with handedness
        vec3 T = normalize(fragTangent);
        vec3 N = normal;
        vec3 B = normalize(cross(N, T) * fragHandedness);

        mat3 TBN = mat3(T, B, N);
        normal = normalize(TBN * normalMap);
    }

    // Lighting calculations
    vec3 ambient = 0.1 * lightColor;
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = lightColor * spec * 0.2;

    vec3 lighting = ambient + diffuse + specular;
    float finalAlpha = texColor.a * materialAlpha;
    fragColor = vec4(texColor.rgb * lighting * color, finalAlpha);
}
