
#pragma once

#include "AssetManager.hpp"

#if AX_GAME_BUILD != 1

void EditorInit(struct Prefab* prefab);
void EditorDestroy();
void EditorShow();
void EditorCastRay();

#else

inline void EditorInit(struct Prefab* prefab){}
inline void EditorDestroy(){}
inline void EditorShow(){}
inline void EditorCastRay(){}

#endif