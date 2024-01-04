
out vec4 FragColor;

in vec3 vFragPos;
in vec2 vTexCoords;
in mat3 vTBN;

uniform sampler2D albedo;
uniform sampler2D normalMap;

uniform vec3 viewPos;
uniform vec3 lightPos;

uniform int hasNormalMap;

void main()
{
    // get diffuse color
    vec4 color = texture(albedo, vTexCoords);
    //if (color.a < 0.012)
    //    discard;

    // ambient
    vec3 ambient = 0.13 * color.rgb;
    vec3 normal = vTBN[2];
    if (hasNormalMap == 1)
    {
        // obtain normal from normal map in range [0,1]
        vec3 normal = texture(normalMap, vTexCoords).rgb * 2.0 - 1.0;
        // transform normal vector to range [-1,1]
        normal = normalize(vTBN * normal);  // this normal is in tangent space
    }

    // diffuse
    vec3 lightDir = normalize(lightPos - vFragPos);
    float diff    = max(dot(lightDir, normal), 0.0);
    vec3 diffuse  = diff * color.rgb;
    // specular
    vec3 viewDir    = normalize(viewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec      = pow(max(dot(normal, halfwayDir), 0.0), 64.0);

    vec3 specular = vec3(0.2) * spec;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}