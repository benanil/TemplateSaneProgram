
// per character textures
uniform highp sampler2D posTex;  // vec2 fp32
// x = half2:size, y = character: uint8, depth: uint8, scale: half
uniform highp usampler2D dataTex; 

uniform ivec2 uScrSize;

out mediump vec2 vTexCoord;
out lowp vec4 vColor;

void main() 
{
    int quadID   = gl_VertexID / 6;
    int vertexID = gl_VertexID % 6;

    // ----    Create Vertex    ----
    vec2 pos  = texelFetch(posTex , ivec2(quadID, 0), 0).rg;
   
    // read per quad data
    uvec4 data = texelFetch(dataTex, ivec2(quadID, 0), 0);
    // unpack per quad data
    lowp uint depth     = (data.y >> 8) & 0xFFu; // unused for now
    lowp uint character = data.y & 0xFFu; // corresponds to ascii character, used for atlas indexing
    float scale = unpackHalf2x16(data.y).y;
    vec2 size   = unpackHalf2x16(data.x);
    vColor      = unpackUnorm4x8(data.z);

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
