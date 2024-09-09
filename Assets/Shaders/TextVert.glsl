
// per character texture
// x = half2:size, y = character: uint8, depth: uint8, scale: half
uniform highp usampler2D dataTex; 
uniform ivec2 uScrSize;
uniform int uIndexStart; // vertex index of the first text character that will be rendered 

out mediump vec2 vTexCoord;
out lowp vec4 vColor;

void main() 
{
    int quadID   = (uIndexStart + gl_VertexID) / 6;
    int vertexID = (uIndexStart + gl_VertexID) % 6;

    // read per quad data
    highp uvec4 data = texelFetch(dataTex, ivec2(quadID, 0), 0);
    // unpack per quad data
    lowp uint depth     = (data.y >> 8) & 0xFFu; // unused for now
    lowp uint character = data.y & 0xFFu; // corresponds to ascii character, used for atlas indexing
    vec4  sizePos = vec4((data.xxww >> uvec4(0u, 16u, 0u, 16u)) & 0xFFFFu) / 10.0f;
    vec2  size   = sizePos.xy;
    vec2  pos    = sizePos.zw;

    float scale = unpackHalf2x16(data.y).y;
    vColor = unpackUnorm4x8(data.z);

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

    const float CellSize   = 12.0;
    const float cellWidth  = 1.0 / CellSize;
    const float cellHeight = 1.0 / CellSize;

    float u = float(character % 12u) / CellSize;
    float v = float(character / 12u) / CellSize;
    
    float CharSize = 48.0 * scale;
    float us = size.x / CharSize;
    float vs = size.y / CharSize;
    mediump vec2 uv = uvs[vertexID];
    // Calculate the UV coordinates within the character cell
    uv = vec2(u + (uv.x * cellWidth * us), v + (uv.y * cellHeight * vs));
    vTexCoord = uv;
}
