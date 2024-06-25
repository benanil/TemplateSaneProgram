
#include "include/Animation.hpp"
#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/Platform.hpp"
#include "../ASTL/String.hpp" // StrCmp16

struct Matrix3x4f16
{
    half x[4];
    half y[4];
    half z[4];
};

const int MaxNumJoints = 256;

void StartAnimationSystem()
{ }

int FindRootNodeIndex(Prefab* prefab)
{
    if (prefab->skins == nullptr)
        return 0;

    ASkin skin = prefab->skins[0];
    if (skin.skeleton != -1) 
        return skin.skeleton;
    
    // search for Armature name, and also record the node that has most children
    int armatureIdx = -1;
    int maxChilds   = 0;
    int maxChildIdx = 0;
    // maybe recurse to find max children
    for (int i = 0; i < prefab->numNodes; i++)
    {
        if (StrCMP16(prefab->nodes[i].name, "Armature")) {
            armatureIdx = i;
            break;
        }
    
        int numChildren = prefab->nodes[i].numChildren;
        if (numChildren > maxChilds) {
            maxChilds = numChildren;
            maxChildIdx = i;
        }
    }
    
    int skeletonNode = armatureIdx != -1 ? armatureIdx : maxChildIdx;
    return skeletonNode;
}

void CreateAnimationController(Prefab* prefab, AnimationController* result)
{
    ASkin& skin = prefab->skins[0];
    result->mMatrixTex = rCreateTexture(skin.numJoints*3, 1, nullptr, TextureType_RGBA16F, TexFlags_RawData);
    result->mRootNodeIndex = FindRootNodeIndex(prefab);
    
    ASSERT(skin.numJoints < MaxNumJoints);
    ASSERT(result->mRootNodeIndex < MaxNumJoints);
    ASSERT(prefab->GetNodePtr(result->mRootNodeIndex)->numChildren > 0); // root node has to have children nodes
}

static void RecurseNodeMatrices(ANode* node, ANode* nodes, Matrix4 parentMatrix, Matrix4* matrices)
{
    for (int c = 0; c < node->numChildren; c++)
    {
        int childIndex = node->children[c];
        ANode* children = &nodes[childIndex];
        Matrix4 childTranslation = Matrix4::PositionRotationScale(children->translation, children->rotation, children->scale) * parentMatrix;
        matrices[childIndex] = childTranslation;

        RecurseNodeMatrices(children, nodes, childTranslation, matrices);
    }
}

static void InitNodes(ANode* nodes, Pose* pose, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        Vec3Store(nodes[i].translation, pose[i].translation);
        VecStore(nodes[i].rotation, pose[i].rotation);
        Vec3Store(nodes[i].scale, pose[i].scale);
    }
}

static void MergeAnims(Pose* pose0, Pose* pose1, float animBlend, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        pose0[i].translation = VecLerp(pose0[i].translation, pose1[i].translation, animBlend);
        pose0[i].scale       = VecLerp(pose0[i].scale, pose1[i].scale, animBlend);
        
        vec_t q = QSlerp(pose0[i].rotation, pose1[i].rotation, animBlend);
        q = QNorm(q);
        pose0[i].rotation = q;
    }
}

static void InitPose(Pose* pose, ANode* nodes, int numNodes)
{
    for (int i = 0; i < numNodes; i++)
    {
        pose[i].translation = VecLoad(nodes[i].translation);
        pose[i].rotation    = VecLoad(nodes[i].rotation);
        pose[i].scale       = VecLoad(nodes[i].scale);
    }
}

static void SampleAnimationPose(Pose* pose, AAnimation* animation, float normTime, ANode* nodes, int numNodes)
{
    InitPose(pose, nodes, numNodes);
    float realTime = normTime * animation->duration;
    
    for (int c = 0; c < animation->numChannels; c++)
    {
        AAnimChannel& channel = animation->channels[c];
        int targetNode = channel.targetNode; // prefab->GetNodePtr();
        AAnimSampler& sampler = animation->samplers[channel.sampler];
    
        // morph targets are not supported
        if (channel.targetPath == AAnimTargetPath_Weight)
            continue;
    
        int beginIdx = 0;
        while (realTime >= sampler.input[beginIdx + 1])
            beginIdx++;
    
        vec_t begin = ((vec_t*)sampler.output)[beginIdx]; 
        vec_t end   = ((vec_t*)sampler.output)[beginIdx + 1];
    
        float t = MAX(0.0f, realTime - sampler.input[beginIdx]) / MAX(sampler.input[beginIdx + 1] - sampler.input[beginIdx], 0.0001f);
    
        switch (channel.targetPath)
        {
            case AAnimTargetPath_Scale:
                pose[targetNode].scale = VecLerp(begin, end, t);
                break;
            case AAnimTargetPath_Translation:
                pose[targetNode].translation = VecLerp(begin, end, t);
                break;
            case AAnimTargetPath_Rotation:
                Quaternion rot = QSlerp(begin, end, t);
                rot = QNorm(rot);
                pose[targetNode].rotation = rot;
                break;
        };
    }
}

