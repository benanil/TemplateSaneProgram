
#include "include/Editor.hpp"

#include "../ASTL/HashMap.hpp"
#include "../ASTL/IO.hpp"
#include "../ASTL/String.hpp"
#include "../ASTL/Queue.hpp"

#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/Camera.hpp"
#include "include/UI.hpp"
#include "include/BVH.hpp"
#include "include/TLAS.hpp"

#if AX_GAME_BUILD != 1

enum FileType_
{
    FileType_CPP,
    FileType_File,
    FileType_Folder,
    FileType_HLSL,
    FileType_HPP,
    FileType_Material,
    FileType_Audio,
    FileType_Mesh,
    FileType_Texture,
    FileType_NumFileTypes
};
typedef uint FileType;

// SaneProgram.cpp
extern PrefabID MainScenePrefab;
extern PrefabID SpherePrefab;

static bool* isNodeOpenArray = 0;

int SelectedNodeIndex = 0;
int SelectedNodePrimitiveIndex = 0;

static StackHashMap<uint32_t, int, 32, NoHasher<uint32_t>> mFolderOpenMap = {};

static char mCurrentFolder[512] = "Assets";

static Texture mFileIcons[FileType_NumFileTypes];
static Texture mSearchIcon;

static float mTreeWidth = 300.0f;
static bool mTreeWidthDragging = false;

static const float mBoxSize = 70.0f;
static const float mBoxPadding = 16.0f;

static Vector2f mBoxStartPos;
static int mSelectedElementIndex = -1;
static int mCurrElementIdx = 0;
static UWindow* mResWin = nullptr;

//------------------------------------------------------------------------
// Searching
static Array<const char*> mSearchResults={};
static FixedSizeGrowableAllocator<char> mSearchTextBuffer(2048);
static const char* mSearchWord;
static bool mSearching = false;

constexpr uint PrefabViewWindowHash = StringToHash("Prefab View");

static const char* CopyString(const char* str)
{
    int len = StringLength(str);
    char* alloc = mSearchTextBuffer.AllocateUninitialized(len + 16);
    SmallMemCpy(alloc, str, len);
    alloc[len] = '\0'; // null terminate
    return alloc;
}

static void ResetSearchTexts()
{
    mSearchTextBuffer.current = mSearchTextBuffer.base;
    mSearchTextBuffer.currentCapacity = 2048;
    mSearchResults.m_count = 0;
}

static void SearchForElements(const char* path, void* dataUnused)
{
    if (StringContains(path, mSearchWord) != -1)
    {
        const char* pathCopy = CopyString(path);
        mSearchResults.PushBack(pathCopy);
    }

    if (IsDirectory(path)) {
        VisitFolder(path, SearchForElements, nullptr); // recurse
    }
}

void EditorInit(Prefab* prefab)
{
    isNodeOpenArray = new bool[prefab->numNodes]{};
    mFolderOpenMap[StringToHash("Assets")] = true;

    const char* FileIconFolders[] = {
        "Assets/Textures/Icons/cpp_icon.png",
        "Assets/Textures/Icons/file.png",
        "Assets/Textures/Icons/folder.png",
        "Assets/Textures/Icons/glsl.png",
        "Assets/Textures/Icons/hpp_icon.png",
        "Assets/Textures/Icons/Material_Icon.png",
        "Assets/Textures/Icons/audio_file.png",
        "Assets/Textures/Icons/mesh.png",
        "Assets/Textures/Icons/image_file.png"
    };
    mSearchIcon = rImportTexture("Assets/Textures/Icons/magnifying-glass.png");

    for (int i = 0; i < FileType_NumFileTypes; i++)
        mFileIcons[i] = rImportTexture(FileIconFolders[i]);
}

void EditorDestroy()
{ 
    for (int i = 0; i < FileType_NumFileTypes; i++)
        rDeleteTexture(mFileIcons[i]);

    rDeleteTexture(mSearchIcon);
    delete[] isNodeOpenArray;
}

