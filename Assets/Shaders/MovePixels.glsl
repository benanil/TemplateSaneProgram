
out lowp vec4 Result;

uniform lowp sampler2D mSource;
uniform ivec2 mMoveDir;

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    Result = texelFetch(mSource, coord + (mMoveDir * 64), 0);
}