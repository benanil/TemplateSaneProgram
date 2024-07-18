
layout(location = 0) out vec4 result;

in vec2 texCoord;

uniform sampler2D uLightingTex;
uniform sampler2D uGodRays;

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// https://www.shadertoy.com/view/WdjSW3
// equivalent to reinhard tone mapping but no need to gamma correction
vec3 CustomToneMapping(vec3 x)
{
    #ifdef __ANDROID__
    // reinhard approximation no need gamma correction
    x += 0.0125;
    return x / (x + 0.15) * 0.88;
    #else
    return pow(ACESFilm(x), vec3(1.0 / 2.2));
    #endif
}

// https://www.shadertoy.com/view/lsKSWR
float Vignette(vec2 uv)
{
    uv *= vec2(1.0) - uv.yx;   // vec2(1.0)- uv.yx; -> 1.-u.yx; Thanks FabriceNeyret !
    float vig = uv.x * uv.y * 15.0; // multiply with sth for intensity
    vig = pow(vig, 0.15); // change pow for modifying the extend of the  vignette
    return vig; 
}
void main()
{
    vec3 godRays = vec3(0.8, 0.65, 0.58) * texture(uGodRays, texCoord).r;
    result.rgb  = CustomToneMapping(texture2D(uLightingTex, texCoord).rgb) * Vignette(texCoord);
    result.rgb += godRays;
}