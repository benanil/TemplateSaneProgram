
layout(location = 0) out lowp vec3 oFragColor;
layout(location = 1) out lowp vec3 oNormal;

uniform lowp  sampler2D uNormalTex;
uniform highp sampler2D uDepthTex;

in vec2 texCoord;

float checkerboard(in vec2 uv)
{
    vec2 pos = floor(uv);
    return mod(pos.x + mod(pos.y, 2.0), 2.0);
}

void main() {
    // oFragColor = texture(ColorTex , texCoord).rgb;
    oNormal    = texture(uNormalTex, texCoord).rgb;
    
    #if 1
    // https://eleni.mutantstargoat.com/hikiko/depth-aware-upsampling-2/
    float d1 = textureOffset(uDepthTex, texCoord, ivec2(0, 0)).r;
    float d2 = textureOffset(uDepthTex, texCoord, ivec2(0, 1)).r;
    float d3 = textureOffset(uDepthTex, texCoord, ivec2(1, 1)).r;
    float d4 = textureOffset(uDepthTex, texCoord, ivec2(1, 0)).r;
    
    gl_FragDepth = mix(max(max(d1, d2), max(d3, d4)),
                       min(min(d1, d2), min(d3, d4)),
                       checkerboard(texCoord));
    #else
    float d1 = textureOffset(uDepthTex, texCoord, ivec2(0, 0)).r;
    float d2 = textureOffset(uDepthTex, texCoord, ivec2(0, 1)).r;
    float d3 = textureOffset(uDepthTex, texCoord, ivec2(1, 1)).r;
    float d4 = textureOffset(uDepthTex, texCoord, ivec2(1, 0)).r;
    
    gl_FragDepth = (d1 + d2 + d3 + d4) * 0.25;
    #endif
}