static void FocusPrefabViewToSelectedNode()
{
    // stores index and parent index
    static Queue<uint64_t> queue = {};
    Prefab* mainScene = g_CurrentScene.GetPrefab(MainScenePrefab);
    
    uint64_t rootIndex = (uint64_t)mainScene->GetRootNodeIdx();
    uint64_t parent = rootIndex; // make its parent as itself
    queue.Enqueue(rootIndex | (parent << 32));
    
    uint64_t elementIndex = 1; // start from one because we use nodes tree at start
    bool isRoot = true;
    while (!queue.Empty())
    {
        uint64_t nodeData = queue.Dequeue();
        uint64_t nodeIdx = nodeData & 0xFFFFFFFFull;
        ANode* node = mainScene->nodes + nodeIdx;
        elementIndex++;
        
        uint64_t parentShifted = nodeData >> 32ull; //  & 0x00000000FFFFFFFFull;
        uint64_t nodeAsParent = nodeIdx << 32ull;
        
        if (nodeIdx == SelectedNodeIndex)
        {
            isNodeOpenArray[nodeIdx]  = true;
            int realElementIdx = (int)elementIndex + (SelectedNodePrimitiveIndex * isNodeOpenArray[parentShifted]);
            uFocusWindowToElement(uGetWindowFromHash(PrefabViewWindowHash), realElementIdx);
            isNodeOpenArray[parentShifted] = true;
            break;
        }
            
        bool shouldDive = mainScene->nodes[parentShifted].numChildren == 1;

        if (isNodeOpenArray[parentShifted] || shouldDive || isRoot)
        for (int i = 0; i < node->numChildren; i++)
        {
            queue.Enqueue((uint64_t)node->children[i] | nodeAsParent);
        }
        isRoot = false;
    }
    queue.Reset();
}

void EditorCastRay()
{
    Vector2f rayPos;
    GetMouseWindowPos(&rayPos.x, &rayPos.y); // { 1920.0f / 2.0f, 1080.0f / 2.0f };

    if (!GetMousePressed(MouseButton_Left) || uAnyWindowHovered(rayPos)) return;
    
    Prefab* sphere = g_CurrentScene.GetPrefab(SpherePrefab);
    CameraBase* camera = SceneRenderer::GetCamera();
    Scene* currentScene = &g_CurrentScene;

    Prefab* mainScene = g_CurrentScene.GetPrefab(MainScenePrefab);
    Triout rayResult = RayCastFromCamera(camera, rayPos, currentScene, MainScenePrefab, nullptr);
    
    if (rayResult.t != RayacastMissDistance)
    {
        int nodeIndex = mainScene->nodes[SelectedNodeIndex].index;
        AMesh* mesh = nullptr;

        if (nodeIndex != -1) {
            mesh = mainScene->meshes + nodeIndex;
            // remove outline of last selected object
            mesh->primitives[SelectedNodePrimitiveIndex].hasOutline = false;
        }
  
        SelectedNodeIndex = rayResult.nodeIndex;
        SelectedNodePrimitiveIndex = rayResult.primitiveIndex;

        mesh = mainScene->meshes + mainScene->nodes[SelectedNodeIndex].index;
        mesh->primitives[SelectedNodePrimitiveIndex].hasOutline = true;
        
        sphere->globalNodeTransforms[0].r[3] = rayResult.position;
        
        FocusPrefabViewToSelectedNode();
    }
    else {
        SelectedNodeIndex = 0;
        SelectedNodePrimitiveIndex = 0;
    }
}

static Vector2f PopupElementPos;
static Vector2f PopupSize = { 400.0f, 220.0f };
const float PopElementPadding = 20.0f;

