out vec4 Result;

in mediump vec2 vTexCoord;
in lowp vec4 vColor;

in      lowp float oFade;
in flat lowp uint  oEffect;
in flat lowp float oCutStart;

void main() 
{
    bool hasFade = bool(oEffect & 1);
    bool hasCut  = bool((oEffect & 2) >> 1);
    bool hasIntenseFade = bool((oEffect & 16) >> 4);
    bool hasCenterFade  = bool((oEffect & 32) >> 5);

    lowp float fade = oFade;
    lowp float centerFade = 1.0f - abs(2.0 * fade - 1.0);
    fade = mix(fade, centerFade, hasCenterFade);
    fade = mix(fade, fade * 1.5, hasIntenseFade);

    lowp vec4 result = vColor * mix(1.0, fade, hasFade);
    result  *= mix(1.0, 0.0, hasCut && fade < oCutStart);
    Result = result;
}