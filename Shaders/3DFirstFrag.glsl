
in  vec3 vnorm;
in  vec2 vtexCoord;

out vec4 fragColor;

uniform sampler2D albedo;

void main()
{
    float ndl = max(dot(vnorm, normalize(vec3(-0.35, 0.55, -0.1f))), 0.24);
    vec3 ambient = vec3(0.16, 0.115, 0.135) * (1.0 - ndl);
    vec3 color = (texture(albedo, vtexCoord).xyz * ndl * 1.24) + ambient;
    fragColor = vec4(color, 1.0f);
}