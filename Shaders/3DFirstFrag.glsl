
out vec4 FragColor;

in highp   vec3 vFragPos;
in mediump vec2 vTexCoords;
in mediump mat3 vTBN;

uniform sampler2D albedo;
uniform sampler2D normalMap;

uniform vec3 viewPos;
uniform vec3 lightPos;

uniform int hasNormalMap;

void main()
{
    // get diffuse color
    vec4 color = texture(albedo, vTexCoords);
    if (color.a < 0.012)
        discard;

    // ambient
    vec3 ambient = 0.1 * color.rgb;
    vec3 normal = vTBN[2];
    if (hasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        normal = texture(normalMap, vTexCoords).rgb;
        // normal.g = 1.0 - normal.g;
        normal = normal * 2.0 - 1.0;
        // transform normal vector to range [-1,1]
        normal = normalize(vTBN * normal);  // this normal is in tangent space
    }

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