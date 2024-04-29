
layout(location = 0) out lowp vec4 Result;

// fwidth() is not supported by default on OpenGL ES. Enable it.
#if defined(GL_OES_standard_derivatives)
  #extension GL_OES_standard_derivatives : enable
#endif

#define fixed lowp float

uniform lowp sampler2D atlas;
in mediump vec2 texCoord;

fixed EaseOut(fixed x) { 
    fixed r = 1.0f - x;
    return 1.0f - (r * r); 
}

fixed contour(fixed dist, fixed edge, fixed width) {
  return clamp(smoothstep(edge - width, edge + width, dist), 0.0, 1.0);
}

void main() 
{
    fixed s = texture(atlas, texCoord).r;
    
    fixed width = fwidth(s);
    const fixed outerEdge = 0.282;
    s = contour(s, outerEdge, width);
    // s = EaseOut(s);
    lowp vec3 rgb = vec3(s) * 0.9;
    Result = vec4(rgb, s);
}