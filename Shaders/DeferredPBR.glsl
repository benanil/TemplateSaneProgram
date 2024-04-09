
#ifdef __ANDROID__
    #define MEDIUMP_FLT_MAX    65504.0
    #define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#else
    #define saturateMediump(x) x
#endif

#ifdef __ANDROID__
    #define float16 mediump float
    #define half2   mediump vec2
    #define half3   mediump vec3
#else
    #define float16 float
    #define half2   vec2
    #define half3   vec3
#endif

layout(location = 0) out vec4 oFragColor; // TextureType_RGB8

in vec2 texCoord;

struct LightInstance
{
    vec3 position;
    mediump vec3 direction;
    highp   uint color;
    mediump float intensity;
    mediump float cutoff; // < zero if this is point light
    mediump float range; 
};

uniform LightInstance uPointLights[16];
uniform LightInstance uSpotLights[16];
uniform int uNumSpotLights;
uniform int uNumPointLights;

uniform mediump sampler2D uAlbedoTex;
uniform mediump sampler2D uShadowMetallicRoughnessTex;
uniform mediump sampler2D uNormalTex;
uniform highp   sampler2D uDepthMap;
uniform lowp    sampler2D uAmbientOclussionTex; // < ambient occlusion

uniform mediump vec3 uSunDir;

uniform highp mat4 uInvView;
uniform highp mat4 uInvProj;

const float gamma = 2.2;
const float PI = 3.1415926535;

mediump vec4 toLinear(mediump vec4 sRGB)
{
    // https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return sRGB * (sRGB * (sRGB * 0.305306011 + 0.682171111) + 0.012522878);
}

float sqr(float x) { return x*x; }

// https://google.github.io/filament/Filament.html
float16 D_GGX(float16 roughness, float16 NoH, const half3 n, const half3 h)
{
    #if defined(__ANDROID__)
    half3 NxH = cross(n, h);
    float16 oneMinusNoHSquared = dot(NxH, NxH);
    #else
    float16 oneMinusNoHSquared = 1.0 - NoH * NoH;
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

float16 V_Neubelt(float16 NoV, float16 NoL) {
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
    
    float16 lum = dot(color, vec3(0.3, 0.59, 0.11));
    float16 f0 = mix(0.0075, lum, metallic * 0.15);
    float16 shadow4 = shadow * shadow;
    shadow4 *= shadow4;
    float16 F = F_Schlick(LoH, f0); // F_Schlick2(LoH, 0.4, f90);
    float16 V = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);// V_Neubelt(NoV, NoL); //
    float16 lightIntensity = 1.0;
    // // specular BRDF
    float16 Fr = (D * V) * F * shadow4 * 0.20;
    // diffuse BRDF
    color *= lightIntensity;
    float16 darkness = max(NoL * shadow4, 0.05);
    half3 ambient = color * (1.0-darkness) * 0.05;
    half3 Fd = (color / 3.1415926535) * darkness;

    return  (Fd + Fr) + ambient;
}

float16 Reflection(half3 l, half3 n, half3 v)
{
    half3 h = normalize(v + l);
    float16 ldh = max(dot(l, h), 0.0);
    half3  rl = reflect(-l, n);
    float16 s = dot(rl, v);
    s = ldh * ldh * s;
    return s * s;
}