static Vector2f* PopupWindowBegin(const char* windowName, const char* question)
{
    uPushFloat(uf::Depth, 0.2f);
    
    const Vector2f fullSize = { 1920.0f, 1080.0f };
    uQuad(Vector2f::Zero(), fullSize, uGetColor(uColor::Quad));
    
    Vector2f position = (fullSize * 0.5f) - (PopupSize * 0.5f);

    uQuad(position, PopupSize, uGetColor(uColor::Quad));
    uBorder(position, PopupSize);
    position.y += PopElementPadding;
    
    Vector2f textSize = uCalcTextSize(question, uTextFlags_WrapWidthDetermined);
    position += PopElementPadding;
    
    uSetFloat(uf::TextWrapWidth, PopupSize.x - (PopElementPadding * 2.0f));
    uText(windowName, position, uTextFlags_WrapWidthDetermined);
    
    uPushFloat(uf::TextScale, uGetFloat(uf::TextScale) * 0.8f);
    position.y += textSize.y + PopElementPadding;
    uText(question, position, uTextFlags_WrapWidthDetermined);
    uPopFloat(uf::TextScale);
    
    position.y += textSize.y + PopElementPadding;
    PopupElementPos = position;

    return &PopupElementPos;
}

static void PopupWindowEnd() 
{
    uPopFloat(uf::Depth);
}

//------------------------------------------------------------------------
// Prefab View

static void FocusCameraToPrimitive(int nodeIndex, int primitiveIndex)
{
    CameraBase* camera = SceneRenderer::GetCamera();
    Prefab* mainScene = g_CurrentScene.GetPrefab(MainScenePrefab);
    ANode* node = mainScene->nodes + nodeIndex;
    APrimitive* primitive = mainScene->meshes[node->index].primitives + primitiveIndex;
    Matrix4 transformation = mainScene->globalNodeTransforms[nodeIndex];
    Vector4x32f min = Vector4Transform(VecLoadA(primitive->min), transformation.r);
    Vector4x32f max = Vector4Transform(VecLoadA(primitive->max), transformation.r);
    camera->FocusToAABB(min, max);
    SelectedNodeIndex = nodeIndex;
    SelectedNodePrimitiveIndex = primitiveIndex;
}

static void ShowPrefabView(Prefab* prefab)
{
    static Queue<int> queue = {};

    static bool windowOpen = true, nodesOpen = true;
    windowOpen ^= GetKeyPressed('B');
    
    const Vector2f position = { 1436.0f, 131.0f };
    const Vector2f scale    = { 450.0f, 500.0f };

    if (uBeginWindow("Prefab View", PrefabViewWindowHash, position, scale, &windowOpen))
    {
        queue.Enqueue(prefab->GetRootNodeIdx());
        nodesOpen ^= uTreeBegin("Nodes", true, nodesOpen);
        
        if (nodesOpen) while (!queue.Empty())
        {
            int index = queue.Dequeue();
            ANode* node = prefab->nodes + index;
            AMesh* mesh = prefab->meshes + node->index;
            
            bool collapsable = mesh->numPrimitives > 0 || node->index == -1;
            bool clicked = uTreeBegin(node->name, collapsable, isNodeOpenArray[index]);
            isNodeOpenArray[index] ^= clicked;

            if (isNodeOpenArray[index] && node->index != -1)
            for (int i = 0; i < mesh->numPrimitives; i++)
            { 
                char temp[64] = "no name ";
                IntToString(temp+8, i);
                const char* name = mesh->name == nullptr ? temp : mesh->name;
                bool primitiveClicked = uTreeBegin(name, false, false);
                if (!clicked && primitiveClicked) {
                    FocusCameraToPrimitive(index, i);
                }

                uTreeEnd();
            }
        
            uTreeEnd();
        
            for (int i = 0; i < node->numChildren; i++)
            {
                queue.Enqueue(node->children[i]);
            }
        }
        
        queue.Reset();
        uTreeEnd();
        uWindowEnd();
    }
}

//------------------------------------------------------------------------
// Resources Window
static bool SetCurrentFolder(const char* path)
{
    MemsetZero(mCurrentFolder, 512);
    SmallMemCpy(mCurrentFolder, path, StringLength(path));
    return true;
}

static Texture* FolderIconFn(const char* path) 
{
    return &mFileIcons[FileType_Folder];
}

