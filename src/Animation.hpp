
#pragma once

#include "Renderer.hpp"
#include "../ASTL/Math/Matrix.hpp"

struct Pose
{
    vec_t translation;
    vec_t rotation;
    vec_t scale;
};

constexpr int MaxBonePoses = 192;

struct AnimationController
{
    Texture matrixTex;
    int numAnimations;
    
    // animation indexes to blend coordinates
    // Given xy blend coordinates, we will blend animations.
    // in typical animation system, the diagram should be like the diagran below.
    // #  #  #  #  #   <- DiagonalRun , .., ForwardRun , ..., DiagonalRun
    // #  #  #  #  #   <- DiagonalJog , .., ForwardJog , ..., DiagonalJog
    // #  #  #  #  #   <- DiagonalWalk, .., ForwardWalk, ..., DiagonalWalk
    // #  #  #  #  #   <- ............, .., Idle       , ..., ...........  
    int locomotionIndices[4][5];
    int locomotionIndicesInv[4][5];

    void SetAnim(int x, int y, int index)
    {
        if (y >= 0) locomotionIndices[y][x] = index;
        else        locomotionIndicesInv[Abs(y)][x] = index;
    }
};


enum eAnimLocation
{
    a_left_most, a_left, a_middle, a_right, a_right_most
};

struct Prefab;

void StartAnimationSystem();

void DestroyAnimationSystem();

void CreateAnimationController(Prefab* prefab, AnimationController* animController);

void ClearAnimationController(AnimationController* animController);

// x, y has to be between -1.0 and 1.0
// normTime should be between 0 and 1
void EvaluateAnimOfPrefab(Prefab* prefab, AnimationController* animController, float x, float y, float normTime);