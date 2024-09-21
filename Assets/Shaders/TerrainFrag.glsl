
out vec4 Result;

layout(location = 0) out lowp vec4 oFragColorShadow; // TextureType_RGBA8
layout(location = 1) out lowp vec4 oNormalMetallic;  // TextureType_RGBA8
layout(location = 2) out lowp float oRoughness; // TextureType_R8

uniform sampler2D mLayer0Diff;
uniform sampler2D mLayer1Diff;
uniform sampler2D mLayer2Diff;

uniform sampler2D mLayer0ARM;
uniform sampler2D mLayer1ARM;
uniform sampler2D mLayer2ARM;

uniform sampler2D mGrayNoise;

in mediump vec3 vWorldPos;
in mediump vec3 vNormal;

float EaseOut(float x) { 
    float r = 1.0 - x;
    return 1.0 - (r * r); 
}

float hash( uint n ) 
{   // integer hash copied from Hugo Elias
	n = (n << 13U) ^ n; 
    n = n * (n * n * 15731U + 789221u) + 1376312589u;
    return float(n & 0x0fffffffu) / float(0x0fffffffu);
}

// https://www.shadertoy.com/view/3sd3Rs
float bnoise(float x) {
    // setup    
    float i = floor(x);
    float f = fract(x);
    float s = sign(fract(x/2.0)-0.5);
    // use some hash to create a random value k in [0..1] from i
    // float k = hash(uint(i));
    // float k = 0.5+0.5*sin(i);
    float k = fract(i*.1731);
    // quartic polynomial
    return s*f*(f-1.0)*((16.0*k-4.0)*f*(f-1.0)-1.0);
}

float sum(vec3 v) { return v.x + v.y + v.z; }

// https://www.shadertoy.com/view/Xtl3zf
// seamless texture
vec3 textureNoTile(sampler2D sampler, vec2 x, float v)
{
    float k = texture(mGrayNoise, 0.005 * x).x; // cheap (cache friendly) lookup
    float l = k * 8.0;
    float f = fract(l);
    
    float ia = floor(l + 0.5); // suslik's method 
    float ib = floor(l);
    f = min(f, 1.0 - f) * 2.0;
    
    vec2 offa = sin(vec2(3.0, 7.0) * ia); // can replace with any other hash
    vec2 offb = sin(vec2(3.0, 7.0) * ib); // can replace with any other hash

    vec2 duvdx = dFdx(x);
    vec2 duvdy = dFdy(x);
    vec3 cola = textureGrad(sampler, x + v * offa, duvdx, duvdy).xyz;
    vec3 colb = textureGrad(sampler, x + v * offb, duvdx, duvdy).xyz;
    return mix(cola, colb, smoothstep(0.2, 0.8, f - 0.1 * sum(cola - colb)));
}

void main()
{
    const float sunAngle = -0.16f;
    const vec3 sunDir = normalize(vec3(-0.20f, abs(cos(sunAngle)) + 0.1f, sin(sunAngle)));

    vec3 normal = vNormal * 2.0 - 1.0;
    float ndl = dot(normal, sunDir);
    const float terrainHeight = 35.0;
    float ynorm = (vWorldPos.y ) / terrainHeight;

    const lowp float scales[3]    = float[3](0.06, 0.04, 0.06);
    const lowp float tresholds[3] = float[3](0.10, 0.34, 0.52);

    vec3 aoRoughMetal0 = texture(mLayer0ARM, vWorldPos.xz * scales[0]).rgb;
    vec3 aoRoughMetal1 = texture(mLayer1ARM, vWorldPos.xz * scales[1]).rgb;
    vec3 aoRoughMetal2 = texture(mLayer2ARM, vWorldPos.xz * scales[2]).rgb;

    float ao0 = clamp(aoRoughMetal0.r + 0.15, 0.15, 1.0);
    float ao1 = clamp(aoRoughMetal1.r + 0.15, 0.15, 1.0);
    float ao2 = clamp(aoRoughMetal2.r + 0.15, 0.15, 1.0);

    float roughness0 = aoRoughMetal0.g * aoRoughMetal0.g;
    float roughness1 = aoRoughMetal1.g * aoRoughMetal1.g;
    float roughness2 = aoRoughMetal2.g * aoRoughMetal2.g;
    
    vec3 mossColor = textureNoTile(mLayer0Diff, vWorldPos.xz * scales[0], 0.6);
    vec3 grassColor = texture(mLayer1Diff, vWorldPos.xz * scales[1]).rgb * ao1;
    vec3 rockColor = textureNoTile(mLayer2Diff, vWorldPos.xz * scales[2], 0.4); // texture(mLayer2Diff, vWorldPos.xz * scales[2]).rgb * ao2;
    
    const float blendSize = 0.05;
    float mossBlend  = 0.0, grassBlend = 0.0, rockBlend  = 0.0;
    
    if      (ynorm < tresholds[0]) mossBlend = 1.0;
    else if (ynorm < tresholds[0] + blendSize) grassBlend = (ynorm - tresholds[0]) * 20.0, mossBlend = 1.0 - grassBlend;
    else if (ynorm < tresholds[2]) grassBlend = 1.0;
    else if (ynorm < tresholds[2] + blendSize) rockBlend = (ynorm - tresholds[2]) * 20.0, grassBlend = 1.0 - rockBlend;
    else if (ynorm > tresholds[2] + blendSize) rockBlend = 1.0;

    vec3 color = mossColor  * mossBlend  +
                 grassColor * grassBlend +
                 rockColor  * rockBlend;

    oFragColorShadow.xyz = color * 1.2;
    oFragColorShadow.xyz *= ndl;

    oFragColorShadow.w = 1.0;
    oNormalMetallic.xyz = normal;
    oNormalMetallic.w = aoRoughMetal0.b * mossBlend  +
                        aoRoughMetal1.b * grassBlend +
                        aoRoughMetal2.b * rockBlend;

    oRoughness = roughness0 * mossBlend  +
                 roughness1 * grassBlend +
                 roughness2 * rockBlend;
}