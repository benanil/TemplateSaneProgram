
#pragma once

#include "Renderer.hpp"
#include "../ASTL/Math/Matrix.hpp"

struct Pose
{
    vec_t translation;
    vec_t rotation;
    vec_t scale;
};

constexpr int MaxBonePoses = 128; // make 192 or 256 if we use more joints

struct AnimationController
{
    Texture matrixTex;
    int numAnimations;
    
    // animation indexes to blend coordinates
    // Given xy blend coordinates, we will blend animations.
    // in typical animation system, the diagram should be like the diagran below.
    //  #  #  #  <- DiagonalRun , ForwardRun , DiagonalRun
    //  #  #  #  <- DiagonalJog , ForwardJog , DiagonalJog
    //  #  #  #  <- DiagonalWalk, ForwardWalk, DiagonalWalk
    //  #  #  #  <- StrafeLeft  , Idle       , StrafeRight 
    int locomotionIndices[4][3];
    int locomotionIndicesInv[3][3];

    void SetAnim(int x, int y, int index)
    {
        if (y >= 0) locomotionIndices[y][x] = index;
        else        locomotionIndicesInv[Abs(y)][x] = index;
    }

    int GetAnim(int x, int y)
    {
        if (y >= 0) return locomotionIndices[y][x];
        else        return locomotionIndicesInv[Abs(y)][x];
    }
};

enum eAnimLocation_
{
    a_left, a_middle, a_right
};
typedef int eAnimLocation;

struct Prefab;

void StartAnimationSystem();

void DestroyAnimationSystem();

void CreateAnimationController(Prefab* prefab, AnimationController* animController);

void ClearAnimationController(AnimationController* animController);

// x, y has to be between -1.0 and 1.0
// normTime should be between 0 and 1
void EvaluateAnimOfPrefab(Prefab* prefab,
                          AnimationController* animController,
                          float x, 
                          float y, 
                          float xSpeed, 
                          float ySpeed);

//------------------------------------------------------------------------