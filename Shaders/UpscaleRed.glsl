
layout(location = 0) out float Result;

uniform sampler2D halfTex;
in vec2 texCoord;         

void main() 
{
    float ao = dot(vec4(textureOffset(halfTex, texCoord, ivec2( 1,  1)).r,
                        textureOffset(halfTex, texCoord, ivec2(-1, -1)).r,
                        textureOffset(halfTex, texCoord, ivec2(-1,  1)).r,
                        textureOffset(halfTex, texCoord, ivec2( 1, -1)).r), vec4(0.2));
    Result = ao + (texture(halfTex, texCoord).r * 0.2);
}