#pragma once


#include "../../ASTL/Math/Matrix.hpp"
#include "../../ASTL/Algorithms.hpp"
#include "Platform.hpp"

// #define FREE_CAMERA

struct CameraBase
{
    Matrix4 projection;
    Matrix4 view;
    
    float verticalFOV = 65.0f;
    float nearClip = 0.1f;
    float farClip = 2400.0f;

    Vector2i viewportSize, monitorSize;

    Vector3f position;
    Vector2f mouseOld;
    Vector3f targetPos;
    Vector3f Front, Right, Up;
 
    float pitch = 0.0f, yaw = -9.0f , senstivity = 10.0f;

    bool wasPressing = false;
 
    FrustumPlanes frustumPlanes;

    Matrix4 inverseProjection;
    Matrix4 inverseView;

    virtual void Update() = 0;
    virtual void Init(Vector2i xviewPortSize) = 0;
    virtual void RecalculateView() = 0;

    void InitBase(Vector2i xviewPortSize)
    {
        verticalFOV = 65.0f;
        nearClip = 0.1f;
        farClip = 2400.0f;
        viewportSize = xviewPortSize;
        targetPos.x = 39.0f; // negate for bistro scene
        targetPos.z = 16.0f; // negate for bistro scene
        position = targetPos + Vec3(5.5f, 4.0f, 0.0f);
        wGetMonitorSize(&monitorSize.x, &monitorSize.y);

        RecalculateView();
        RecalculateProjection(xviewPortSize.x, xviewPortSize.y);
    }

    void RecalculateProjection(int width, int height)
    {
        viewportSize.x = width; viewportSize.y = height;
        projection = Matrix4::PerspectiveFovRH(verticalFOV * DegToRad, (float)width, (float)height, nearClip, farClip);
        inverseProjection = Matrix4::Inverse(projection);
    }

    void SetCursorPos(int x, int y)
    {
        SetMousePos((float)x, (float)y);
        mouseOld = Vec2((float)x, (float)y);
    }

    // when you move the mouse out of window it will apear opposide side what I mean by that is:
    // for example when your cursor goes to right like this |  ^->|   your mouse will apear at the left of the monitor |^    |
    void InfiniteMouse(const Vector2f& point)
    {
        #ifndef __ANDROID__
        if (point.x > monitorSize.x - 2) SetCursorPos(3, (int)point.y);
        if (point.y > monitorSize.y - 2) SetCursorPos((int)point.x, 3);
        
        if (point.x < 2) SetCursorPos(monitorSize.x - 3, (int)point.y);
        if (point.y < 2) SetCursorPos((int)point.x, monitorSize.y - 3);
        #endif
    }

    void VECTORCALL FocusToAABB(Vector4x32f min, Vector4x32f max)
    {
        Vector4x32f center   = VecLerp(min, max, 0.5f);
        Vector4x32f toCamDir = Vec3Norm(VecSub(VecLoad(position.arr), center));
        
        float camDist = Vec3Lenf(VecSub(min, max));
        Vector4x32f newPos = VecAdd(center, VecMulf(toCamDir, camDist));
        
        Vec3Store(position.arr, newPos);
        Vector4x32f vFront = VecNeg(toCamDir);
        Vec3Store(Front.arr, vFront);
        Vector4x32f vRight = Vec3Norm(Vec3Cross(vFront, VecSetR(0.0f, 1.0f, 0.0f, 0.0f)));
        Vec3Store(Right.arr, vRight);
        Vec3Store(Up.arr   , Vec3Cross(vRight, vFront));

        RecalculateView();
        frustumPlanes = CreateFrustumPlanes(view * projection);
        pitch = ASin(Front.y) * RadToDeg;
        yaw = ATan2(Front.z, Front.x) * RadToDeg;
    }

