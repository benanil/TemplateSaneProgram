
#ifdef __ANDROID__
    #define MEDIUMP_FLT_MAX    65504.0
    #define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#else
    #define saturateMediump(x) x
#endif

// #ifdef __ANDROID__
//     #define float16 mediump float
//     #define half2   mediump vec2
//     #define half3   mediump vec3
// #else
#define float16 float
#define half2   vec2
#define half3   vec3
// #endif

layout(location = 0) out vec4 oFragColor; // TextureType_RGB8

in vec2 texCoord;

uniform sampler2D uAlbedoTex;
uniform sampler2D uShadowMetallicRoughnessTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthMap;
uniform sampler2D aoTex; // < ambient occlusion

uniform highp   vec3 viewPos;
uniform mediump vec3 sunDir;

uniform highp   mat4 uInvView;
uniform highp   mat4 uInvProj;

const float gamma = 2.2;
const float PI = 3.1415926535;

vec4 toLinear(vec4 sRGB)
{
    return sRGB * sRGB; // < gamma 2.2
    bvec4 cutoff = lessThan(sRGB, vec4(0.04045));
    vec4 higher = pow((sRGB + vec4(0.055))/vec4(1.055), vec4(2.4));
    vec4 lower = sRGB/vec4(12.92);
    return mix(higher, lower, cutoff);
}

// https://google.github.io/filament/Filament.html
float16 D_GGX(float16 roughness, float16 NoH, const half3 n, const half3 h)
{
    #if defined(__ANDROID__)
    vec3 NxH = cross(n, h);
    float oneMinusNoHSquared = dot(NxH, NxH);
    #else
    float oneMinusNoHSquared = 1.0 - NoH * NoH;
    #endif
    float16 a = NoH * roughness;
    float16 k = roughness / (oneMinusNoHSquared + a * a);
    float16 d = k * k * (1.0 / 3.1415926535);
    return saturateMediump(d);
}

// visibility
float16 V_SmithGGXCorrelatedFast(float16 NoV, float16 NoL, float16 roughness) {
    return 0.5 / mix(2.0 * NoL * NoV, NoL + NoV, roughness);
}

float V_Neubelt(float NoV, float NoL) {
    // Neubelt and Pettineo 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"
    return saturateMediump(1.0 / (4.0 * (NoL + NoV - NoL * NoV)));
}

float16 F_Schlick(float16 u, float16 f0) {
    float16 x = 1.0 - u;
    float16 f = x * x * x * x * x;
    return f + f0 * (1.0 - f);
}

float16 F_Schlick2(float16 u, float16 f0, float16 f90) {
    // float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float16 x = 1.0 - u;
    return f0 + (f90 - f0) * (x * x * x * x * x);
}

half3 StandardBRDF(half3 color, half3 l, half3 v, half3 n, float16 roughness, float16 metallic, float16 shadow)
{
    half3 h = normalize(v + l);

    float16 NoV = abs(dot(n, v)) + 1e-5;
    float16 NoL = clamp(dot(n, l), 0.0, 1.0);
    float16 NoH = clamp(dot(n, h), 0.0, 1.0);
    float16 LoH = clamp(dot(l, h), 0.0, 1.0);
    float16 D = D_GGX(roughness, NoH, n, h);
    
    float lum = dot(color, vec3(0.3, 0.59, 0.11));
    float16 f0 = mix(0.020, lum, metallic * 0.20);
    float16 shadow3 = shadow * shadow * shadow;

    float16 F = F_Schlick(LoH, f0); // F_Schlick2(LoH, 0.4, f90);
    float16 V = V_Neubelt(NoV, NoL); //V_SmithGGXCorrelatedFast(NoV, NoL, roughness);// 
    float16 lightIntensity = 1.0;
    // // specular BRDF
    float16 Fr = (D * V) * F * shadow3 * 0.65; 
    // diffuse BRDF
    color *= lightIntensity;
    float16 darkness = max(NoL * shadow3, 0.05);
    half3 ambient = color * (1.0-darkness) * 0.05;
    half3 Fd = (color / 3.1415926535) * darkness;

    return  (Fd + Fr) + ambient;
}

vec3 GetViewRay()
{
    vec2 uv = texCoord * 2.0 - 1.0;
    vec4 target = uInvProj * vec4(uv, 1.0, 1.0);
    target /= target.w;
    vec3 rayDir = normalize(vec3(uInvView * target));
    return rayDir;
}

