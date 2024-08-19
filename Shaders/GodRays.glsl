
layout(location = 0) out float result;

in vec2 texCoord;

uniform float uIntensity;
uniform vec2 uSunPos;
uniform sampler2D uDepthMap;

#define NUM_SAMPLES 60

float EaseOut(float x) { 
    float r = 1.0f - x;
    return 1.0f - (r * r); 
}

void main()
{
    const float Exposure = 0.2;// 0.3f;
    const float Decay    = 0.96815;// 0.96815;
    const float Density  = 0.926;// 0.926;
    const float Weight   = 0.587;// 0.587;
    
    vec2 deltaTexCoord = (texCoord - uSunPos);
    deltaTexCoord *= 1.0f / float(NUM_SAMPLES) * Density;
    
    float godRaysColor = 0.0; 
    
    float illuminationDecay = 1.0f;
    vec2 mTexCoord = texCoord;
    
    for (int j = 0; j < NUM_SAMPLES; j++) 
    {     
        // Step sample location along ray.
        mTexCoord -= deltaTexCoord;
        float hasSun = float(distance(mTexCoord, uSunPos) < 0.1);
        float hasSky = float(texture(uDepthMap, mTexCoord).r > .9992);
        
        float _sample  = 0.75 * hasSun * hasSky + 0.04 * hasSky;
        
        _sample *= illuminationDecay * Weight;
        godRaysColor += _sample;
        illuminationDecay *= Decay;
    }
    
    result = godRaysColor * Exposure * uIntensity * EaseOut(texCoord.y);
    result = clamp(result, 0.0, 0.45);
}
