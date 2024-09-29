
out lowp vec4 Result;

uniform lowp sampler2D mPerlinNoise;

// https://stackoverflow.com/questions/5281261/generating-a-normal-map-from-a-height-map
void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    // read neightbor heights using an arbitrary small offset relative to grid size
    float hL = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(-1, 0)).r;
    float hR = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(1, 0)).r;
    float hD = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(0, -1)).r;
    float hU = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(0, 1)).r;
    
    // deduce terrain normal
    vec3 normal;
    normal.x = hL - hR;
    normal.z = hD - hU;
    normal.y = 0.03;
    normal = normalize(normal);
    Result.xyz = (normal + 1.0) * 0.5;
    Result.w = 1.0;
}
