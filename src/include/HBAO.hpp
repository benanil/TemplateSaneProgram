
#pragma once

struct Texture;
struct CameraBase;

void HBAOInit(int width, int height);
void HBAOResize(int width, int height);
void HBAORender(CameraBase* camera, Texture* depthTex, Texture* normalTex);
void HBAOEdit(Vector2f pos, int* currElement, float textPadding);
void HBAODestroy();

Texture* HBAOGetResult();