static Texture* GetIconByFileType(const char* path)
{
    const char* ext = GetFileExtension(path, StringLength(path));
    uint hash = StringToHash(ext);

    switch (hash) {
        case StringToHash("cpp"):  return &mFileIcons[FileType_CPP];
        case StringToHash("glsl"): return &mFileIcons[FileType_HLSL];
        case StringToHash("hpp"):  return &mFileIcons[FileType_HPP];
        case StringToHash("mat"):  return &mFileIcons[FileType_Material];
    
        case StringToHash("png"): case StringToHash("jpg"): case StringToHash("jpeg"):
            return &mFileIcons[FileType_Texture];
        case StringToHash("wav"): case StringToHash("ogg"): case StringToHash("mp3"):
            return &mFileIcons[FileType_Audio];
        case StringToHash("abm"): case StringToHash("gltf"):
            return &mFileIcons[FileType_Mesh];
    };
    
    return &mFileIcons[FileType_File];
}

static char mCopiedResource[512];
static bool mDeleteContentWarnPopOpen = false;

static void PasteResource(void* unused) {
    if (mCopiedResource[0] == 0 || !FileExist(mCopiedResource)) return;
    char destination[512] = {};
    CombinePaths(destination, mCurrentFolder, GetFileName(mCopiedResource));
    CopyFile(mCopiedResource, destination);
}

static void DeleteResource(void* unused) {
    if (mCopiedResource[0] == 0 || !FileExist(mCopiedResource)) return;
    mDeleteContentWarnPopOpen = true;
}

static void DeleteWarnWindow(const char* windowName, const char* question)
{
    Vector2f elementPos = *PopupWindowBegin(windowName, question);
    elementPos.y += PopElementPadding;
    elementPos.x += PopupSize.x - 100.0f - PopElementPadding;
    
    bool pressedEnter = GetKeyPressed(Key_ENTER);
    if (pressedEnter || uButton("Yes", elementPos, Vector2f::Zero(), uButtonOpt_Border))
    {
        bool isDir = IsDirectory(mCopiedResource);
        if (isDir) RemoveFolder(mCopiedResource);
        else       RemoveFile(mCopiedResource);
        mDeleteContentWarnPopOpen = false;
    }
    
    elementPos.x -= 100.0f;
    if (uButton("No", elementPos, Vector2f::Zero(), uButtonOpt_Border))
    {
        mDeleteContentWarnPopOpen = false;
    }

    PopupWindowEnd();
}

static void DrawResource(const char* path, 
                         bool isDir, 
                         float textSize,
                         Texture*(*ChoseIconFn)(const char*),
                         bool(*DoubleClickFn)(const char*))
{
    // we only want to iterate over folders or files
    if (isDir ^ IsDirectory(path)) {
        return;
    }
    
    const char* fileName = GetFileName(path);

    if (!(mResWin->elementPos.y + mResWin->topHeight < mResWin->position.y) &&
        !(mResWin->elementPos.y >= mResWin->position.y + mResWin->scale.y))
    {
        Vector2f mouseTestPos = uGetMouseTestPos();
        bool hovered = RectPointIntersect(mResWin->elementPos, Vec2(mBoxSize), mouseTestPos);
        
        if (hovered)
        {
            if (IsDoubleClick() && mSelectedElementIndex == mCurrElementIdx)
                DoubleClickFn(path);

            if (GetMousePressed(MouseButton_Left)) 
                mSelectedElementIndex = mCurrElementIdx;
            
            if (GetMousePressed(MouseButton_Right))
            {
                MemsetZero(mCopiedResource, 512);
                SmallMemCpy(mCopiedResource, path, StringLength((const char*)path));
            }
        }
    
        // border
        bool isSelected = mSelectedElementIndex == mCurrElementIdx;
        uint borderColor = uGetColor(isSelected ? uColor::SelectedBorder : uColor::Hovered);
        if (!hovered) borderColor &= 0x77FFFFFFu;
    
        uQuad(mResWin->elementPos - 5.0f, Vec2(mBoxSize + 10.0f), borderColor, 0u);
    
        uPushFloat(uf::Depth, uGetFloat(uf::Depth) * 0.9f);
    
        uSprite(mResWin->elementPos, Vec2(mBoxSize), ChoseIconFn(fileName), false);
    
        Vector2f textPos = mResWin->elementPos;
        textPos.y += mBoxSize + mResWin->elementOffsetY;
        uSetFloat(uf::TextWrapWidth, mBoxSize);
        uPushFloat(uf::TextScale, uGetFloat(uf::TextScale) * textSize);
        uText(fileName, textPos, uTextFlags_WrapImmediately);
        uPopFloat(uf::TextScale);
    
        uPopFloat(uf::Depth);
    }
    mResWin->elementPos.x += mBoxSize + mBoxPadding;
    if (mResWin->elementPos.x + mBoxSize >= mResWin->position.x + mResWin->scale.x)
    {
        float boxHeight = mBoxSize + mResWin->elementOffsetY + 25.0f;
        mResWin->elementPos.x = mBoxStartPos.x;
        mResWin->elementPos.y += boxHeight;
        mResWin->lastElementsTotalHeight += boxHeight;
    }
    
    mCurrElementIdx++;
}

