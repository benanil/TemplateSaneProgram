
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
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

vec3 BRDF(vec3 normal, vec3 color, float metallic, float roughness)
{
    vec3 N = normal;
    vec3 V = normalize(viewPos - vFragPos);
    vec3 L = normalize(lightPos - vFragPos);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, color, metallic);
    vec3 H = normalize(V + L);

    vec3 lightColor = vec3(0.9, 0.80, 0.76);
    const float intensity = 1.4;
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

    vec3 lighting = (kD * color / PI + specular) * radiance * NdotL; 
    vec3 ambient = 0.02 * color * (1.0 - dot(normal, L));
    
    lighting += ambient;

    lighting = ACESFilm(lighting);
    // gamma correct
    lighting = pow(lighting , vec3(1.0/2.0)); 
    return lighting;
}

#endif // __ANDROID__ // android don't have brdf

vec3 PhongLighting(vec3 normal, vec3 color)
{
    // ambient
    vec3 ambient = 0.1 * color;
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
    return ambient + diffuse + specular;
}

void main()
{
    // get diffuse color
    vec4 color = texture(albedo, vTexCoords);
    if (color.a < 0.001)
        discard;

    vec3 normal = vTBN[2];
    vec3 lighting = vec3(0.0);
#ifndef __ANDROID__
    if (hasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        vec2  c = texture(normalMap, vTexCoords).rg * 2.0 - 1.0;
        float z = sqrt(1.0 - c.x * c.x - c.y * c.y);
        
        normal  = vec3(c, z);
        // transform normal vector to range [-1,1]
        normal  = normalize(vTBN * normal);  // this normal is in tangent space
        
        vec2 metalRoughness = texture(metallicRoughnessMap, vTexCoords).rg;
        float metallic  = metalRoughness.r;
        float roughness = metalRoughness.g;

        lighting = BRDF(normal, color.rgb, metallic, roughness);
    }
    else
#endif
        lighting = PhongLighting(normal, color.rgb);

    FragColor = vec4(lighting, 1.0);
}