// send matrices to GPU
void AnimationController::UploadAnimationPose(Prefab* prefab, Pose* pose)
{
    InitNodes(prefab->nodes, pose, prefab->numNodes);

    Matrix4 nodeMatrices[MaxNumJoints];
    ANode* rootNode = prefab->GetNodePtr(mRootNodeIndex);
    Matrix4 rootMatrix = Matrix4::PositionRotationScale(rootNode->translation, rootNode->rotation, rootNode->scale);
    nodeMatrices[mRootNodeIndex] = rootMatrix;

    RecurseNodeMatrices(rootNode, prefab->nodes, rootMatrix, nodeMatrices);

    ASkin& skin = prefab->skins[0];
    Matrix4* invMatrices = (Matrix4*)skin.inverseBindMatrices;
    Matrix3x4f16 outMatrices[MaxNumJoints];

    for (int i = 0; i < skin.numJoints; i++)
    {
        Matrix4 mat = invMatrices[i] * nodeMatrices[skin.joints[i]];
        mat = Matrix4::Transpose(mat);
        ConvertFloatToHalf(outMatrices[i].x, (float*)&mat.r[0], 4);
        ConvertFloatToHalf(outMatrices[i].y, (float*)&mat.r[1], 4);
        ConvertFloatToHalf(outMatrices[i].z, (float*)&mat.r[2], 4);
    }
    
    // upload anim matrix texture to the GPU
    rUpdateTexture(mMatrixTex, outMatrices);
}

void AnimationController::PlayAnim(Prefab* prefab, int index, float norm)
{
    Pose animPose[MaxBonePoses];
    AAnimation* animation0 = &prefab->animations[index];
    SampleAnimationPose(animPose, animation0, norm, prefab->nodes, prefab->numNodes);

    UploadAnimationPose(prefab, animPose);
}

void MixAnimations(Prefab* prefab, int IdxAnimA, int IdxAnimB, float normA, float normB, float blend)
{
    const int numNodes = prefab->numNodes;
    AAnimation* animation0 = &prefab->animations[IdxAnimA];
    AAnimation* animation1 = &prefab->animations[IdxAnimB];
    Pose animPose0[MaxBonePoses], animPose1[MaxBonePoses];

    SampleAnimationPose(animPose0, animation0, normA, prefab->nodes, numNodes);
    SampleAnimationPose(animPose1, animation1, normB, prefab->nodes, numNodes);
    MergeAnims(animPose0, animPose1, EaseOut(blend), numNodes);
}

// x, y has to be between -1.0 and 1.0
// normTime should be between 0 and 1
void AnimationController::EvaluateLocomotion(Prefab* prefab, float x, float y, float xNorm, float yNorm)
{
    ASSERT(prefab->numNodes < 250);
    const bool hasAnimation = prefab->numSkins > 0;

    if (!hasAnimation)
        return;

    const bool yIsNegative = y < 0.0f;
    // step 0 sample animations to pose arrays,
    // step 1 mix sampled pose of animations into another pose
    // step 2 send mixed pose to nodes
    Pose animPose0[MaxBonePoses], animPose1[MaxBonePoses];

    AAnimation* animation0 = nullptr;
    AAnimation* animation1 = nullptr;
    
    y = Abs(y); // todo: fix negative animations
    const int numNodes = prefab->numNodes;
    const float yBlend = Fract(y);

    int yi = int(y);
    // sample y anim
    {
        ASSERT(yi <= 3); // must be between 1 and 4
        
        if (yIsNegative)
            yi = -yi;
        
        int sign = Sign(yi);
        int index = GetAnim(a_middle, yi);
        animation0 = &prefab->animations[index];
        
        SampleAnimationPose(animPose0, animation0, yNorm, prefab->nodes, numNodes);

        if (yi != 3 && yBlend > 0.00001f) 
        {
            index = GetAnim(a_middle, yi + sign);
            animation1 = &prefab->animations[index];
            
            SampleAnimationPose(animPose1, animation1, yNorm, prefab->nodes, numNodes);
            MergeAnims(animPose0, animPose1, EaseOut(yBlend), numNodes);
        }
    }

    // Todo: make better horizontal movement, 
    //       better when we are not moving vertical
    if (!AlmostEqual(x, 0.0f))
    {
        // -2,-2 range to 0, 4 range to make array indexing
        float xBlend = Abs(Fract(x));
        eAnimLocation xAnim = x < 0.0f ? a_left : a_right;
        {
            int index = GetAnim(xAnim, yi);
            animation1 = &prefab->animations[index];
            SampleAnimationPose(animPose1, animation1, xNorm, prefab->nodes, numNodes);
        }
        MergeAnims(animPose0, animPose1, EaseOut(xBlend), numNodes);
    }

    UploadAnimationPose(prefab, animPose0);
}

void ClearAnimationController(AnimationController* animSystem)
{
    rDeleteTexture(animSystem->mMatrixTex);
}

void DestroyAnimationSystem()
{ }