static void DrawFoldersFn(const char* path, void* dataUnused)
{
    DrawResource(path, true, 0.7f, FolderIconFn, SetCurrentFolder);
}

static void DrawFilesFn(const char* path, void* dataUnused)
{
    DrawResource(path, false, 0.6f, GetIconByFileType, wOpenFile);
}

static void RecurseFolderTree(const char* path, void* unusedData)
{
    if (IsDirectory(path))
    {
        uint32_t hash = StringToHash(path);
        bool hasSubdir = HasAnySubdir(path);
        bool nodeOpen = mFolderOpenMap[hash];
        bool clicked = uTreeBegin(GetFileName(path), hasSubdir, nodeOpen, mTreeWidth);
        bool isOpen = nodeOpen ^ clicked; 
        mFolderOpenMap[hash] = isOpen;
    
        if (clicked)
            SetCurrentFolder(path);
    
        if (isOpen)
            VisitFolder(path, RecurseFolderTree, (void*)path);
    
        uTreeEnd();
    }
}

static void DrawNavigationBars(UWindow* window)
{
    // left - right icons for going back and forward
    uHorizontalTriangle(window->elementPos, window->elementOffsetY * 0.5f, -1.0f, ~0u);
    if (uClickCheck(window->elementPos, Vec2(window->elementOffsetY)) || GetMouseReleased(MouseButton_Backward))
    {
        if (mCurrentFolder[sizeof("Assets")] != 0)
            PathGoBackwards(mCurrentFolder, StringLength(mCurrentFolder), true);
    }
    window->elementPos.x += window->elementOffsetY;
        
    uHorizontalTriangle(window->elementPos, window->elementOffsetY * 0.5f, 1.0f, ~0u);
    if (uClickCheck(window->elementPos, Vec2(window->elementOffsetY)))
    {
        // path go forward is not implemented
    }
    window->elementPos.x += window->elementOffsetY;
            
    uSprite(window->elementPos, Vec2(window->elementOffsetY * 0.6f), &mSearchIcon, false);
    if (uClickCheck(window->elementPos, Vec2(window->elementOffsetY))) {
        mSearching ^= true;
    }
    window->elementPos.x += window->elementOffsetY;
    window->elementPos.y += window->elementOffsetY * 0.6f;
}

static void DrawSearch(UWindow* window, float searchWidth)
{
    static char searchText[512] = {};
    uSetFloat(uf::TextWrapWidth, searchWidth);
    uPushFloat(uf::ContentStart, searchWidth);
    if (mSearching)
    {
        uSetElementFocused(true);
        if (uTextBox("", window->elementPos, Vec2(searchWidth, window->elementOffsetY * 0.72f), searchText))
        {
            ResetSearchTexts();
            if (searchText[0] != 0) // at least one character written
            {
                mSearchWord = searchText;
                SearchForElements("Assets", nullptr);
            }
        }
    }
    else
        uText(mCurrentFolder, window->elementPos, uTextFlags_WrapImmediately | uTextFlags_NoNewLine);
            
    uPopFloat(uf::ContentStart);
}

