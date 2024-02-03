
out vec4 FragColor;
in vec2 texCoord;

uniform sampler2D tex;
uniform sampler2D aoTex; // < ambient occlusion

const float gamma = 2.2;

// https://mini.gmshaders.com/p/gm-shaders-mini-fxaa

//Maximum texel span
#define SPAN_MAX   (8.0)
//These are more technnical and probably don't need changing:
//Minimum "dir" reciprocal
#define REDUCE_MIN (1.0/128.0)
//Luma multiplier for "dir" reciprocal
#define REDUCE_MUL (1.0/8.0)

vec3 fxaa(vec2 uv)
{
    #ifdef __ANDROID__
    return texture(tex, uv).rgb;
    #else
    vec2 u_texel = vec2(1.0, 1.0) / vec2(textureSize(tex, 0));
	//Sample center and 4 corners
    vec3 rgbCC = texture(tex, uv).rgb;
    vec3 rgb00 = texture(tex, uv + (vec2(-1.0, -1.0) * u_texel)).rgb;
    vec3 rgb10 = texture(tex, uv + (vec2(+1.0, -1.0) * u_texel)).rgb;
    vec3 rgb01 = texture(tex, uv + (vec2(-1.0, +1.0) * u_texel)).rgb;
    vec3 rgb11 = texture(tex, uv + (vec2(+1.0, +1.0) * u_texel)).rgb;
	
	//Luma coefficients
    const vec3 luma = vec3(0.299, 0.587, 0.114);
	//Get luma from the 5 samples
    float lumaCC = dot(rgbCC, luma);
    float luma00 = dot(rgb00, luma);
    float luma10 = dot(rgb10, luma);
    float luma01 = dot(rgb01, luma);
    float luma11 = dot(rgb11, luma);
	
	//Compute gradient from luma values
    vec2 dir = vec2((luma01 + luma11) - (luma00 + luma10), (luma00 + luma01) - (luma10 + luma11));
	//Diminish dir length based on total luma
    float dirReduce = max((luma00 + luma10 + luma01 + luma11) * REDUCE_MUL, REDUCE_MIN);
	//Divide dir by the distance to nearest edge plus dirReduce
    float rcpDir = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	//Multiply by reciprocal and limit to pixel span
    dir = clamp(dir * rcpDir, -SPAN_MAX, SPAN_MAX) * u_texel.xy;
	
	//Average middle texels along dir line
    vec4 A = 0.5 * (
      texture(tex, uv - dir * (1.0/6.0)) +
      texture(tex, uv + dir * (1.0/6.0)));
	
	//Average with outer texels along dir line
    vec4 B = A * 0.5 + 0.25 * (
      texture(tex, uv - dir * (0.5)) +
      texture(tex, uv + dir * (0.5)));
		
	//Get lowest and highest luma values
    float lumaMin = min(lumaCC, min(min(luma00, luma10), min(luma01, luma11)));
    float lumaMax = max(lumaCC, max(max(luma00, luma10), max(luma01, luma11)));
    
	//Get average luma
	float lumaB = dot(B.rgb, luma);
	//If the average is outside the luma range, using the middle average
    return ((lumaB < lumaMin) || (lumaB > lumaMax)) ? A.rgb : B.rgb;
    #endif
}

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

#ifdef __ANDROID__
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
    #ifndef __ANDROID__
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
    vec3 color = fxaa(texCoord);
    FragColor = CustomToneMapping(color * ao).xyzz * Vignette(texCoord) ;
    // FragColor = vec4(ao);
}