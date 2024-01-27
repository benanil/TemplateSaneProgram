#ifdef __ANDROID__
    #define MEDIUMP_FLT_MAX    65504.0
    #define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#else
    #define saturateMediump(x) x
#endif

#define MakeVec3(x) vec3(x)

#define float16 mediump float
#define half2   mediump vec2
#define half3   mediump vec3

precision mediump sampler2DShadow;

out vec4 FragColor;

in highp   vec3 vFragPos;
in highp   vec2 vTexCoords;
in highp   vec4 vLightSpaceFrag;
in mediump mat3 vTBN;

uniform sampler2D albedo;
uniform sampler2D normalMap;

uniform sampler2D metallicRoughnessMap;
uniform sampler2DShadow shadowMap;

uniform highp   vec3 viewPos;
uniform mediump vec3 sunDir;

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

float sq(float x) {
    return x * x;
}

vec3 GammaCorrect(vec3 x)
{
    return pow(x, vec3(1.0 / gamma));
}

// https://www.shadertoy.com/view/WdjSW3
vec3 CustomToneMapping(vec3 x)
{
#ifdef __ANDROID__
    return GammaCorrect(x / (1.0 + x)); // reinhard
    return x / (x + 0.155) * 1.019; // < doesn't require gamma correction
#else
    //return GammaCorrect(x / (1.0 + x)); // reinhard
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return GammaCorrect((x * (a * x + b)) / (x * (c * x + d) + e));
#endif
}

// https://google.github.io/filament/Filament.html
float16 D_GGX(float16 roughness, float16 NoH, const half3 n, const half3 h) 
{
    half3 NxH = cross(n, h);
    float16 oneMinusNoHSquared = dot(NxH, NxH);
    float16 a = NoH * roughness;
    float16 k = roughness / (oneMinusNoHSquared + a * a);
    float16 d = k * k * (1.0 / 3.1415926535);
    return saturateMediump(d);
}

// visibility
float16 V_SmithGGXCorrelatedFast(float16 NoV, float16 NoL, float16 roughness) {
    return 0.5 / mix(2.0 * NoL * NoV, NoL + NoV, roughness);
}

half3 F_Schlick(float16 u, half3 f0) {
    float16 x = 1.0 - u;
    float16 f = x * x * x * x;
    return f + f0 * (1.0 - f);
}

half3 StandardBRDF(half3 color, half3 n, float16 roughness, float16 metallic, float16 shadow)
{
    half3 l = sunDir;
    half3 v = normalize(viewPos - vFragPos);
    half3 h = normalize(v + l);

    float16 NoV = abs(dot(n, v)) + 1e-5;
    float16 NoL = clamp(dot(n, l), 0.0, 1.0);
    float16 NoH = clamp(dot(n, h), 0.0, 1.0);
    float16 LoH = clamp(dot(l, h), 0.0, 1.0);
    float16 D = D_GGX(roughness, NoH, n, h);

    half3 f0 = mix(vec3(0.001), vec3(color), metallic * 0.4);

    half3   F = F_Schlick(LoH, f0);
    float16 V = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);
    float16 lightIntensity = 1.2;
    // // specular BRDF
    half3 Fr = (D * V) * F * step(0.401, shadow); // step because we don't want to have 0.4 shadow.
    // diffuse BRDF
    color *= lightIntensity;
    float16 darkness = NoL * (shadow * shadow);
    half3 ambient = color * (1.0-darkness) * 0.10;
    half3 Fd = (color / 3.1415926535) * darkness;

    return CustomToneMapping((Fd + Fr) + ambient);
}

float ShadowLookup(vec4 loc, vec2 offset)
{
    const vec2 texmapscale = vec2(0.0007, 0.0007);
    return textureProj(shadowMap, vec4(loc.xy + offset * texmapscale * loc.w, loc.z, loc.w));
}

// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
float ShadowCalculation()
{
    const vec4 minShadow = vec4(0.40);
#ifdef __ANDROID__
    vec2 offset;
    const vec3 mixer = vec3(1.0, 1.1, 1.2);
    offset.x = fract(dot(vFragPos.xyz * 0.33, mixer)) * 0.5;
    offset.y = fract(dot(vFragPos.yzx * 0.33, mixer)) * 0.5;

    vec4 shadow = vec4(ShadowLookup(vLightSpaceFrag, offset + vec2(-0.50,  0.25)),
                       ShadowLookup(vLightSpaceFrag, offset + vec2( 0.25,  0.25)),
                       ShadowLookup(vLightSpaceFrag, offset + vec2(-0.50, -0.50)),
                       ShadowLookup(vLightSpaceFrag, offset + vec2( 0.25, -0.50)));

    return dot(max(shadow, minShadow), vec4(1.0)) * (0.25);  // max is used with 4 elements maybe it helps to make this simd
#else
    vec4 result = vec4(0.0); // store 4x4 shadow results to avoid dependency chains
    float y = -1.5;
    for (int i = 0; i < 4; i++, y += 1.0)
    {
        vec4 shadow = vec4(ShadowLookup(vLightSpaceFrag, vec2(-1.5, y)),
                           ShadowLookup(vLightSpaceFrag, vec2(-0.5, y)),
                           ShadowLookup(vLightSpaceFrag, vec2(+0.5, y)),
                           ShadowLookup(vLightSpaceFrag, vec2(+1.5, y)));

        // horizontal sum. max is used with 4 elements maybe it helps to make this simd
        result[i] = dot(max(shadow, minShadow), vec4(1.0));
    }
    return dot(result, vec4(1.0)) / 16.0;
#endif
}

void main()
{
    // get diffuse color
    mediump vec4 color = toLinear(texture(albedo, vTexCoords));
#if ALPHA_CUTOFF
    if (color.a < 0.001)
        discard;
#endif

    half3 normal   = vTBN[2];
    half3 lighting = vec3(0.0);

    float shadow = ShadowCalculation();

    float16 metallic  = 0.5;
    float16 roughness = 0.3;
#ifndef __ANDROID__
    if (hasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        half2   c = texture(normalMap, vTexCoords).rg * 2.0 - 1.0;
        float16 z = sqrt(1.0 - c.x * c.x - c.y * c.y);
        normal  = normalize(vec3(c, z));
        // transform normal vector to range [-1,1]
        normal  = normalize(vTBN * normal);  // this normal is in tangent space

        half2 metalRoughness = texture(metallicRoughnessMap, vTexCoords).rg;
        metallic  = metalRoughness.r;
        roughness = metalRoughness.g;
    }
#endif
    lighting = StandardBRDF(color.rgb, normal, metallic, roughness, shadow);
    FragColor = vec4(lighting * shadow, 1.0);
}