half3 Lighting(half3 albedo, half3 l, half3 n, half3 v)
{
    float16 ndl = max(dot(l, n), 0.2);
    float16 r = Reflection(l, n, v);
    return albedo * ndl + (r * 0.08);
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

vec3 GetViewRay(vec3 viewPos, vec3 worldSpacePos)
{
    // calculate direction between WorldSpacePos and 3rd row of invView(viewPos)
    vec3 dir = worldSpacePos - viewPos;
    return dir * inversesqrt(dot(dir, dir));
}

half3 GammaCorrect(half3 x) {
    return pow(x, vec3(1.0 / gamma)); //  sqrt(x); // with sqrt gamma is 2.0
}

// https://www.shadertoy.com/view/WdjSW3
half3 CustomToneMapping(half3 x)
{
    return GammaCorrect(x / (1.0 + x)); // reinhard
#ifdef __ANDROID__
    return x / (x + 0.155) * 1.019; // < doesn't require gamma correction
#else
    //return  x / (x + 0.0832) * 0.982;
    const float16 a = 2.51;
    const float16 b = 0.03;
    const float16 c = 2.43;
    const float16 d = 0.59;
    const float16 e = 0.14;
    return GammaCorrect((x * (a * x + b)) / (x * (c * x + d) + e));
#endif
}

// https://www.shadertoy.com/view/lsKSWR
float16 Vignette(half2 uv)
{
	uv *= vec2(1.0) - uv.yx;   // vec2(1.0)- uv.yx; -> 1.-u.yx; Thanks FabriceNeyret !
	float16 vig = uv.x * uv.y * 15.0; // multiply with sth for intensity
	vig = pow(vig, 0.15); // change pow for modifying the extend of the  vignette
	return vig; 
}

#ifdef __ANDROID__
// gaussian blur
lowp float Blur5(lowp float a, lowp float b, lowp float c, lowp float d, lowp float e) 
{
    const lowp float Weights5[3] = float[3](6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f);
    return Weights5[0]*a + Weights5[1]*(b+c) + Weights5[2]*(d+e);
}
#endif

lowp float GetAO(float shadow)
{
    lowp float ao = 0.0;
    #ifdef __ANDROID__
    {
        ao = Blur5(texture(uAmbientOclussionTex, texCoord).r,
             textureOffset(uAmbientOclussionTex, texCoord, ivec2(-1, 0)).r,
             textureOffset(uAmbientOclussionTex, texCoord, ivec2( 1, 0)).r,
             textureOffset(uAmbientOclussionTex, texCoord, ivec2(-2, 0)).r,
             textureOffset(uAmbientOclussionTex, texCoord, ivec2( 2, 0)).r);
    }
    #else
    {
        // 9x gaussian blur
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(-4, 0)).r * 0.05;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(-3, 0)).r * 0.09;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(-2, 0)).r * 0.12;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(-1, 0)).r * 0.15;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(+0, 0)).r * 0.16;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(+1, 0)).r * 0.15;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(+2, 0)).r * 0.12;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(+3, 0)).r * 0.09;
        ao += textureOffset(uAmbientOclussionTex, texCoord, ivec2(+4, 0)).r * 0.05;
    }
    #endif
    return ao;
}

void main()
{
    half3 shadMetRough = texture(uShadowMetallicRoughnessTex, texCoord).rgb;
    half3 normal = texture(uNormalTex, texCoord).rgb * 2.0 - 1.0;
    half3 albedo = toLinear(texture(uAlbedoTex, texCoord)).rgb;
    normal = normalize(normal);
    
    float16 shadow    = shadMetRough.x;
    float16 metallic  = shadMetRough.y;
    float16 roughness = shadMetRough.z * shadMetRough.z;

    vec3 pos = WorldSpacePosFromDepthBuffer();
    
    half3 viewRay = GetViewRay(uInvView[3].xyz, pos); // viewPos: uInvView[3].xyz
    half3 sunColor = vec3(0.98f, 0.94, 0.91);
    half3 lighting = Lighting(albedo * sunColor, uSunDir, normal, viewRay); 

    for (int i = 0; i < uNumPointLights; i++)
    {
        half3 lightDir = pos - uPointLights[i].position;
        float16 len = length(lightDir);
        float16 range = uPointLights[i].range;
        float16 intensity = min(len, range) / range;
    
        intensity = intensity * intensity;
        intensity = 1.0 - intensity;
        intensity *= uPointLights[i].intensity;
    
        lightDir = lightDir / len; // < normalize
        half3 lightColor = unpackUnorm4x8(uPointLights[i].color).xyz;
        lightColor = lightColor * lightColor; // convert to linear space
        half3 color = mix(albedo, lightColor, 0.55);
        lighting += Lighting(color, lightDir, normal, viewRay) * intensity;
        shadow = min(shadow + intensity, 1.0);
    }
    
    for (int i = 0; i < uNumSpotLights; i++)
    {
        LightInstance spotLight = uSpotLights[i];
        half3 lightDir = pos - spotLight.position;
        float16 len = length(lightDir);
        lightDir /= len;
        float16 angle = dot(lightDir, spotLight.direction);
        
        if (angle > spotLight.cutoff && len < spotLight.range)
        {
            angle = angle * (1.0 / spotLight.cutoff);
            angle = 1.0 - angle;
            len = min(len, spotLight.range) / spotLight.range;
            len = 1.0 - len;
            float16 effect = len * len * angle * angle;
            float16 intensity = spotLight.intensity * effect;
            half3 lightColor = unpackUnorm4x8(spotLight.color).xyz;
            lightColor = lightColor * lightColor;
            half3 color = mix(albedo, lightColor, 0.55);
    
            lighting *= 0.18 + intensity;
            lighting += Lighting(color, lightDir, normal, viewRay) * intensity;
            shadow += min(shadow + intensity, 1.0);
        }
    }

    oFragColor = CustomToneMapping(lighting).xyzz * shadow * Vignette(texCoord) * GetAO(shadow);
}