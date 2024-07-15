  
// Output image
layout(location = 0) out float result;

// Input images
uniform sampler2D aoSource;
uniform sampler2D texLinearDepth;

// Input uniforms
// Define the uniform buffer
const float Sharpness = 40.0;
const float KERNEL_RADIUS = 3.0;

//-------------------------------------------------------------------------
float BlurFunction(ivec2 uv, float r, float center_c, float center_d, inout float w_total) 
{
    float c = texelFetch(aoSource, uv, 0).x;
    float d = texelFetch(texLinearDepth, uv, 0).x;
  
    const float BlurSigma = float(KERNEL_RADIUS) * 0.5;
    const float BlurFalloff = 1.0 / (2.0 * BlurSigma * BlurSigma);
  
    float ddiff = (d - center_d) * Sharpness;
    float w = exp2(-r * r * BlurFalloff - ddiff * ddiff);
    w_total += w;
    return c * w;
}

void main() 
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
  
    float center_c = texelFetch(aoSource, coord, 0).x;
    float center_d = texelFetch(texLinearDepth, coord, 0).x;
    float c_total = center_c;
    float w_total = 1.0;

    coord.x -= 3;

    c_total += BlurFunction(coord, 3.0f, center_c, center_d, w_total); coord.x += 1;
    c_total += BlurFunction(coord, 2.0f, center_c, center_d, w_total); coord.x += 1;
    c_total += BlurFunction(coord, 1.0f, center_c, center_d, w_total); coord.x += 1;
    
    coord.x++;

    c_total += BlurFunction(coord, 1.0f, center_c, center_d, w_total); coord.x += 1;
    c_total += BlurFunction(coord, 2.0f, center_c, center_d, w_total); coord.x += 1;
    c_total += BlurFunction(coord, 3.0f, center_c, center_d, w_total); coord.x += 1;
    
    result = c_total / w_total;
}