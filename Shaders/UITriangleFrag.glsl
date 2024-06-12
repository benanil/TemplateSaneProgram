
out lowp vec4 OutColor;

in lowp      float oFade;
in lowp      vec4  oColor;
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

    lowp vec4 result = oColor * mix(1.0, fade, hasFade);
    result  *= mix(1.0, 0.0, hasCut && fade < oCutStart);
    OutColor = result;
}