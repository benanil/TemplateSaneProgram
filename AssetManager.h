//
// Created by Administrator on 9/2/2023.
//

#ifndef DEFANCEGAME_ASSETMANAGER_H
#define DEFANCEGAME_ASSETMANAGER_H

#include "Renderer.h"

Mesh AssetManagerGetMesh(const char* path);

Texture AssetManagerGetTexture(const char* path);

void DestroyAssetManager();

void InitAssetManager();

#endif //DEFANCEGAME_ASSETMANAGER_H
