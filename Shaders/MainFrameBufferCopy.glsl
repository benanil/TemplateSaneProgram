layout(location = 0) out vec3 oFragColor;
layout(location = 1) out vec3 oNormal;

uniform sampler2D ColorTex;
uniform sampler2D NormalTex;
uniform sampler2D DepthTex;

in vec2 texCoord;         

float checkerboard(in vec2 uv)
{
    vec2 pos = floor(uv);
    return mod(pos.x + mod(pos.y, 2.0), 2.0);
}

void main() {
    // oFragColor = texture(ColorTex , texCoord).rgb;
    oNormal    = texture(NormalTex, texCoord).rgb;
    
    float d1 = textureOffset(DepthTex, texCoord, ivec2(0, 0)).r;
    float d2 = textureOffset(DepthTex, texCoord, ivec2(0, 1)).r;
    float d3 = textureOffset(DepthTex, texCoord, ivec2(1, 1)).r;
    float d4 = textureOffset(DepthTex, texCoord, ivec2(1, 0)).r;
    
    gl_FragDepth = (d1 + d2 + d3 + d4) * 0.25;
    // // #ifdef 1
    // // https://eleni.mutantstargoat.com/hikiko/depth-aware-upsampling-2/
    // float d1 = textureOffset(DepthTex, texCoord, ivec2(0, 0)).r;
    // float d2 = textureOffset(DepthTex, texCoord, ivec2(0, 1)).r;
    // float d3 = textureOffset(DepthTex, texCoord, ivec2(1, 1)).r;
    // float d4 = textureOffset(DepthTex, texCoord, ivec2(1, 0)).r;
    // 
    // gl_FragDepth = mix(max(max(d1, d2), max(d3, d4)),
    //                    min(min(d1, d2), min(d3, d4)),
    //                    checkerboard(texCoord));
    // // #else
    // vec4 edges = vec4(textureOffset(DepthTex, texCoord, ivec2( 1,  1)).r,
    //                   textureOffset(DepthTex, texCoord, ivec2(-1, -1)).r,
    //                   textureOffset(DepthTex, texCoord, ivec2( 1, -1)).r,
    //                   textureOffset(DepthTex, texCoord, ivec2(-1,  1)).r);
    // float center = texture(DepthTex, texCoord).r;
    // oDepth = center * 0.2 + dot(edges, vec4(0.2));
    // #endif
}