vec3 WorldSpacePosFromDepthBuffer()
{
    float depth = texture(uDepthMap, texCoord).r;
    vec2 uv = texCoord * 2.0 - 1.0;
    vec4 vsPos = uInvProj * vec4(uv, depth * 2.0 - 1.0, 1.0);
    vsPos /= vsPos.w;
    vsPos = uInvView * vsPos;
    return vsPos.xyz ;
}

vec3 GammaCorrect(vec3 x) {
    return sqrt(x); // pow(x, vec3(1.0 / gamma)); // with sqrt gamma is 2.0
}

// https://www.shadertoy.com/view/WdjSW3
vec3 CustomToneMapping(vec3 x)
{
    return GammaCorrect(x / (1.0 + x)); // reinhard
#ifdef __ANDROID__
    return x / (x + 0.155) * 1.019; // < doesn't require gamma correction
#else
    //return  x / (x + 0.0832) * 0.982;
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return GammaCorrect((x * (a * x + b)) / (x * (c * x + d) + e));
#endif
}

// https://www.shadertoy.com/view/lsKSWR
float Vignette(vec2 uv)
{
	uv *= vec2(1.0f) - uv.yx;   // vec2(1.0)- uv.yx; -> 1.-u.yx; Thanks FabriceNeyret !
	float vig = uv.x * uv.y * 15.0f; // multiply with sth for intensity
	vig = pow(vig, 0.15f); // change pow for modifying the extend of the  vignette
	return vig; 
}

#ifdef __ANDROID__
// gaussian blur
float Blur5(float a, float b, float c, float d, float e) 
{
    const float Weights5[3] = float[3](6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f);
    return Weights5[0]*a + Weights5[1]*(b+c) + Weights5[2]*(d+e);
}
#endif

float GetAO(float shadow)
{
    float ao = 0.0;
    #ifndef __ANDROID__
    {
        ao = Blur5(texture(aoTex, texCoord).r,
                   textureOffset(aoTex, texCoord, ivec2(-1, 0)).r,
                   textureOffset(aoTex, texCoord, ivec2( 1, 0)).r,
                   textureOffset(aoTex, texCoord, ivec2(-2, 0)).r,
                   textureOffset(aoTex, texCoord, ivec2( 2, 0)).r);
    }
    #else
    {
        // 9x gaussian blur
        ao += textureOffset(aoTex, texCoord, ivec2(-4.0, 0)).r * 0.05;
        ao += textureOffset(aoTex, texCoord, ivec2(-3.0, 0)).r * 0.09;
        ao += textureOffset(aoTex, texCoord, ivec2(-2.0, 0)).r * 0.12;
        ao += textureOffset(aoTex, texCoord, ivec2(-1.0, 0)).r * 0.15;
        ao += textureOffset(aoTex, texCoord, ivec2(+0.0, 0)).r * 0.16;
        ao += textureOffset(aoTex, texCoord, ivec2(+1.0, 0)).r * 0.15;
        ao += textureOffset(aoTex, texCoord, ivec2(+2.0, 0)).r * 0.12;
        ao += textureOffset(aoTex, texCoord, ivec2(+3.0, 0)).r * 0.09;
        ao += textureOffset(aoTex, texCoord, ivec2(+4.0, 0)).r * 0.05;
    }
    #endif
    // we want ao only if there is shadow 
    float invAO = 1.0 - ao;
    invAO *= step(shadow, 0.50);
    return 1.0 - invAO;
}

void main()
{
    vec3  shadMetRough = texture(uShadowMetallicRoughnessTex, texCoord).rgb;
    vec3  normal = texture(uNormalTex, texCoord).rgb * 2.0 - 1.0;
    vec3  albedo = toLinear(texture(uAlbedoTex, texCoord)).rgb;
    normal = normalize(normal);
    float shadow = shadMetRough.x;
    vec3 viewRay = GetViewRay();
    // vec3 pos = WorldSpacePosFromDepthBuffer(texCoord);
    vec3 v = -viewRay;// -normalize(WorldSpacePosFromDepthBuffer()); // normalize(viewPos - pos);

    vec3 lighting = StandardBRDF(albedo, sunDir, v, normal, shadMetRough.y, shadMetRough.z, shadow);
    vec3 pbr = lighting * shadow;
 
    oFragColor = CustomToneMapping(pbr).xyzz * GetAO(shadow) * Vignette(texCoord);
}