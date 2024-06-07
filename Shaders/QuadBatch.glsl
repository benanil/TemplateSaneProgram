
// per character textures
uniform highp usampler2D dataTex; // ivec2 uint32 = half2:size, rgba8:color

uniform ivec2 uScrSize;
uniform vec2 uScale;

out mediump vec2 vTexCoord;
out lowp vec4 vColor;

void main() 
{
    int quadID   = gl_VertexID / 6;
    int vertexID = gl_VertexID % 6;

    // unpack per quad data
    highp uvec4 data = texelFetch(dataTex, ivec2(quadID, 0), 0);
    lowp uint depth = data.z & 0xFFu;
    vec2 size = vec2(float(data.x >> 0u  & 0xFFFFu),
                     float(data.x >> 16u & 0xFFFFu)) * uScale;
    vColor = unpackUnorm4x8(data.y);
    vec2 pos  = vec2(float((data.w >> 0) & 0xFFFFu),
                     float((data.w >> 16) & 0xFFFFu));
    // ----    Create Vertex    ----
    vec2 scrSize = vec2(uScrSize);
    vec2 vertices[6];
    vertices[0] = vec2(pos.x         , scrSize.y - (pos.y         ));
    vertices[1] = vec2(pos.x         , scrSize.y - (pos.y + size.y));
    vertices[2] = vec2(pos.x + size.x, scrSize.y - (pos.y         ));
    vertices[4] = vec2(pos.x + size.x, scrSize.y - (pos.y + size.y));
    vertices[5] = vertices[2]; // reuse vertex 2
    vertices[3] = vertices[1]; // reuse vertex 1 

    vec2 proj = 2.0 / scrSize;
    vec2 translate = proj * vertices[vertexID] - 1.0;
    gl_Position = vec4(translate, (float(depth) / 255.0), 1.0);

    // ----    Create UV    ----
    const mediump vec2 uvs[6] = vec2[6](
        vec2(0.0, 0.0),            
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0),
        vec2(1.0, 0.0)
    );

    vTexCoord = uvs[vertexID];
}
