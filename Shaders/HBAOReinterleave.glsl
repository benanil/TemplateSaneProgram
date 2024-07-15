
layout(location = 0) out vec4 out_Color;
uniform sampler2DArray texResultsArray;

//----------------------------------------------------------------------------------

void main() {
    ivec2 FullResPos = ivec2(gl_FragCoord.xy);
    ivec2 Offset = FullResPos & 3;
    int SliceId = Offset.y * 4 + Offset.x;
    ivec2 QuarterResPos = FullResPos >> 2;
    out_Color = vec4(texelFetch(texResultsArray, ivec3(QuarterResPos, SliceId), 0).x);
}