static void OpenCurrentFolderFn(void* data) {
    wOpenFolder(mCurrentFolder);
}

static bool mCreateFolderPopOpen = false;
static bool mCreateFilePopOpen = false;
static char mCreatedResourceName[256] = {};

static void CreateResourcePopup(const char* windowName, const char* question, bool isFile)
{
    Vector2f elementPos = *PopupWindowBegin(windowName, question);
    uPushFloat(uf::ContentStart, PopupSize.x - 100.0f);
    uPushFloat(uf::SliderHeight, uGetFloat(uf::SliderHeight) * 1.2f);
    uSetElementFocused(true);
        uTextBox("Name", elementPos, Vector2f::Zero(), mCreatedResourceName);
    uPopFloat(uf::ContentStart);
    uPopFloat(uf::SliderHeight);
    
    elementPos.y += PopElementPadding;
    elementPos.x += PopupSize.x - 100.0f - PopElementPadding;
    bool pressedEnter = GetKeyPressed(Key_ENTER);
    if (pressedEnter || uButton("Create", elementPos, Vector2f::Zero(), uButtonOpt_Border))
    {
        char combined[512] = {};
        CombinePaths(combined, mCurrentFolder, mCreatedResourceName);
        if (isFile) {
            AFile file = AFileOpen(combined, AOpenFlag_WriteText);
            AFileClose(file);
        }
        else {
            CreateFolder(combined);
        }
        mCreateFolderPopOpen = mCreateFilePopOpen = false;
    }
    
    elementPos.x -= 100.0f;
    if (uButton("Cancel", elementPos, Vector2f::Zero(), uButtonOpt_Border))
    {
        mCreateFolderPopOpen = mCreateFilePopOpen = false;
    }

    PopupWindowEnd();
}

static void CreateNewFolderFn(void* data) {
    mCreateFolderPopOpen = true;
}

static void CreateNewFileFn(void* data) {
    mCreateFilePopOpen = true;
}

