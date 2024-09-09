
#define ALPHA_CUTOFF 0
// text above edited by engine, do not touch if you don't know what it does.
precision mediump sampler2DShadow;

layout(location = 0) out lowp vec4 oFragColorShadow; // TextureType_RGBA8
layout(location = 1) out lowp vec4 oNormalMetallic;  // TextureType_RGBA8
layout(location = 2) out lowp float oRoughness; // TextureType_R8

in mediump vec2 vTexCoords;
in highp   vec4 vLightSpaceFrag;
in lowp    mat3 vTBN;
// in flat lowp int  vBoneIdx;

uniform lowp sampler2D uAlbedo;
uniform lowp sampler2D uNormalMap;

uniform lowp sampler2D  uMetallicRoughnessMap;
uniform sampler2DShadow uShadowMap; 

uniform int uHasNormalMap;

float ShadowLookup(vec4 loc, vec2 offset)
{
    vec2 texmapscale = 1.0 / vec2(textureSize(uShadowMap, 0));
    texmapscale *= 1.35; // increase spread area, to make shadow softer 
    return textureProj(uShadowMap, vec4(loc.xy + offset * texmapscale * loc.w, loc.z, loc.w));
}

// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
float ShadowCalculation()
{
    const vec4 minShadow = vec4(0.25);
    #ifdef ANDROID
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
    if (color.a < 0.8 && dot(color.rgb, color.rgb) < 0.1)
        discard;
    #endif

    mediump vec3  normal = vTBN[2];

    #if !defined(ANDROID)
    if (uHasNormalMap == 1 )
    {
        // obtain normal from normal map in range [0,1]
        lowp vec2  c = texture(uNormalMap, vTexCoords).rg * 2.0 - 1.0;
        lowp float z = sqrt(1.0 - c.x * c.x - c.y * c.y);
        normal  = normalize(vec3(c, z));
        // transform normal vector to range [-1,1]
        normal  = normalize(vTBN * normal);  // this normal is in tangent space
    }
    #endif

    lowp vec2 metallicRoughness = texture(uMetallicRoughnessMap, vTexCoords).rg;
    color.a = ShadowCalculation();
    oRoughness = metallicRoughness.y;
    oFragColorShadow = color;
    oNormalMetallic.xyz = normalize(normal) + vec3(1.0) * vec3(0.5); // convert to 0-1 range
    oNormalMetallic.a = metallicRoughness.x;
    // if (vBoneIdx > 58)
    //     oFragColorShadow = vec4(1.0);
}
