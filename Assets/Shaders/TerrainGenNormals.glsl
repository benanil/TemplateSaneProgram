
out lowp vec3 Result;

uniform sampler2D mPerlinNoise;

// https://stackoverflow.com/questions/5281261/generating-a-normal-map-from-a-height-map
void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    
    float heightRight = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(+1, 0)).r;
    float heightLeft  = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(-1, 0)).r;
    float heightUp    = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(0 ,+1)).r;
    float heightDown  = texelFetchOffset(mPerlinNoise, coord, 0, ivec2(0 ,-1)).r;

    float s01 = heightLeft  * 5.0f;
    float s21 = heightRight * 5.0f;
    float s10 = heightDown  * 5.0f;
    float s12 = heightUp    * 5.0f;

    const vec2 size = vec2(2.0, 0.0);
    vec3 va = normalize(vec3(size.x, s21-s01, size.y));
    vec3 vb = normalize(vec3(size.y, s12-s10, -size.x));
    Result = cross(va, vb);
}
