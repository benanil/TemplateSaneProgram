
#define ALPHA_CUTOFF 0
// text above edited by engine, do not touch if you don't know what it does.
precision mediump sampler2DShadow;

layout(location = 0) out lowp vec3 oFragColor; // TextureType_RGB8
layout(location = 1) out lowp vec3 oNormal;    // TextureType_RGB8
layout(location = 2) out lowp vec3 oShadowMetallicRoughness; // TextureType_RGB565

in mediump vec2 vTexCoords;
in highp   vec4 vLightSpaceFrag;
in lowp    mat3 vTBN;

uniform lowp sampler2D uAlbedo;
uniform lowp sampler2D uNormalMap;

uniform lowp sampler2D  uMetallicRoughnessMap;
uniform sampler2DShadow uShadowMap; 

uniform int uHasNormalMap;

float ShadowLookup(vec4 loc, vec2 offset)
{
    const vec2 texmapscale = vec2(0.0006, 0.0006);
    return textureProj(uShadowMap, vec4(loc.xy + offset * texmapscale * loc.w, loc.z, loc.w));
}

// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
float ShadowCalculation()
{
    const vec4 minShadow = vec4(0.30);
    #ifdef __ANDROID__
    vec2 offset;
    const vec2 mixer = vec2(1.037, 1.137);
    offset.x = fract(dot(vTexCoords.xy, mixer)) * 0.5;
    offset.y = fract(dot(vTexCoords.yx, mixer)) * 0.5;
    
    vec4 shadow = vec4(ShadowLookup(vLightSpaceFrag, offset + vec2(-0.50,  0.25)),
                       ShadowLookup(vLightSpaceFrag, offset + vec2( 0.25,  0.25)),
                       ShadowLookup(vLightSpaceFrag, offset + vec2(-0.50, -0.50)),
                       ShadowLookup(vLightSpaceFrag, offset + vec2( 0.25, -0.50)));
    
    return dot(max(shadow, minShadow), vec4(1.0)) * 0.25;  // max is used with 4 elements maybe it helps to make this simd
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
    lowp vec4 color = texture(uAlbedo, vTexCoords);
    #if ALPHA_CUTOFF == 1
        if (color.a < 0.001)
            discard;
    #endif

    lowp vec3  normal    = vTBN[2];
    lowp float metallic  = 0.5;
    lowp float roughness = 0.3;

    #if !defined(__ANDROID__)
    if (uHasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        lowp vec2  c = texture(uNormalMap, vTexCoords).rg * 2.0 - 1.0;
        lowp float z = sqrt(1.0 - c.x * c.x - c.y * c.y);
        normal  = normalize(vec3(c, z));
        // transform normal vector to range [-1,1]
        normal  = normalize(vTBN * normal);  // this normal is in tangent space

        lowp vec2 metalRoughness = texture(uMetallicRoughnessMap, vTexCoords).rg;
        metallic  = metalRoughness.r;
        roughness = metalRoughness.g;
    }
    #endif
    float shadow = ShadowCalculation();
    oShadowMetallicRoughness = vec3(shadow, metallic, roughness);
    oFragColor = color.rgb;
    oNormal = normal + vec3(1.0) * vec3(0.5); // convert to 0-1 range
}
