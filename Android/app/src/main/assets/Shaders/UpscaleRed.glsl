
layout(location = 0) out lowp float Result;

uniform lowp sampler2D halfTex;
in vec2 texCoord;

void main() 
{
    lowp float ao = 0.0;
    ao += textureOffset(halfTex, texCoord, ivec2( 0,  0)).r * 0.2;
    ao += textureOffset(halfTex, texCoord, ivec2( 1,  1)).r * 0.2;
    ao += textureOffset(halfTex, texCoord, ivec2(-1, -1)).r * 0.2;
    ao += textureOffset(halfTex, texCoord, ivec2(-1,  1)).r * 0.2;
    ao += textureOffset(halfTex, texCoord, ivec2( 1, -1)).r * 0.2;
    Result = ao;
}