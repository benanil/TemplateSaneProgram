
layout(location = 0) out mediump vec2 vTexCoord;

uniform ivec2 uScrSize;
uniform vec2 uPos;
uniform vec2 uSize;
uniform lowp int uInvertY;
uniform lowp int uDepth;

void main() 
{
    int quadID   = gl_VertexID / 6;
    int vertexID = gl_VertexID % 6;

    // unpack per quad data
    vec2 size = uSize;
    vec2 pos  = uPos;
    // ----    Create Vertex    ----
    vec2 vertices[6];
    vertices[0] = vec2(pos.x         , pos.y         );
    vertices[1] = vec2(pos.x         , pos.y + size.y);
    vertices[2] = vec2(pos.x + size.x, pos.y         );
    vertices[4] = vec2(pos.x + size.x, pos.y + size.y);
    vertices[5] = vertices[2]; // reuse vertex 2
    vertices[3] = vertices[1]; // reuse vertex 1 

    vec2 proj = 2.0 / vec2(uScrSize);
    vec2 translate = proj * vertices[vertexID] - 1.0;
    translate.y = -translate.y;
    gl_Position = vec4(translate, float(uDepth) / 255.0f, 1.0);

    // ----    Create UV    ----
    const mediump vec2 uvs[6] = vec2[6](
        vec2(0.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0),
        vec2(1.0, 0.0)
    );
    vec2 uv = uvs[vertexID];
    if (uInvertY == 1) uv.y = 1.0 - uv.y;
    vTexCoord = uv;
}
