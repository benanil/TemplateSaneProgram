
#pragma once

#include "Animation.hpp"
#include "Scene.hpp"

struct CharacterController
{
    Vector3f animatedPos;
    Vector3f startPos;
    int m_rootNodeIdx;
    Quaternion startRotation;
    
    Prefab* character;
    AnimationController animationController;

    Vector2f animTarget = { 0.0f, 0.0f };
    float animTime = 0.0f;
    float moveSpeed = 1.0f;

//------------------------------------------------------------------------
    void Start(Prefab* _character);

    void Update(float deltaTime);
    
    void Destroy();
};