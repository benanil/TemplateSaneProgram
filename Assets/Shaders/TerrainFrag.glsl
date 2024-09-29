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

// https://www.shadertoy.com/view/3sd3Rs
float bnoise(float x) {
    // setup    
    float i = floor(x);
    float f = fract(x);
    float s = sign(fract(x / 2.0) - 0.5);
    // use some hash to create a random value k in [0..1] from i
    // float k = hash(uint(i));
    float k = fract(i * .1731);
    // quartic polynomial
    return s * f * (f - 1.0) * ((16.0 * k - 4.0) * f * (f - 1.0) - 1.0);
}

float sum(vec3 v) { return v.x + v.y + v.z; }

float sqr(float x) { return x * x; }

// https://www.shadertoy.com/view/Xtl3zf seamless texture
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
    vec3 normal = normalize(vNormal * 2.0 - 1.0);
    const float terrainHeight = 35.0 * 2.24;
    float heightNorm = max(vWorldPos.y / terrainHeight, 0.0);

    const lowp float scales[3] = float[3](0.05, 0.034, 0.04);    
    vec3 aoRoughMetal0 = texture(mLayer0ARM, vWorldPos.xz * scales[0]).rgb;
    vec3 aoRoughMetal1 = texture(mLayer1ARM, vWorldPos.xz * scales[1]).rgb;
    vec3 aoRoughMetal2 = texture(mLayer2ARM, vWorldPos.xz * scales[2]).rgb;

    float ao0 = clamp(aoRoughMetal0.r + 0.15, 0.15, 1.0);
    float ao1 = clamp(aoRoughMetal1.r + 0.15, 0.15, 1.0);
    float ao2 = clamp(aoRoughMetal2.r + 0.15, 0.15, 1.0);

    float roughness0 = aoRoughMetal0.g * aoRoughMetal0.g;
    float roughness1 = aoRoughMetal1.g * aoRoughMetal1.g;
    float roughness2 = aoRoughMetal2.g * aoRoughMetal2.g;
    
    vec3 mossColor  = textureNoTile(mLayer0Diff, vWorldPos.xz * scales[0], 0.6) * ao0;
    vec3 grassColor = textureNoTile(mLayer1Diff, vWorldPos.xz * scales[1], 0.5) * ao1; // texture(mLayer1Diff, vWorldPos.xz * scales[1]).rgb * ao1;
    vec3 rockColor  = textureNoTile(mLayer2Diff, vWorldPos.xz * scales[2], 0.4) * ao2; // texture(mLayer2Diff, vWorldPos.xz * scales[2]).rgb * ao2;
    
    float steepness = 1.0 - sqr(normal.y);//abs(normal.x) + abs(normal.z); 
    float s = (vWorldPos.x + vWorldPos.y) * 0.1;
    s = sin(s)*.018 + (sin(s*4.0)*.008);
    
    float isRock = smoothstep(0.84, 0.92, steepness);   
    float isMoss = smoothstep(0.40, 0.70, steepness) - isRock;
    
    float grassBlend = max(  smoothstep(0.244, 0.210, heightNorm - s) - isRock, 0.0);
    float rockBlend  =       smoothstep(0.670, 0.700, heightNorm + s) + isRock;
    float mossBlend = clamp(smoothstep(0.195, 0.244, heightNorm - s) - rockBlend + isMoss, 0.0, 1.0);
    
    float total = 1.0 / (mossBlend + grassBlend + rockBlend);
    vec3 color = (mossColor * mossBlend +  // vec3(grassBlend, mossBlend, rockBlend);
                 grassColor * grassBlend +
                 rockColor  * rockBlend) * total;
                 
    oFragColorShadow.xyz = color;

    oFragColorShadow.w = 1.0;
    oNormalMetallic.xyz = vNormal;
    oNormalMetallic.w = (aoRoughMetal0.b * mossBlend + aoRoughMetal1.b * grassBlend + aoRoughMetal2.b * rockBlend) * total;

    oRoughness = (roughness0 * mossBlend + roughness1 * grassBlend + roughness2 * rockBlend) * total;
}