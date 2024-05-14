out vec4 Result;

// fwidth() is not supported by default on OpenGL ES. Enable it.
#if defined(GL_OES_standard_derivatives)
  #extension GL_OES_standard_derivatives : enable
#endif

uniform lowp sampler2D atlas;
in mediump vec2 texCoord;

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

float contour(float dist, float edge, float width) {
    return saturate(smoothstep(edge - width, edge + width, dist));
}

// #define OUTLINE
// #define SHADOW
void main() 
{
    float dist  = texture(atlas, texCoord).r;
    float width = fwidth(dist);
    
    vec4 textColor = vec4(1.0);
    float outerEdge = 0.5;

    float alpha = contour(dist, outerEdge, width);
    vec4 result = vec4(textColor.rgb, textColor.a * alpha);

    #if defined(OUTLINE)
        // https://github.com/suikki/sdf_text_sample/blob/master/assets/shaders/text_sdf_effects.f.glsl
        const float outlineEdgeWidth = 0.25;
        const vec4 outlineColor = vec4(0.1, 0.1, 0.45, 1.0);

        outerEdge = outerEdge - outlineEdgeWidth;
        float outlineOuterAlpha = clamp(smoothstep(outerEdge - width, outerEdge + width, dist), 0.0, 1.0);
        float outlineAlpha = outlineOuterAlpha - alpha;
        result.rgb = mix(outlineColor.rgb, result.rgb, alpha);
        result.a = max(result.a, outlineColor.a * outlineOuterAlpha);
    #endif

    #if defined(SHADOW)
        const float ShadowDist = 2.2;
        const float CharSize = 12.0 * 48.0; // CharSize * CellSize
        const float glowMin = 0.2;
        const float glowMax = 0.6;

        // https://github.com/mattdesl/gl-sprite-text/blob/master/demo/sdf/frag.glsl
        vec2 texelSize = vec2(1.0) / vec2(CharSize);
        float dist2 = texture(atlas, texCoord - texelSize * ShadowDist).r;

        vec4 glowColor = vec4(0.1, 0.1, 0.1, 1.0);
        vec4 glow = glowColor * smoothstep(glowMin, glowMax, dist2);
        result = mix(glow, result, alpha);
    #endif

    Result = result;
}


    int findPairs(vector<int>& nums, int k) 
    {
        unordered_set<int> set;
        for (int i = 0; i < nums.size(); i++)
            set.insert(i);

        int numPairs = 0;
        for (int i = 0; i < nums.size(); i++)
        {
            if (set.contains(k - nums[i]) || set.contains(nums[i] - k))
                numPairs++;
        }
        return numPairs;
    }