
// per character textures
uniform highp  sampler2D posTex;  // vec2 fp32
uniform highp  sampler2D sizeTex; // vec2 fp16
uniform lowp  usampler2D charTex; // uint8

uniform ivec2 uScrSize;
uniform float uScale;

out mediump vec2 texCoord;

mat4 OrthoRH(float right, float top)
{
    mat4 res = mat4(0.0);
    res[0][0] = 2.0 / right;
    res[1][1] = 2.0 / top;
    res[2][2] = -0.0033333333;// -2.0 / (zFar);//
    res[3] = vec4(-1.0f, -1.0f, -1.0f, 1.0);
    return res;
}

void main() 
{
    int quadID   = gl_VertexID / 6;
    int vertexID = gl_VertexID % 6;

    // ----    Create Vertex    ----
    vec2 pos  = texelFetch(posTex , ivec2(quadID, 0), 0).rg;
    vec2 size = texelFetch(sizeTex, ivec2(quadID, 0), 0).rg;
    float uScrHeight = float(uScrSize.y);

    vec2 vertices[6];
    vertices[0] = vec2(pos.x         , uScrHeight - (pos.y         ));
    vertices[1] = vec2(pos.x         , uScrHeight - (pos.y + size.y));
    vertices[2] = vec2(pos.x + size.x, uScrHeight - (pos.y         ));
    vertices[4] = vec2(pos.x + size.x, uScrHeight - (pos.y + size.y));
    vertices[5] = vertices[2]; // reuse vertex 2
    vertices[3] = vertices[1]; // reuse vertex 1 

    mat4 proj = OrthoRH(float(uScrSize.x), uScrHeight);
    gl_Position = proj * vec4(vertices[vertexID], 0.0, 1.0);

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

    lowp uint character = texelFetch(charTex, ivec2(quadID, 0), 0).r;
    float u = float(character % 12u) / CellSize;
    float v = float(character / 12u) / CellSize;
    
    float CharSize = 48.0 * uScale;
    float us = size.x / CharSize;
    float vs = size.y / CharSize;
    mediump vec2 uv = uvs[vertexID];
    // Calculate the UV coordinates within the character cell
    uv = vec2(u + (uv.x * cellWidth * us), v + (uv.y * cellHeight * vs));
    texCoord = uv;
}
