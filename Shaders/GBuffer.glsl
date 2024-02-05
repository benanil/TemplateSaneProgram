
precision mediump sampler2DShadow;

layout(location = 0) out vec3 oFragColor; // TextureType_RGB8
layout(location = 1) out vec3 oNormal;    // TextureType_R11F_G11F_B10
layout(location = 2) out vec3 oShadowMetallicRoughness;// TextureType_RGB8

in highp   vec3 vFragPos;
in highp   vec2 vTexCoords;
in highp   vec4 vLightSpaceFrag;
in mediump mat3 vTBN;

uniform sampler2D albedo;
uniform sampler2D normalMap;

uniform sampler2D metallicRoughnessMap;
uniform sampler2DShadow shadowMap;

uniform int hasNormalMap;

float ShadowLookup(vec4 loc, vec2 offset)
{
    const vec2 texmapscale = vec2(0.0007, 0.0007);
    return textureProj(shadowMap, vec4(loc.xy + offset * texmapscale * loc.w, loc.z, loc.w));
}

// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
float ShadowCalculation()
{
    const vec4 minShadow = vec4(0.35);
    #ifdef __ANDROID__
    vec2 offset;
    const vec2 mixer = vec2(1.037, 1.137);
    offset.x = fract(dot(vTexCoords.xy, mixer)) * 0.5;
    offset.y = fract(dot(vTexCoords.yx, mixer)) * 0.5;

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
    #if ALPHA_CUTOFF
    if (color.a < 0.001)
    discard;
    #endif

    mediump vec3 normal   = vTBN[2];
    mediump vec3 lighting = vec3(0.0);

    float shadow = ShadowCalculation();

    mediump float metallic  = 0.5;
    mediump float roughness = 0.3;
    #ifndef __ANDROID__
    if (hasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        mediump vec2  c = texture(normalMap, vTexCoords).rg * 2.0 - 1.0;
        mediump float z = sqrt(1.0 - c.x * c.x - c.y * c.y);
        normal  = normalize(vec3(c, z));
        // transform normal vector to range [-1,1]
        normal  = normalize(vTBN * normal);  // this normal is in tangent space

        mediump vec2 metalRoughness = texture(metallicRoughnessMap, vTexCoords).rg;
        metallic  = metalRoughness.r;
        roughness = metalRoughness.g;
    }
    #endif
    oShadowMetallicRoughness = vec3(shadow, metallic, roughness);
    oFragColor = texture(albedo, vTexCoords).rgb;
    oNormal    = normal + vec3(1.0) * vec3(0.5); // convert to 0-1 range
    //oPosition  = vFragPos;
}
        