static void ShowResourcesWindow()
{
    static bool windowOpen = true;
    windowOpen ^= GetKeyPressed('B');
    
    constexpr uint hash = StringToHash("Resources");
    UWindow* window = uGetWindowFromHash(hash);
    mResWin = window;
    
    const Vector2f windowPos = { 366.0f, 727.0f };
    const Vector2f windowScale = { 1054.0f, 334.0f };

    const uWindowFlags windowFlags = uWindowFlags_FixedElementStart | uWindowFlags_RightClickable;
    
    if (uBeginWindow("Resources", hash, windowPos, windowScale, &windowOpen, windowFlags))
    {
        Vector2f linePos = window->elementPos;
        float lineThickness = uGetFloat(uf::LineThickness);
        float scrollWidth = uGetFloat(uf::ScrollWidth);
        linePos.x += mTreeWidth - scrollWidth;
        linePos.y -= window->elementOffsetY - lineThickness;
        linePos.y += window->scrollPercent * window->elementsTotalHeight;

        Vector2f startPos = linePos;

        if (linePos.x < window->position.x + window->scale.x)
        {
            Vector2f mouseTestPos = uGetMouseTestPos();

            float lineLength = window->scale.y - window->topHeight - lineThickness;
            uLineVertical(linePos, lineLength, 0u);
            
            float testDistance = mTreeWidthDragging ? 60.0f : 5.0f;
            bool scalingWidth = Abs(linePos.x - mouseTestPos.x) < testDistance && 
                                window->isFocused && uGetWindowState() == 0 &&
                                InRange(mouseTestPos.y, window->position.y, window->scale.y);
            
            if (scalingWidth) wSetCursor(wCursor_ResizeEW);
            
            if (scalingWidth && GetMouseDown(MouseButton_Left)) {
                mTreeWidth = Clamp(mouseTestPos.x - window->position.x, 60.0f, 600.0f);
                mTreeWidthDragging = true;
            }
            else 
                mTreeWidthDragging = false;

            linePos.y += window->elementOffsetY;
            float treeWidth = linePos.x - window->position.x;
            lineLength = window->scale.x - treeWidth - lineThickness;
            uLineHorizontal(linePos, lineLength, 0u);
        }

        uRightClickAddEvent("Open Folder", OpenCurrentFolderFn, nullptr);
        uRightClickAddEvent("Copy", nullptr, nullptr);
        uRightClickAddEvent("Delete", DeleteResource, nullptr);
        uRightClickAddEvent("Create Folder", CreateNewFolderFn, nullptr);
        uRightClickAddEvent("Create File", CreateNewFileFn, nullptr);

        if (mCopiedResource[0] != 0)
        uRightClickAddEvent("Paste", PasteResource, nullptr);

        if (mCreateFolderPopOpen) CreateResourcePopup("Create Folder", "What would you like as a folder name?", false);
        if (mCreateFilePopOpen) CreateResourcePopup("Create File", "What would you like as a file name?", true);
        if (mDeleteContentWarnPopOpen) DeleteWarnWindow("Delete?", mCopiedResource);

        // draw folder tree
        RecurseFolderTree("Assets", nullptr);

        window->elementPos = startPos + (lineThickness * 3.0f);
        if (window->elementPos.x + window->elementOffsetY * 3.0f < window->position.x + window->scale.x)
        {
            float windowEndX = window->position.x + window->scale.x - 15.0f;
            float windowEndY = window->position.y + window->scale.y - 15.0f;
            float searchWidth = windowEndX - window->elementPos.x;
            
            DrawNavigationBars(window);
            // subtract navigation width from search
            searchWidth = Clamp(searchWidth - window->elementOffsetY * 3.0f, 0.0f, 1920.0f);
            DrawSearch(window, searchWidth);

            window->elementPos.x -= window->elementOffsetY * 2.0f;
            window->elementPos.y += window->elementOffsetY + lineThickness * 3.0f;

            constexpr uint scissorMask = uScissorMask_All & ~uScissorMask_Quad;
            uBeginScissor(window->elementPos - 22.0f, Vec2(windowEndX - scrollWidth, windowEndY) - window->elementPos + 32.0f, scissorMask);
            uSetFloat(uf::QuadYMin, window->elementPos.y - 22.0f); // stenciling quads with min y pos
            
            // apply scrolling
            window->elementPos.y -= window->scrollPercent * window->elementsTotalHeight;

            mCurrElementIdx = 0;
            mBoxStartPos = window->elementPos;
            
            // boxes after this line
            if (!mSearching)
            {
                // draw folders first
                VisitFolder(mCurrentFolder, DrawFoldersFn, nullptr);
                // draw files second
                VisitFolder(mCurrentFolder, DrawFilesFn, nullptr);
            }
            else
            {
                for (int i = 0; i < mSearchResults.Size(); i++)
                {
                    bool isDir = IsDirectory(mSearchResults[i]);
                    if (isDir) DrawFoldersFn(mSearchResults[i], nullptr);
                    else       DrawFilesFn(mSearchResults[i], nullptr);
                }
            }

            uEndScissor(scissorMask);
            uSetFloat(uf::QuadYMin, 0.0f);
        }

        uWindowEnd();
    }
}

void EditorShow()
{
    static bool open0 = true;
    open0 ^= GetKeyPressed('B');
        
    SceneRenderer::ShowEditor(0.0f, &open0);
    
    Prefab* mainScene = g_CurrentScene.GetPrefab(MainScenePrefab);
    ShowPrefabView(mainScene);
    ShowResourcesWindow();

    if (GetKeyPressed('F'))
        FocusCameraToPrimitive(SelectedNodeIndex, SelectedNodePrimitiveIndex),
        FocusPrefabViewToSelectedNode();
}

#endif // AX_GAME_BUILD != 1
