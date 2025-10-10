#version 450 core

in vec3 fragPos;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragTangent;
in vec3 fragBitangent;

out vec4 FragColor;

// PBR Textures
uniform sampler2D diffuseTexture;           // Base color texture
uniform sampler2D normalTexture;            // Normal map
uniform sampler2D metallicRoughnessTexture; // Metallic (B) and Roughness (G) in one texture
uniform sampler2D emissiveTexture;          // Emissive texture
uniform sampler2D occlusionTexture;         // Ambient occlusion texture

// PBR Material properties
uniform vec4 materialColor;      // Base color factor (RGBA)
uniform vec3 emissiveFactor;     // Emissive color factor
uniform float metallicFactor;    // Metallic factor (0.0 = dielectric, 1.0 = metal)
uniform float roughnessFactor;   // Roughness factor (0.0 = smooth, 1.0 = rough)

// Texture usage flags
uniform int useDiffuseTexture;
uniform int useNormalTexture;
uniform int useMetallicRoughnessTexture;
uniform int useEmissiveTexture;
uniform int useOcclusionTexture;

// Alpha handling uniforms
uniform int alphaMode;      // 0=opaque, 1=mask, 2=blend
uniform float alphaCutoff;  // Alpha cutoff for mask mode

// Camera
uniform vec3 viewPos;

// Multiple lights support (max 8 lights)
#define MAX_LIGHTS 8
uniform int lightCount;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform float lightRanges[MAX_LIGHTS];

// Constants
const float PI = 3.14159265359;

// PBR Functions

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Geometry Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Function (Schlick approximation)
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    // Base color and alpha
    vec4 baseColor = materialColor;
    if (useDiffuseTexture == 1) {
        vec4 textureColor = texture(diffuseTexture, fragTexCoord);
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
        baseColor.a = 1.0;
    } else if (alphaMode == 2) {
        // BLEND mode - use alpha as-is for blending
        if (baseColor.a < 0.01) {
            discard;
        }
    }

    // Normal mapping
    vec3 N;
    if (useNormalTexture == 1) {
        N = texture(normalTexture, fragTexCoord).rgb;
        N = normalize(N * 2.0 - 1.0);
        mat3 TBN = mat3(fragTangent, fragBitangent, fragNormal);
        N = normalize(TBN * N);
    } else {
        N = normalize(fragNormal);
    }

    // Metallic and roughness
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (useMetallicRoughnessTexture == 1) {
        vec3 mrSample = texture(metallicRoughnessTexture, fragTexCoord).rgb;
        roughness *= mrSample.g;  // Green channel = roughness
        metallic *= mrSample.b;   // Blue channel = metallic
    }

    // Ambient occlusion
    float ao = 1.0;
    if (useOcclusionTexture == 1) {
        ao = texture(occlusionTexture, fragTexCoord).r;
    }

    // Emissive
    vec3 emissive = emissiveFactor;
    if (useEmissiveTexture == 1) {
        emissive *= texture(emissiveTexture, fragTexCoord).rgb;
    }

    // View direction
    vec3 V = normalize(viewPos - fragPos);

    // Calculate reflectance at normal incidence
    // For dielectrics use 0.04, for metals use the albedo color
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor.rgb, metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightCount && i < MAX_LIGHTS; i++) {
        vec3 lightPos = lightPositions[i];
        vec3 lightColor = lightColors[i];
        float lightIntensity = lightIntensities[i];
        float lightRange = lightRanges[i];

        // Calculate per-light radiance
        vec3 L = lightPos - fragPos;
        float distance = length(L);
        L = normalize(L);
        vec3 H = normalize(V + L);

        // Attenuation
        float attenuation = 1.0;
        if (lightRange > 0.0) {
            attenuation = max(0.0, 1.0 - (distance * distance) / (lightRange * lightRange));
            attenuation = attenuation * attenuation;
        }

        vec3 radiance = lightColor * lightIntensity * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic; // Metals have no diffuse

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        // Add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * baseColor.rgb / PI + specular) * radiance * NdotL;
    }

    // Ambient lighting (simplified)
    vec3 ambient = vec3(0.03) * baseColor.rgb * ao;

    // Final color
    vec3 color = ambient + Lo + emissive;

    // No manual gamma correction needed - GL_FRAMEBUFFER_SRGB handles linear-to-sRGB automatically
    // sRGB textures are automatically converted to linear on read, we do PBR in linear space,
    // and the sRGB framebuffer converts back to sRGB for display

    FragColor = vec4(color, baseColor.a);
}
