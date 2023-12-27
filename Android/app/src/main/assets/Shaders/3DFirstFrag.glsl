
in  vec3 vnorm;
in  vec2 vtexCoord;

out vec4 fragColor;

uniform sampler2D albedo;
uniform int uOnlyColor;
uniform vec4 uColor;

void main()
{
    float ndl    = max(dot(vnorm, normalize(vec3(-0.35, 0.55, -0.1f))), 0.24);
    vec3 ambient = vec3(0.16, 0.115, 0.135) * (1.0 - ndl);
    
    vec3 color = uOnlyColor == 1 ? uColor.xyz : texture(albedo, vtexCoord).xyz;

    color = (color * ndl * 1.2) + ambient;
    fragColor = vec4(texture(albedo, vtexCoord).rgb, 1.0f);
}