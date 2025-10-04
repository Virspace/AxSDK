#version 450 core

in vec3 fragPos;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragTangent;
in vec3 fragBitangent;

out vec4 FragColor;

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform vec4 materialColor;  // Now vec4 to include alpha
uniform vec3 viewPos;

uniform int useDiffuseTexture;
uniform int useNormalTexture;

// Alpha handling uniforms
uniform int alphaMode;      // 0=opaque, 1=mask, 2=blend
uniform float alphaCutoff;  // Alpha cutoff for mask mode

// Multiple lights support (max 8 lights)
#define MAX_LIGHTS 8
uniform int lightCount;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform float lightRanges[MAX_LIGHTS];

// Gamma correction functions
vec3 LinearToSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}

vec3 SRGBToLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));
}

void main()
{
    // Base color and alpha - texture or material
    vec4 baseColor = materialColor;
    if (useDiffuseTexture == 1) {
        vec4 textureColor = texture(diffuseTexture, fragTexCoord);

        // If texture is in sRGB space but we're working in linear, convert
        // textureColor.rgb = SRGBToLinear(textureColor.rgb);

        // According to GLTF spec: multiply texture color with material factor
        baseColor = materialColor * textureColor;
    }

    // Alpha handling based on alpha mode
    if (alphaMode == 0) {
        // OPAQUE mode - force alpha to 1.0
        baseColor.a = 1.0;
    } else if (alphaMode == 1) {
        // MASK mode - alpha testing with cutoff
        if (baseColor.a < alphaCutoff) {
            discard;
        }
        baseColor.a = 1.0; // Set to fully opaque after passing test
    } else if (alphaMode == 2) {
        // BLEND mode - use alpha as-is for blending
        // Early discard for completely transparent fragments
        if (baseColor.a < 0.01) {
            discard;
        }
    }

    // Normal mapping
    vec3 normal;
    if (useNormalTexture == 1) {
        // Sample normal from texture and transform to [-1, 1] range
        normal = texture(normalTexture, fragTexCoord).rgb;
        normal = normalize(normal * 2.0 - 1.0);

        // Transform normal from tangent space to world space using TBN matrix
        mat3 TBN = mat3(fragTangent, fragBitangent, fragNormal);
        normal = normalize(TBN * normal);
    } else {
        normal = normalize(fragNormal);
    }

    // View direction (used for specular on all lights)
    vec3 viewDir = normalize(viewPos - fragPos);

    // Accumulate lighting from all lights
    vec3 totalAmbient = vec3(0.0);
    vec3 totalDiffuse = vec3(0.0);
    vec3 totalSpecular = vec3(0.0);

    for (int i = 0; i < lightCount && i < MAX_LIGHTS; i++) {
        vec3 lightPos = lightPositions[i];
        vec3 lightColor = lightColors[i];
        float lightIntensity = lightIntensities[i];
        float lightRange = lightRanges[i];

        // Calculate light direction and distance
        vec3 lightDir = lightPos - fragPos;
        float distance = length(lightDir);
        lightDir = normalize(lightDir);

        // Calculate attenuation based on range (if range > 0)
        float attenuation = 1.0;
        if (lightRange > 0.0) {
            // Smooth attenuation that reaches zero at range
            attenuation = max(0.0, 1.0 - (distance * distance) / (lightRange * lightRange));
            attenuation = attenuation * attenuation; // Squared for smoother falloff
        }

        // Apply intensity and attenuation
        vec3 effectiveLight = lightColor * lightIntensity * attenuation;

        // Ambient (small contribution from each light)
        totalAmbient += 0.05 * effectiveLight;

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        totalDiffuse += diff * effectiveLight;

        // Specular
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        totalSpecular += spec * effectiveLight;
    }

    // Clamp ambient to reasonable range
    totalAmbient = min(totalAmbient, vec3(0.2));

    // Final color with alpha
    vec3 result = (totalAmbient + totalDiffuse + totalSpecular) * baseColor.rgb;
    FragColor = vec4(result, baseColor.a);
}