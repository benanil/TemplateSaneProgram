

#ifdef ANDROID
#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#else
#define saturateMediump(x) x
#endif

#define float16 mediump float
#define half2   mediump vec2
#define half3   mediump vec3
#define half4   mediump vec4

layout(location = 0) out vec4 oFragColor; // TextureType_RGBA8, alpha is luminence

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

uniform lowp  sampler2D uAlbedoShadowTex; // albedo + shadow
uniform lowp  sampler2D uRoughnessTex;
uniform lowp  sampler2D uNormalMetallicTex; // normal + metallic
uniform highp sampler2D uDepthMap;
uniform lowp  sampler2D uAmbientOclussionTex; // < ambient occlusion

uniform highp   vec3 uPlayerPos;
uniform mediump vec3 uSunDir;

uniform highp mat4 uInvView;
uniform highp mat4 uInvProj;

const float PI = 3.1415926535;

mediump vec3 toLinear(mediump vec3 sRGB)
{
    // https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return sRGB * (sRGB * (sRGB * 0.305306011 + 0.682171111) + 0.012522878);
}

float sqr(float x) { return x*x; }
float16 sqrf16(float16 x) { return x*x; }

// I've made custom lighting shader below,
// if you want you can use google filament's mobile pbr renderer:
// https://google.github.io/filament/Filament.html

float16 D_GGX(float16 NoH, float16 roughness) {
    float16 a = NoH * roughness;
    float16 k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

float16 V_SmithGGXCorrelated_Fast(float16 roughness, float16 NoV, float16 NoL) {
    // Hammon 2017, "PBR Diffuse Lighting for GGX+Smith Microsurfaces"
    float16 v = 0.5 / mix(2.0 * NoL * NoV, NoL + NoV, roughness);
    return (v); //saturateMediump
}

float16 V_Neubelt(float16 NoV, float16 NoL) {
    // Neubelt and Pettineo 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"
    return (1.0 / (4.0 * (NoL + NoV - NoL * NoV))); // saturateMediump
}

float16 F_Schlick(float16 u, half3 albedo, float16 metallic) {
    float16 F0 = 0.04;
    F0 = mix(F0, dot(albedo, vec3(0.333333)), metallic);
    float16 x = 1.0 - u;
    return F0 + (1.0 - F0) * (x * x * x * x); // * x
}

half3 Lighting(half3 albedo, half3 l, half3 n, half3 v,
               float16 metallic, float16 roughness, float16 ao)
{
    half3 h = normalize(l + v);
    float16 ndl = max(dot(n, l), 0.10);
    float16 ndh = max(dot(n, h), 0.0);
    float16 ndv = abs(dot(n, v)) + 1e-5;
    float16 ldh = max(dot(l, h), 0.0);

    float16 D = D_GGX(ndh, roughness);
    float16 V = V_SmithGGXCorrelated_Fast(roughness, ndv, ndl) * ao; // V_Neubelt(ndv, ndl); 
    float16 F = F_Schlick(ldh, albedo, metallic);
    float16 Fr = F * (D * V); // specular BRDF
    half3   Fd = (albedo / PI);
    return Fd + Fr; //albedo * ndl + (r * 0.08);
}

vec3 WorldSpacePosFromDepthBuffer()
{
    float depth = texture(uDepthMap, texCoord).r;
    vec2 uv = texCoord * 2.0 - 1.0;
    vec4 vsPos = uInvProj * vec4(uv, depth * 2.0 - 1.0, 1.0);
    vsPos /= vsPos.w;
    vsPos = uInvView * vsPos;
    return vsPos.xyz;
}

// Direction vector from the surface point to the viewer (normalized).
// direction between WorldSpacePos and 3rd row of invView(viewPos)
vec3 GetViewRay(vec3 viewPos, vec3 worldSpacePos)
{
    return normalize(viewPos - worldSpacePos);
}

float16 EaseInCirc(float16 x)
{
    return 1.0 - sqrt(1.0 - (x * x));
}

// get shadow and ao
float GetShadow(float shadow, vec3 surfPos)
{
    // if (gl_FragCoord.x > 900.0f) ao = 1.0f;
    // fake player shadow, works like point light but darkens instead of lighting
    vec3 playerPos = uPlayerPos + vec3(0.0, 0.15, 0.0);
    vec3 surfDir = playerPos - surfPos;
    surfDir *= 6.0; // scale down the shadow by 6.0x
    float len = inversesqrt(dot(surfDir, surfDir));
    float playerShadow = 1.0 - min(len, 1.0);
    return shadow * EaseInCirc(playerShadow);
}

void main()
{
    // vertical blur
    lowp float ao = texture(uAmbientOclussionTex, texCoord).r;

    float16 roughness    = texture(uRoughnessTex, texCoord).r;
    half4 normalMetallic = texture(uNormalMetallicTex, texCoord);
    half4 albedoShadow   = texture(uAlbedoShadowTex, texCoord); 
    
    albedoShadow.rgb = toLinear(albedoShadow.rgb);
    normalMetallic.rgb = normalize(normalMetallic.rgb * 2.0 - 1.0);
    
    float16 shadow    = albedoShadow.w;
    float16 metallic  = normalMetallic.w;

    ao *= min(shadow * 3.0, 1.0);
    ao += 0.05;
    ao = min(1.0, ao * 1.1);

    vec3 pos = WorldSpacePosFromDepthBuffer();
    
    half3 viewRay = GetViewRay(uInvView[3].xyz, pos); // viewPos: uInvView[3].xyz
    const half3 sunColor = vec3(1.0); // 0.982f, 0.972, 0.966);
    
    half3 lighting = Lighting(albedoShadow.rgb * sunColor, uSunDir, 
                              normalMetallic.xyz, viewRay, metallic, roughness, ao);

    for (int i = 0; i < uNumPointLights; i++)
    {
        half3 lightDir = pos - uPointLights[i].position;
        float16 len = length(lightDir);
        float16 range = uPointLights[i].range;
        float16 mIntensity = min(len, range) / range;
    
        mIntensity = mIntensity * mIntensity;
        mIntensity = 1.0 - mIntensity;
        mIntensity *= uPointLights[i].intensity;
    
        lightDir = lightDir / len; // < normalize
        half3 lightColor = unpackUnorm4x8(uPointLights[i].color).xyz;
        lightColor = lightColor * lightColor; // convert to linear space
        half3 color = mix(albedoShadow.rgb, lightColor, 0.55);
        lighting += Lighting(color, lightDir, normalMetallic.xyz, viewRay, 
                             metallic, roughness, ao) * mIntensity;
        shadow = min(shadow + mIntensity, 1.0);
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
            angle = (angle-spotLight.cutoff) * (1.0 / (1.0 - spotLight.cutoff));
            len = min(len, spotLight.range) / spotLight.range;
            len = 1.0 - len;
            
            float16 mIntensity = spotLight.intensity * len * angle;
            half3 lightColor = unpackUnorm4x8(spotLight.color).xyz;
            lightColor = lightColor * lightColor;
            half3 color = mix(albedoShadow.rgb, lightColor, 0.55);
            lighting *= max(mIntensity, 0.25);
    
            half3 sl = Lighting(color, lightDir, normalMetallic.xyz, viewRay, 
                                metallic, roughness, ao);
            lighting += sl * mIntensity;
            shadow += shadow + mIntensity;
        }
    }
    
    shadow = max(min(shadow, 1.0), 0.0);
    oFragColor.rgb = pow(lighting * GetShadow(shadow, pos), vec3(1.0 / 2.2));
    oFragColor.a = dot(oFragColor.rgb, vec3(.299f, .587f, .114f));
}