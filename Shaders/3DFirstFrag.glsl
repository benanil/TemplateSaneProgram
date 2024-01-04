
out vec4 FragColor;

in vec3 vFragPos;
in vec2 vTexCoords;
in mat3 vTBN;

uniform sampler2D albedo;
uniform sampler2D normalMap;

uniform vec3 viewPos;
uniform vec3 lightPos;

void main()
{
    // obtain normal from normal map in range [0,1]
    vec3 normal = texture(normalMap, vTexCoords).rgb * 2.0 - 1.0;
    // transform normal vector to range [-1,1]

    normal = normalize(vTBN * normal);  // this normal is in tangent space
    // get diffuse color
    vec3 color = texture(albedo, vTexCoords).rgb;
    // ambient
    vec3 ambient = 0.13 * color;
    // diffuse
    vec3 lightDir = normalize(lightPos - vFragPos);
    float diff    = max(dot(lightDir, normal), 0.0);
    vec3 diffuse  = diff * color;
    // specular
    vec3 viewDir    = normalize(viewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec      = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    
    vec3 specular = vec3(0.2) * spec;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}