    Ray ScreenPointToRay(Vector2f pos) const
    {
        Vector2f coord = { pos.x / (float)viewportSize.x, pos.y / (float)viewportSize.y };
        coord.y = 1.0f - coord.y;    // Flip Y to match the NDC coordinate system
        coord = coord * 2.0f - 1.0f; // Map to range [-1, 1]

        Vector4x32f clipSpacePos = VecSetR(coord.x, coord.y, 1.0f, 1.0f);
        Vector4x32f viewSpacePos = Matrix4::Vector4Transform(clipSpacePos, inverseProjection);
        viewSpacePos = VecDiv(viewSpacePos, VecSplatW(viewSpacePos));
        
        Vector4x32f worldSpacePos = Matrix4::Vector4Transform(viewSpacePos, inverseView);
        
        Vector4x32f rayDir = Vec3Norm(VecSub(worldSpacePos, VecLoad(&position.x)));
        
        Ray ray;
        ray.origin = VecLoad(&position.x); 
        ray.direction = rayDir;
        return ray;
    }
};

//------------------------------------------------------------------------
struct FreeCamera : public CameraBase
{
    void Init(Vector2i xviewPortSize) override
    {
        pitch = 1.0f, yaw = 160.0f , senstivity = 10.0f;
        CalculateLook();

        InitBase(xviewPortSize);
    }
    
    void RecalculateView() override
    {
        view = Matrix4::LookAtRH(position, Front, Up);
        inverseView = Matrix4::Inverse(view);
    }

    void CalculateLook() // from yaw pitch
    {
        Front.x = Cos(yaw * DegToRad) * Cos(pitch * DegToRad);
        Front.y = Sin(pitch * DegToRad);
        Front.z = Sin(yaw * DegToRad) * Cos(pitch * DegToRad);
        Front.NormalizeSelf();
        // also re-calculate the Right and Up vector
        // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        Right = Vector3f::NormalizeEst(Vector3f::Cross(Front, Vector3f::Up()));
        Up = Vector3f::Cross(Right, Front);
    }

    void Update() override
    {
        bool pressing = GetMouseDown(MouseButton_Right);
        float dt = (float)GetDeltaTime();
        float speed = dt * (1.0f + GetKeyDown(Key_SHIFT) * 2.0f) * 85.0f;
        
        if (!pressing) { wasPressing = false; return; }
        
        Vector2f mousePos;
        GetMousePos(&mousePos.x, &mousePos.y);
        Vector2f diff = mousePos - mouseOld;
        wSetCursor(wCursor_ResizeAll);
        
        // if platform is android left side is for movement, right side is for rotating camera
#ifdef __ANDROID__
        if (mousePos.x > (monitorSize.x / 2.0f))
#endif
        {
            if (wasPressing && diff.x + diff.y < 130.0f)
            {
                pitch -= diff.y * dt * senstivity;
                yaw   += diff.x * dt * senstivity;
                yaw   = FMod(yaw + 180.0f, 360.0f) - 180.0f;
                pitch = Clamp(pitch, -89.0f, 89.0f);
            }
            
            CalculateLook();
        }
    #ifdef __ANDROID__
        else if (wasPressing && diff.x + diff.y < 130.0f)
            position += (Right * diff.x * 0.02f) + (Front * -diff.y * 0.02f);
    #endif  

    #ifndef __ANDROID__
        if (GetKeyDown('D')) position += Right * speed;
        if (GetKeyDown('A')) position -= Right * speed;
        if (GetKeyDown('W')) position += Front * speed;
        if (GetKeyDown('S')) position -= Front * speed;
        if (GetKeyDown('Q')) position -= Up * speed;
        if (GetKeyDown('E')) position += Up * speed;
    #endif
        mouseOld = mousePos;
        wasPressing = true;

        InfiniteMouse(mousePos);
        RecalculateView();
    
        frustumPlanes = CreateFrustumPlanes(view * projection);
    }
};


//------------------------------------------------------------------------
struct PlayerCamera : public CameraBase
{
    void Init(Vector2i xviewPortSize) override
    {
        senstivity = 0.02f;
        viewportSize = xviewPortSize;
        targetPos = Vec3(-39.0f, 0.0f, -16.0f);
        
        InitBase(xviewPortSize);
    }

