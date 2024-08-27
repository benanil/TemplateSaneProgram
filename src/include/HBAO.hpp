
#pragma once

struct Texture;
struct CameraBase;

void HBAOInit(int width, int height);
void HBAOResize(int width, int height);

void HBAOLinearizeDepth(Texture* depthTex, float near, float far); // < first call this
void HBAORender(CameraBase* camera, Texture* depthTex, Texture* normalTex); // then call this

void HBAOEdit();
void HBAODestroy();

Texture* HBAOGetResult();

Texture* HBAOGetLinearDepth();


