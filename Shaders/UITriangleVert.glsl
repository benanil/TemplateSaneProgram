
layout(location = 0) in highp uint vPos; // 2x16 fixed point
layout(location = 1) in highp uint vData; // fade, depth, cutStart, effect
layout(location = 2) in lowp  vec4 vColor;

out lowp      float oFade;
out lowp      vec4  oColor;
out flat lowp uint  oEffect;
out flat lowp float oCutStart;

uniform ivec2 uScrSize;

void main()
{
    vec2 pos = vec2(float((vPos >> 0u ) & 0xFFFFu),
                    float((vPos >> 16u) & 0xFFFFu)) / 10.0f;
    lowp vec4 unpackData = unpackUnorm4x8(vData);
    lowp float fade = unpackData.x;
    lowp float depth = unpackData.y;
    lowp float cutStart = unpackData.z;

    vec2 proj = 2.0 / vec2(uScrSize);
    vec2 translate = proj * pos - 1.0;
    translate.y = -translate.y; // 1080 - position.y
    gl_Position = vec4(translate, depth, 1.0);

    oColor    = vColor;
    oEffect   = (vData >> 24u) & 0xFFu;
    oCutStart = cutStart;
    oFade     = fade;
}