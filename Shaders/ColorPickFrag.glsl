out vec4 Result;

in mediump vec2 vTexCoord;

uniform vec3 uHSV;
uniform vec2 uSize;

vec3 hsv2rgb(vec3 c)
{
    c.z = 1.0 - c.z;
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 hue2rgb(float h) {
    float r = abs(h * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(h * 6.0 - 2.0);
    float b = 2.0 - abs(h * 6.0 - 4.0);
    return clamp(vec3(r, g, b), 0.0, 1.0);
}

void main() 
{
    vec2 uv = vTexCoord;
    vec3 hsv = uHSV;
    Result = vec4(1.0 - vTexCoord.y);

    if (vTexCoord.y > 0.88) {
        Result.xyz = hue2rgb(uv.x);
    }
    else if (uv.x < 0.88) {  
        Result.xyz = hsv2rgb(vec3(hsv.x, uv));

        vec2 aspect = uSize / vec2(max(uSize.x, uSize.y));
        uv *= 0.88;
        uv *= aspect;
        hsv.z = 1.0 - hsv.z;
        hsv.z *= 0.8 * aspect.y;
        hsv.y *= .78 * aspect.x;
        float dst = distance(uv, hsv.yz);
        if (dst < 0.025) {
            float intensity = 1.0f - (dst * 40.0); // 40 is 1/0.025
            Result = mix(Result, vec4(hsv.y), intensity);
        }
    }

    Result.w = 1.0f;
}