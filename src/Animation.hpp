
#pragma once

#include "Renderer.hpp"
#include "../ASTL/Math/Matrix.hpp"

struct AnimTexture
{
    Texture matrixTex;
    int numFrames;
};

struct AnimationController
{
    AnimTexture* animTextures;
    Texture jointComputeOutTex; // compute shader will output to this. joint matrix texture
    int numAnimations;
};

struct Prefab;

void StartAnimationSystem();

void DestroyAnimationSystem();

void CreateAnimationController(Prefab* prefab, AnimationController* result);

void ClearAnimationController(AnimationController* animSystem);

void EvaluateAnimOfPrefab(Prefab* prefab, int animIndex, double animTime, AnimationController* animSystem);