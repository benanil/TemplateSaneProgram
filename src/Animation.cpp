
#include "Animation.hpp"
#include "Scene.hpp"
#include "SceneRenderer.hpp"
#include "Platform.hpp"

struct Matrix3x4f16
{
    half x[4];
    half y[4];
    half z[4];
};

const int MaxNumJoints = 256;

void StartAnimationSystem()
{ }

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

void CreateAnimationController(Prefab* prefab, AnimationController* result)
{
    ASkin& skin = prefab->skins[0];
    ASSERT(skin.numJoints < MaxNumJoints);

    result->numAnimations = prefab->numAnimations;
    result->matrixTex = rCreateTexture(skin.numJoints*3, 1, nullptr, TextureType_RGBA16F, TexFlags_RawData);
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

// x, y has to be between -1.0 and 1.0
// normTime should be between 0 and 1
void EvaluateAnimOfPrefab(Prefab* prefab, AnimationController* animController, float x, float y, float normTime)
{
    ASSERT(prefab->numNodes < 250);

    const bool hasAnimation = prefab->numSkins > 0 && animController != nullptr;
    
    if (!hasAnimation)
        return;

    y = MAX(y, 0.0f); // todo: fix negative animations
    static float yBefore = 0.0f;
    // WORKING on: take node transformations of anim0 and anim1 then compose
    // step 0 sample animations to trs arrays,
    // step 1 mix sampled trs of animations into another trs
    // step 2 send mixed trs to nodes
    const int numNodes = prefab->numNodes;
    const float xBlend = Fract(x);
    const float yBlend = Fract(y);

    const int xi = int(Floor(x)); 
    const int yi = int(Floor(y));
    
    Pose animPose0[MaxBonePoses], animPose1[MaxBonePoses];

    AAnimation* animation0 = nullptr;
    AAnimation* animation1 = nullptr;
    
    bool sampleYAnim = true; // (y > Epsilon) && AlmostEqual(y, 4.0f);

    // if (sampleYAnim)
    {
        ASSERT(yi >= 0 && y <= 3); // must be between 1 and 4
        int index = animController->locomotionIndices[yi][a_middle];
        animation0 = &prefab->animations[index];
        
        SampleAnimationPose(animPose0, animation0, normTime, prefab->nodes, numNodes);

        if (yi != 3 && yBlend > 0.00001f) 
        {
            index = animController->locomotionIndices[yi + 1][a_middle];
            animation1 = &prefab->animations[index];
            
            SampleAnimationPose(animPose1, animation1, normTime, prefab->nodes, numNodes);
            MergeAnims(animPose0, animPose1, EaseOut(yBlend), numNodes);
        }
    }
    // todo: merge with x animation

    InitNodes(prefab->nodes, animPose0, numNodes);

    int skeletonNode = 0;
    if (prefab->numScenes > 0) {
        AScene defaultScene = prefab->scenes[prefab->defaultSceneIndex];
        skeletonNode = defaultScene.nodes[0];
    }

    Matrix4 nodeMatrices[MaxNumJoints];
    Matrix3x4f16 outMatrices[MaxNumJoints];
    
    ASkin& skin = prefab->skins[0];
    int rootNodeIdx = skin.skeleton != -1 ? skin.skeleton : skeletonNode; ASSERT(rootNodeIdx < MaxNumJoints);
    ANode* rootNode = prefab->GetNodePtr(rootNodeIdx);

    Matrix4 rootMatrix = Matrix4::PositionRotationScale(rootNode->translation, rootNode->rotation, rootNode->scale);
    nodeMatrices[rootNodeIdx] = rootMatrix;
    RecurseNodeMatrices(rootNode, prefab->nodes, rootMatrix, nodeMatrices);
    
    Matrix4* invMatrices = (Matrix4*)skin.inverseBindMatrices;
    
    for (int i = 0; i < skin.numJoints; i++)
    {
        Matrix4 mat = invMatrices[i] * nodeMatrices[skin.joints[i]];
        mat = Matrix4::Transpose(mat);
        ConvertFloatToHalf(outMatrices[i].x, (float*)&mat.r[0], 4);
        ConvertFloatToHalf(outMatrices[i].y, (float*)&mat.r[1], 4);
        ConvertFloatToHalf(outMatrices[i].z, (float*)&mat.r[2], 4);
    }
    
    // upload anim matrix texture to the GPU
    rUpdateTexture(animController->matrixTex, outMatrices);
}

void ClearAnimationController(AnimationController* animSystem)
{
    rDeleteTexture(animSystem->matrixTex);
}

void DestroyAnimationSystem()
{ }

