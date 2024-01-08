
out vec4 FragColor;

in highp   vec3 vFragPos;
in highp   vec2 vTexCoords;
in mediump mat3 vTBN;

uniform sampler2D albedo;
uniform sampler2D normalMap;

uniform sampler2D metallicRoughnessMap;

uniform vec3 viewPos;
uniform vec3 lightPos;

uniform int hasNormalMap;

const float PI = 3.1415926535;

#ifndef __ANDROID__

// pbr code directly copied from here
// https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/6.pbr/1.2.lighting_textured/1.2.pbr.fs
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp( (x * (a * x + b)) / (x * (c * x + d) + e) , vec3(0.0), vec3(1.0) );
}

void main()
{
    // get diffuse color
    vec4 color = texture(albedo, vTexCoords);
    if (color.a < 0.001)
        discard;

    vec2 metalRoughness = texture(metallicRoughnessMap, vTexCoords).rg;
    float metallic  = metalRoughness.r;
    float roughness = metalRoughness.g;

    vec3 normal = vTBN[2];
    if (hasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        normal = texture(normalMap, vTexCoords).rgb;
        //normal.r = 1.0 - normal.r;
        normal = normal * 2.0 - 1.0;
        // transform normal vector to range [-1,1]
        normal = normalize(vTBN * normal);  // this normal is in tangent space
    }
    vec3 N = normal;
    vec3 V = normalize(viewPos - vFragPos);
    vec3 L = normalize(lightPos - vFragPos);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, color.rgb, metallic);
    vec3 H = normalize(V + L);

    vec3 lightColor = vec3(0.9, 0.80, 0.70);
    const float intensity = 1.2;
    vec3 radiance = lightColor * intensity;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;
        
    // kS is equal to Fresnel
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	  
    float NdotL = max(dot(N, L), 0.0);        

    vec3 lighting = (kD * color.rgb / PI + specular) * radiance * NdotL; 
    vec3 ambient = 0.02 * color.rgb * (1.0 - dot(normal, L));
    
    lighting += ambient;

    lighting = ACESFilm(lighting);
    // gamma correct
    lighting = pow(lighting , vec3(1.0/2.2)); 
    
    FragColor = vec4(lighting, 1.0);
}
#else // Android

void main()
{
    // get diffuse color
    vec4 color = texture(albedo, vTexCoords);
    if (color.a < 0.012)
        discard;

    // ambient
    vec3 ambient = 0.1 * color.rgb;
    vec3 normal = vTBN[2];

    // diffuse
    vec3 lightDir = normalize(lightPos - vFragPos);
    float diff    = max(dot(lightDir, normal), 0.0) ;
    vec3 diffuse  = diff * color.rgb;
    // specular
    vec3 viewDir    = normalize(viewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec      = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    
    vec3 specular = vec3(0.4) * spec;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}

#endif
