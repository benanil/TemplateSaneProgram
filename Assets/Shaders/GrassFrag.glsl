
layout(location = 0) out lowp vec4 oFragColorShadow; // TextureType_RGBA8
layout(location = 1) out lowp vec4 oNormalMetallic;  // TextureType_RGBA8
layout(location = 2) out lowp float oRoughness; // TextureType_R8

in mediump vec3 vWorldPos;
in mediump vec3 vNormal;
in lowp float vIsTop; // top vertex of triangle ?

void main()
{
    const vec3 bottomColor = vec3(27.0 / 255.0, 34.0 / 255.0, 10.0 / 255.0);
    const vec3 topColor    = vec3(60.0 / 255.0, 70.0 / 255.0, 15.0 / 255.0);

    oFragColorShadow.xyz = mix(bottomColor, topColor, vIsTop);
    oFragColorShadow.xyz = pow(oFragColorShadow.xyz, vec3(1.0 / 2.2));
    oFragColorShadow.w = 1.0;

    oNormalMetallic.xyz = vNormal;
    oNormalMetallic.w = 0.0;

    oRoughness = 0.3;
}