    void RecalculateView() override
    {
        Front.x = Cos(yaw * TwoPI) * Cos(pitch * TwoPI);
        Front.y = Sin(pitch * TwoPI);
        Front.z = Sin(yaw * TwoPI) * Cos(pitch * TwoPI);
        Front = -Front;
        Front.NormalizeSelf();
        // also re-calculate the Right and Up vector
        // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        Right = Vector3f::NormalizeEst(Vector3f::Cross(Front, Vector3f::Up()));
        Up = Vector3f::Cross(Right, Front);

        position = targetPos + Vec3(0.0f, 3.6f, 0.0f);
        position -= Front * 5.0f;

        view = Matrix4::LookAtRH(position, Front, Up);
        inverseView = Matrix4::Inverse(view);

        frustumPlanes = CreateFrustumPlanes(view * projection);
    }
    
    void SetCursorPos(int x, int y)
    {
        SetMousePos((float)x, (float)y);
        mouseOld = Vec2((float)x, (float)y);
    }
    
    // when you move the mouse out of window it will apear opposide side what I mean by that is:
    // for example when your cursor goes to right like this |  ^->|   your mouse will apear at the left of the monitor |^    |
    void InfiniteMouse(const Vector2f& point)
    {
#ifndef __ANDROID__
        if (point.x > monitorSize.x - 2) SetCursorPos(3, (int)point.y);
        if (point.y > monitorSize.y - 2) SetCursorPos((int)point.x, 3);
        
        if (point.x < 2) SetCursorPos(monitorSize.x - 3, (int)point.y);
        if (point.y < 2) SetCursorPos((int)point.x, monitorSize.y - 3);
#endif
    }

    void MouseLook(Vector2f dir, float dt)
    {
        if (wasPressing && dir.x + dir.y < 100.0f)
        {
            yaw   += dir.x * dt * senstivity;
            pitch += dir.y * dt * senstivity;
            yaw    = Fract(yaw);
            pitch  = Clamp(pitch, -0.2f, 0.8f);
        }
    }

#if !defined(__ANDROID__)  // not android
    void Update() override
    {
        bool pressing = GetMouseDown(MouseButton_Right);
        float dt = (float)GetDeltaTime();
        
        if (!pressing) {
            RecalculateView();
            wasPressing = false; 
            return; 
        }
        
        Vector2f mousePos;
        GetMousePos(&mousePos.x, &mousePos.y);
        Vector2f diff = mousePos - mouseOld;
        	
        MouseLook(diff, dt);
        
        mouseOld = mousePos;
        wasPressing = true;
        
        InfiniteMouse(mousePos);
        RecalculateView();
    }
#else

    Vector2f GetTouchDir(int index) {
        Touch touch = GetTouch(index);
        return { touch.positionX, touch.positionY };
    }
    
    // Android Update
    void Update() override
    {
        int numTouch = NumTouchPressing();
        float dt = (float)GetDeltaTime();
        
        if (numTouch == 0) {
            RecalculateView();
            wasPressing = false;
            return;
        }
        
        Vector2f touch0 = GetTouchDir(0);
        Vector2f dir;
        wGetMonitorSize(&monitorSize.x, &monitorSize.y);
        // left side is for movement, right side is for rotating camera
        // if num touch is 1 and its right side of the screen
        if (numTouch == 1 && touch0.x < (monitorSize.x / 2.0f))
        {
            RecalculateView();
            return; // this touch used by movement
        }
        else // multi touch
        {
            Vector2f touch1 = GetTouchDir(1);
            
            // we want right touch to be used for looking
            if (touch1.x > touch0.x)
                Swap(touch0, touch1);
        }
        
        dir = touch0 - mouseOld;
        MouseLook(dir, dt);
        
        wasPressing = true; 
        mouseOld = touch0;
        
        RecalculateView();
    }
#endif
};

