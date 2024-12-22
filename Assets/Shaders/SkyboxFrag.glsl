
// adopted version of this:
// https://www.shadertoy.com/view/wslyWs
// sunrise, nightfall colors added, maked sun more visible

layout(location = 0) out vec4 color;
in vec3 vPos;

uniform vec3 fsun;
uniform float time;
uniform sampler2D noiseTex;

float noise(in vec2 uv)
{
    vec2 i = floor(uv) / 64.0;
    vec2 f = fract(uv);
    f = f * f * (3. - 2. * f);
    
    float lb = textureOffset(noiseTex, i, ivec2(0, 0)).r;
    float rb = textureOffset(noiseTex, i, ivec2(1, 0)).r;
    float lt = textureOffset(noiseTex, i, ivec2(0, 1)).r;
    float rt = textureOffset(noiseTex, i, ivec2(1, 1)).r;
    
    return mix(mix(lb, rb, f.x), 
               mix(lt, rt, f.x), f.y);
}

#ifdef ANDROID
#define OCTAVES 3
#else
#define OCTAVES 2
#endif
float fbm(in vec2 uv)
{
    float value = 0.;
    float amplitude = .5;
    
    for (int i = 0; i < OCTAVES; i++)
    {
        value += noise(uv) * amplitude;
        amplitude *= .5;
        uv *= 2.;
    }
    #ifdef ANDROID
    value *= 1.15;
    #endif
    return value;
}

float sqr(float x) { return x * x; }

float easeout(float x) { 
    float r = 1.0f - x;
    return 1.0f - (r * r); 
}

void main()
{
    const float SC = 1e5;

    vec3 rd = normalize(vPos); // ray direction
    // Calculate sky plane
    float dist = (SC) / rd.y; 
    vec2 p = (dist * rd).xz;
    p *= 1.2 / SC;
    
    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    vec3 lightDir = fsun; 
    float sundot  = clamp(dot(rd, lightDir), 0.0, 1.0);
    float invYSqr = sqr(1.0 - max(rd.y, 0.0));

    vec3 cloudCol = vec3(1.);
    // vec3 skyCol = vec3(.6, .71, .85) - rd.y * .2 * vec3(1., .5, 1.) + .15 * .5;
    vec3 dayCol  = vec3(0.3, 0.5, 0.85);
    vec3 riseCol = vec3(0.65, 0.2, 0.0);
    vec3 skyCol  = mix(riseCol, dayCol, max(easeout(lightDir.y), 0.0));

    skyCol = skyCol - rd.y * rd.y * 0.5;
    skyCol = mix(skyCol, 0.85 * skyCol * vec3(1.2, 1.1, 1.0), invYSqr);
    
    // sun
    vec3 sun = 0.25 * vec3(1.0, 0.7, 0.4) * (sundot * sundot * sundot);
    sun += 0.25 * vec3(1.0, 0.8, 0.6) * pow(sundot, 64.0);
    sun += 0.2 * vec3(1.0, 0.8, 0.6) * pow(sundot, 512.0);
    sundot *= 1000.0;
    if (sundot > 999.0) sun *= 1.0 + min(sqr(sundot-999.0), 1.0);// make sun more visible
    skyCol += sun;
    
    #ifndef ANDROID
    // clouds
    float t = time * 0.1 + 10.0;
    float den = fbm(vec2(p.x - t, p.y - t));
    skyCol = mix(skyCol, cloudCol, smoothstep(.4, .8, den));
    #endif
    
    // horizon
    skyCol = mix(skyCol, 0.68 * vec3(.418, .394, .372), invYSqr);
    color.rgb = skyCol;
}

