
out lowp vec4 OutColor;

in lowp      float oFade;
in lowp      vec4  oColor;
in flat lowp uint  oEffect;
in flat lowp float oCutStart;

void main()
{
    bool hasFade = bool(oEffect & 1);
    bool hasCut  = bool((oEffect & 2) >> 1);
    lowp vec4 result = oColor * mix(1.0, oFade, hasFade);
    result  *= mix(1.0, 0.0, hasCut && oFade < oCutStart);
    OutColor = result;
}