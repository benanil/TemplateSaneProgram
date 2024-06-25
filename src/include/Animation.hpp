
#pragma once

#include "Renderer.hpp"
#include "../../ASTL/Math/Matrix.hpp"

struct Prefab;

struct Pose
{
    vec_t translation;
    vec_t rotation;
    vec_t scale;
};

enum eAnimLocation_
{
    a_left, a_middle, a_right
};
typedef int eAnimLocation;

constexpr int MaxBonePoses = 128; // make 192 or 256 if we use more joints

struct AnimationController
{
    Texture mMatrixTex;
    int mRootNodeIndex;

    // animation indexes to blend coordinates
    // Given xy blend coordinates, we will blend animations.
    // in typical animation system, the diagram should be like the diagran below.
    //  #  #  #  <- DiagonalRun , ForwardRun , DiagonalRun
    //  #  #  #  <- DiagonalJog , ForwardJog , DiagonalJog
    //  #  #  #  <- DiagonalWalk, ForwardWalk, DiagonalWalk
    //  #  #  #  <- StrafeLeft  , Idle       , StrafeRight 
    int locomotionIndices   [4][3];
    int locomotionIndicesInv[3][3];
    
    void SetAnim(int x, int y, int index)
    {
        if (y >= 0) locomotionIndices[y][x] = index;
        else        locomotionIndicesInv[Abs(y)][x] = index;
    }

    int GetAnim(int x, int y)
    {
        if (y >= 0) return locomotionIndices[y][x];
        else        return locomotionIndicesInv[Abs(y)-1][x];
    }
    
    // x, y has to be between -1.0 and 1.0 (normalized)
    // xspeed and yspeed is between 0 and infinity speed of animation
    // normTime should be between 0 and 1
    // runs the walking running etc animations from given inputs
    void EvaluateLocomotion(Prefab* prefab,
                            float x,
                            float y,
                            float xSpeed,
                            float ySpeed);

    // play the given animation, norm is the animation progress between 0.0 and 1.0
    void PlayAnim(Prefab* prefab, int index, float norm);

    // upload to gpu. internal usage only for now
    void UploadAnimationPose(Prefab* prefab, Pose* nodeMatrices);
};



// does nothing for now
[[maybe_unused]] void StartAnimationSystem();

[[maybe_unused]] void DestroyAnimationSystem();

void CreateAnimationController(Prefab* prefab, AnimationController* animController);

void ClearAnimationController(AnimationController* animController);

int FindRootNodeIndex(Prefab* prefab);

[[maybe_unused]] void MixAnimations(Prefab* prefab, int IdxAnimA, int IdxAnimB, float normA, float normB, float blend);

//------------------------------------------------------------------------