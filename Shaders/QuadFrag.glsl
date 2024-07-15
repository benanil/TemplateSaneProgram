out lowp vec4 Result;

layout(location = 0) in lowp vec2 vTexCoord;
layout(location = 1) in lowp vec4 vColor;
layout(location = 2) in      lowp float oFade;
layout(location = 3) in flat lowp uint  oEffect; // bitmask
layout(location = 4) in flat lowp float oCutStart;

void main() 
{
    bool hasFade = bool(oEffect & 1u);
    bool hasCut  = bool((oEffect & 2u) >> 1u);
    bool hasInvertFade  = bool((oEffect & 4u) >> 2u);
    bool hasIntenseFade = bool((oEffect & 16u) >> 4u);
    bool hasCenterFade  = bool((oEffect & 32u) >> 5u);

    lowp float fade = oFade;
    lowp float centerFade = 1.0f - abs(2.0 * fade - 1.0);
    fade = mix(fade, centerFade, hasCenterFade);
    fade = mix(fade, fade * 1.5, hasIntenseFade);
    fade = mix(fade, 1.0f - fade, hasInvertFade);

    lowp vec4 result = vColor * mix(1.0, fade, hasFade);
    result  *= mix(1.0, 0.0, hasCut && fade < oCutStart);
    Result = result;
}