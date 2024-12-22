
layout(location = 0) out lowp vec4 oFragColorShadow; // TextureType_RGBA8
layout(location = 1) out lowp vec4 oNormalMetallic;  // TextureType_RGBA8
layout(location = 2) out lowp float oRoughness; // TextureType_R8

in mediump vec2 vTexCoords;
in highp   vec4 vLightSpaceFrag;
in lowp    mat3 vTBN;
in highp   vec3 vWorldPos;

uniform sampler2D uBarkDiffuse;
uniform sampler2D uBarkAORoughnessMetallic;
uniform sampler2D uBarkNormal;

// uniform mediump vec3 uSunDir;
void main()
{
    // vec3 worldPos = vWorldPos / vec3(3.0, 10.0, 3.0);
    vec2 texCoord = vTexCoords; // vec2((worldPos.x + 1.0) * 0.5, worldPos.y);
    texCoord.y *= 0.5;
    vec3 diffuse = texture(uBarkDiffuse, texCoord).rgb;
    vec3 ARM     = texture(uBarkAORoughnessMetallic, texCoord).rgb;
    lowp vec2  c = texture(uBarkNormal, texCoord).rg * 2.0 - 1.0;

    // obtain normal from normal map in range [0,1]
    lowp float z     = sqrt(1.0 - c.x * c.x - c.y * c.y);
    lowp vec3 normal = normalize(vec3(c, z));
    // transform normal vector to range [-1,1]
    normal  = normalize(vTBN * normal);  // this normal is in tangent space
    
    oNormalMetallic.xyz = normal + vec3(1.0) * vec3(0.5);
    oFragColorShadow.xyz = diffuse * max(ARM.r, 0.3);
    oFragColorShadow.xyz = normal;
    oFragColorShadow.w = 1.0; // todo shadow
    oNormalMetallic.w = ARM.z;
    oRoughness = ARM.y;
}