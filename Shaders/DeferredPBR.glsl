
#ifdef __ANDROID__
    #define MEDIUMP_FLT_MAX    65504.0
    #define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#else
    #define saturateMediump(x) x
#endif

#define float16 mediump float
#define half2   mediump vec2
#define half3   mediump vec3

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

uniform highp   vec3 uPlayerPos;
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

// I've made custom lighting shader below,
// if you want you can use google filament's mobile pbr renderer:
// https://google.github.io/filament/Filament.html

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

float16 EaseInCirc(float16 x)
{
    return 1.0 - sqrt(1.0 - (x * x));
}

// get shadow and ao
float GetShadow(float shadow, vec3 surfPos)
{
    // lowp float ao = 0.0;
    // #ifdef __ANDROID__
    // ao = Blur5(texture(uAmbientOclussionTex, texCoord).r,
    //      textureOffset(uAmbientOclussionTex, texCoord, ivec2(-1, 0)).r,
    //      textureOffset(uAmbientOclussionTex, texCoord, ivec2( 1, 0)).r,
    //      textureOffset(uAmbientOclussionTex, texCoord, ivec2(-2, 0)).r,
    //      textureOffset(uAmbientOclussionTex, texCoord, ivec2( 2, 0)).r);
    // #endif
    // fake player shadow, works like point light but darkens instead of lighting
    vec3 playerPos = uPlayerPos + vec3(0.0, 0.3, 0.0);
    vec3 surfDir = playerPos - surfPos;
    surfDir *= 4.0; // scale down the shadow by 4.0x
    float len = inversesqrt(dot(surfDir, surfDir));
    float playerShadow = 1.0 - min(len, 1.0);
    return shadow * EaseInCirc(playerShadow);
}

float sdBox(vec2 p, vec2 b)
{
    vec2 d = abs(p) - b;
    return min(max(d.x,d.y),0.0) + length(max(d,0.0));
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
    half3 sunColor = vec3(0.982f, 0.972, 0.966);
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

    oFragColor = CustomToneMapping(lighting).xyzz * Vignette(texCoord) * GetShadow(shadow, pos);
}