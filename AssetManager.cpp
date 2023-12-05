
/***********************************************************
*    Purpose:                                              *
*        Manages                                           *                                     
*    Author:                                               *
*        anilcangulkaya7@gmail.com github @benanil        *
***********************************************************/

#include "ASTL/Additional/GLTFParser.hpp"
#include "ASTL/Common.hpp"

// loads the scene that is GLTF, OBJ, or FBX
ParsedScene* LoadSceneExternal(const char* path)
{
    int size = 0;
    AX_NO_UNROLL while (path[size]) size++;

    const char* extension = path + (size - 4);
    ParsedScene* result = new ParsedScene();

    if (!(extension[0] ^ 'g' | extension[1] ^ 'l' | extension[2] ^ 't' | extension[3] ^ 'f'))
    {
        ParseGLTF(path, result);
    }
    else if (!(extension[1] ^ 'o' | extension[2] ^ 'b' | extension[3] ^ 'j'))
    {
        // ParseOBJ(path, result);
    }
    else if (!(extension[1] ^ 'f' | extension[2] ^ 'b' | extension[3] ^ 'x'))
    {
        return nullptr; // todo: fbx
    }
    else
    {
        return nullptr;
    }
    return nullptr;
}