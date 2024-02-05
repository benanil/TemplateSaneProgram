
out vec4 FragColor;
in vec2 texCoord;

uniform sampler2D tex;
uniform sampler2D aoTex; // < ambient occlusion


vec3 GammaCorrect(vec3 x)
{
    return pow(x, vec3(1.0 / gamma));
}

// https://www.shadertoy.com/view/WdjSW3
vec3 CustomToneMapping(vec3 x)
{
#ifdef __ANDROID__
    return GammaCorrect(x / (1.0 + x)); // reinhard
    return x / (x + 0.155) * 1.019; // < doesn't require gamma correction
#else
    return GammaCorrect(x / (1.0 + x)); // reinhard
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return GammaCorrect((x * (a * x + b)) / (x * (c * x + d) + e));
#endif
}

// https://www.shadertoy.com/view/lsKSWR
float Vignette(vec2 uv)
{
	uv *= vec2(1.0f) - uv.yx;   // vec2(1.0)- uv.yx; -> 1.-u.yx; Thanks FabriceNeyret !
	float vig = uv.x * uv.y * 15.0f; // multiply with sth for intensity
	vig = pow(vig, 0.15f); // change pow for modifying the extend of the  vignette
	return vig; 
}

#ifndef __ANDROID__
// gaussian blur
float Blur5(float a, float b, float c, float d, float e) 
{
    const float Weights5[3] = float[3](6.0f / 16.0f, 4.0f / 16.0f, 1.0f / 16.0f);
    return Weights5[0]*a + Weights5[1]*(b+c) + Weights5[2]*(d+e);
}
#endif

void main()
{
    vec2 texelSize = vec2(1.0) / vec2(textureSize(aoTex, 0));
    
    float ao = 0.0;
    #ifdef __ANDROID__
    {
        // 9x gaussian blur
        ao += textureOffset(aoTex, texCoord, ivec2(-4.0, 0)).r * 0.05;
        ao += textureOffset(aoTex, texCoord, ivec2(-3.0, 0)).r * 0.09;
        ao += textureOffset(aoTex, texCoord, ivec2(-2.0, 0)).r * 0.12;
        ao += textureOffset(aoTex, texCoord, ivec2(-1.0, 0)).r * 0.15;
        ao += textureOffset(aoTex, texCoord, ivec2(+0.0, 0)).r * 0.16;
        ao += textureOffset(aoTex, texCoord, ivec2(+1.0, 0)).r * 0.15;
        ao += textureOffset(aoTex, texCoord, ivec2(+2.0, 0)).r * 0.12;
        ao += textureOffset(aoTex, texCoord, ivec2(+3.0, 0)).r * 0.09;
        ao += textureOffset(aoTex, texCoord, ivec2(+4.0, 0)).r * 0.05;
    }
    #else
    {
        ao = Blur5(texture(aoTex, texCoord).r,
                   textureOffset(aoTex, texCoord, ivec2(-1, 0)).r,
                   textureOffset(aoTex, texCoord, ivec2( 1, 0)).r,
                   textureOffset(aoTex, texCoord, ivec2(-2, 0)).r,
                   textureOffset(aoTex, texCoord, ivec2( 2, 0)).r);
    }
    #endif
    vec3 color = texture(tex, texCoord);
    ao = 1.0;
    FragColor = CustomToneMapping(color * ao).xyzz * Vignette(texCoord);
    // FragColor = vec4(ao);
}