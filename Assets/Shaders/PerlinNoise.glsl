
out float Result;

uniform ivec2 mChunkOffset;
uniform ivec2 mMoveDir; // used when we rendering some parts of texture

uniform float START_HEIGHT; // = 0.4;
uniform float WEIGHT; // = 0.6;
uniform float MULT; // = 0.35;

in vec2 texCoord;

// Simple 2d noise algorithm from http://shadertoy.wikia.com/wiki/Noise
// I tweaked a few values
float noise( vec2 p ) 
{
    vec2 f = fract(p);
    p = floor(p);
    float v = p.x + p.y * 1000.0;
    vec4 r = vec4(v, v + 1.0, v + 1000.0, v + 1001.0);
    r = fract(10000.0 * sin(r * .001));
    f = f * f * (3.0 - 2.0 * f);
    return 2.0 * (mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y)) - 1.0;
}

//generate terrain using above noise algorithm
float terrain(vec2 p, int freq, float h, float w, float m) 
{
    for (int i = 0; i < freq; i++) {
        h += w * noise((p * m)); // adjust height based on noise algorithm
        w *= 0.5;
        m *= 2.0;
    }
    return h;
}

void main() 
{
    const float numQuads = 64.0f;
    const float quadSize = 20.0f;
    const float quadNumSegments = 8.0;
    const float chunkSize = quadSize * numQuads;
    const float offsetSize = chunkSize / 8.0;
    const int offsetSizei = 512 / 8;
    
    ivec2 icoord = ivec2(gl_FragCoord.xy);
    icoord += mChunkOffset * offsetSizei;

    vec2 coord = vec2(icoord) / 512.0;
    vec2 seed = vec2(5.0);
    Result  = clamp(terrain((coord * 20.0) - seed, 16, 0.250, 0.5, 0.25),  0.00, 1.0);
    Result += clamp(terrain((coord * 20.0) - seed, 16, 0.198, 1.6, 0.21), -0.16, 2.7);
    // Result = perlinNoise(offset + texCoord * 5.0, 1, 8, 0.4, 2.5, 5774255) ; // 342, 
}