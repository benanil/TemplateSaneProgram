
#pragma once

#include "AssetManager.hpp"

#if AX_GAME_BUILD != 1

void EditorInit();
void EditorDestroy();
void EditorShow();

#else

inline void EditorInit(struct Prefab* prefab){}
inline void EditorDestroy(){}
inline void EditorShow(){}

#endif