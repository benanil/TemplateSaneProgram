
out vec4 FragColor;

in highp   vec3 vFragPos;
in highp   vec2 vTexCoords;
in highp   vec4 vLightSpaceFrag;
in mediump mat3 vTBN;

uniform sampler2D albedo;
uniform sampler2D normalMap;

uniform sampler2D metallicRoughnessMap;
uniform sampler2D shadowMap;

uniform vec3 viewPos;
uniform vec3 sunDir;

uniform int hasNormalMap;

const float PI = 3.1415926535;
const float gamma = 2.2;

vec4 toLinear(vec4 sRGB)
{
    bvec4 cutoff = lessThan(sRGB, vec4(0.04045));
    vec4 higher = pow((sRGB + vec4(0.055))/vec4(1.055), vec4(2.4));
    vec4 lower = sRGB/vec4(12.92);
    return mix(higher, lower, cutoff);
}

// combination of unreal and Reinhard
// https://www.shadertoy.com/view/lslGzl
// https://www.shadertoy.com/view/WdjSW3
vec3 CustomToneMapping(vec3 color) // https://www.desmos.com/calculator/o0exqnpjzg
{
    color = (color / (0.35 + color));
    return pow(color, vec3(1.0 / gamma)) * 1.11;
    // x = x / (x + 0.0832) * 0.982; // no need gamma correction but darks are too dark
}

#ifndef __ANDROID__
// pbr code directly copied from here
// https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/1.2.lighting_textured/1.2.pbr.fs
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 BRDF(vec3 N, vec3 color, float metallic, float roughness)
{
    vec3 V = normalize(viewPos - vFragPos);
    vec3 L = sunDir;

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, color, metallic);
    vec3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(NdotV, NdotL, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;
    specular *= 1.60;
    // kS is equal to Fresnel
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 lightColor = vec3(0.98, 0.91, 0.88);
    const float intensity = 2.2;
    vec3 radiance = lightColor * intensity;

    vec3 lighting = (kD * color / PI + specular) * radiance * NdotL;
    vec3 ambient = 0.02 * color * (1.0 - NdotL);

    lighting += ambient;
    lighting = CustomToneMapping(lighting);
    return lighting;
}

#endif // __ANDROID__ // android don't have brdf

float ShadowCalculation()
{
    const float bias = 0.00177;
    // perform perspective divide
    vec3 projCoords = vLightSpaceFrag.xyz / vLightSpaceFrag.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float shadow = 0.0;
    // get depth of current fragment from light's perspective
    shadow += currentDepth - bias > closestDepth  ? .40: 1.0;
    closestDepth = texture(shadowMap, projCoords.xy + texelSize).r;
    shadow += currentDepth - bias > closestDepth  ? .40: 1.0;
    closestDepth = texture(shadowMap, projCoords.xy - texelSize).r;
    shadow += currentDepth - bias > closestDepth  ? .40: 1.0;

    projCoords.x += texelSize.x;
    closestDepth = texture(shadowMap, projCoords.xy).r;
    shadow += currentDepth - bias > closestDepth  ? .40: 1.0;

    projCoords.x = texelSize.x * 2.0;
    closestDepth = texture(shadowMap, projCoords.xy - texelSize).r;
    shadow += currentDepth - bias > closestDepth  ? .40: 1.0;
    // float realBias = bias; // max((bias * 10) * (1.0 - dot(vTBN[2], sunDir)), bias); // vTBN[2] = normal
    //
    // for(int x = -1; x <= 1; ++x)
    // {
    //     for(int y = -1; y <= 1; ++y)
    //     {
    //         float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
    //         shadow += currentDepth - realBias > pcfDepth ? .44 : 1;
    //     }
    // }
    shadow /= 5.0;
    return shadow;
}

void main()
{
    // get diffuse color
    vec4 color = toLinear(texture(albedo, vTexCoords));
    #if ALPHA_CUTOFF
    if (color.a < 0.001)
    discard;
    #endif

    vec3 normal = vTBN[2];
    vec3 lighting = vec3(0.0);
    #ifndef __ANDROID__
    if (hasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        vec2  c = texture(normalMap, vTexCoords).rg * 2.0 - 1.0;
        float z = sqrt(1.0 - c.x * c.x - c.y * c.y);
        c.y = 1.0 * c.y;
        const float bumpiness = 1.2f;
        normal  = normalize(vec3(c * bumpiness, z));
        // transform normal vector to range [-1,1]
        normal  = normalize(vTBN * normal);  // this normal is in tangent space

        vec2 metalRoughness = texture(metallicRoughnessMap, vTexCoords).rg;
        float metallic  = metalRoughness.r;
        float roughness = metalRoughness.g;

        lighting = BRDF(normal, color.rgb, metallic, roughness);
    }
    #else
    {
        float ndl = vTBN[0].x;
        vec3 sunColor = vec3(0.98, 0.92, 0.89);
        vec3 diffuse  = color.rgb * ndl * sunColor;
        vec3 specular = vec3(vTBN[0].y * 0.13);
        vec3 ambient = color.rgb * (1.0 - ndl) * 0.05;
        lighting = diffuse + specular + ambient;
        lighting = CustomToneMapping(lighting);
    }
    #endif
    lighting *= ShadowCalculation();
    FragColor = vec4(lighting, 1.0);
}
