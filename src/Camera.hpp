#pragma once

#include <math.h>
#include "../ASTL/Math/Transform.hpp"
#include "Platform.hpp"


struct FrustumPlanes
{
    vec_t planes[6];
};

inline FrustumPlanes CreateFrustumPlanes(const Matrix4& viewProjection)
{
    FrustumPlanes result;
    Matrix4 C = Matrix4::Transpose(viewProjection);
    result.planes[0] = VecNorm(VecAdd(C.r[3], C.r[0])); // m_left_plane
    result.planes[1] = VecNorm(VecSub(C.r[3], C.r[0])); // m_right_plane
    result.planes[2] = VecNorm(VecAdd(C.r[3], C.r[1])); // m_bottom_plane
    result.planes[3] = VecNorm(VecSub(C.r[3], C.r[1])); // m_top_plane
    result.planes[4] = VecNorm(C.r[2]);                 // m_near_plane  
    // result.planes[5] = VecNorm(VecSub(C.r[3], C.r[2])); // m_far_plane
    return result;
}

__forceinline vec_t VECTORCALL MaxPointAlongNormal(const vec_t min, const vec_t max, const vec_t n) 
{
    veci_t control = VecCmpGe(n, VecZero());
    return VecSelect(min, max, control);
}

inline bool CheckAABBCulled(vec_t minAABB, vec_t maxAABB, const FrustumPlanes& frustum, const Matrix4& matrix)
{
    vec_t min = Matrix4::Vector3Transform(minAABB, matrix);
    vec_t max = Matrix4::Vector3Transform(maxAABB, matrix);
    
    for (uint i = 0u; i < 5u; ++i) // make < 6 if you want far plane 
    {
        vec_t p = MaxPointAlongNormal(min, max, frustum.planes[i]);
        if (VecDotf(frustum.planes[i], p) < 0.0f)
        {
            return false;
        }
    }
    return true;
}

inline bool isPointCulled(const FrustumPlanes& frustum, const Vector3f& _point, const Matrix4& matrix)
{
    vec_t point = Matrix4::Vector3Transform(VecLoad(&_point.x), matrix);
    
    for (uint i = 0u; i < 5u; ++i)
    {
        if (VecDotf(frustum.planes[i], point) < 0.0f)
            return false;
    }
    return true;
}

struct Camera
{
	Matrix4 projection;
	Matrix4 view;

	// Matrix4 inverseProjection;
	// Matrix4 inverseView;

	float verticalFOV = 65.0f;
	float nearClip = 0.1f;
	float farClip = 500.0f;

	Vector2i viewportSize, monitorSize;

	Vector3f position;
	Vector2f mouseOld;

	Vector3f Front, Right, Up;
 
	float pitch = 0.0f, yaw = -9.0f , senstivity = 10.0f;

	bool wasPressing = false;
	
	FrustumPlanes frustumPlanes;

	void Init(Vector2i xviewPortSize)
	{
		verticalFOV = 65.0f;
		nearClip = 0.1f;
		farClip = 500.0f;
		pitch = 5.0f, yaw = -160.0f , senstivity = 10.0f;

		viewportSize = xviewPortSize;
		position = MakeVec3(2.5f, 4.0f, 0.0f);
        CalculateLook();
		wGetMonitorSize(&monitorSize.x, &monitorSize.y);

		RecalculateView();
		RecalculateProjection(xviewPortSize.x, xviewPortSize.y);
	}

	void UpdateFrustumPlanes()
	{
		Matrix4 viewProjection = view * projection;
		frustumPlanes = CreateFrustumPlanes(viewProjection);
	}

	void RecalculateProjection(int width, int height)
	{
		viewportSize.x = width; viewportSize.y = height;
		projection = Matrix4::PerspectiveFovRH(verticalFOV * DegToRad, (float)width, (float)height, nearClip, farClip);
		UpdateFrustumPlanes();
		// inverseProjection = Matrix4::Inverse(projection);
	}

	void RecalculateView()
	{
		view = Matrix4::LookAtRH(position, Front, Up);
		// inverseView = Matrix4::Inverse(view);
	}

	void SetCursorPos(int x, int y)
	{
		SetMousePos((float)x, (float)y);
		mouseOld = MakeVec2((float)x, (float)y);
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
		UpdateFrustumPlanes();
    }

	void Update()
	{
		bool pressing = GetMouseDown(MouseButton_Right);
		if (!pressing) { wasPressing = false; return; }

		float dt = (float)GetDeltaTime() * 2.0f;
		float speed = dt * (1.0f + GetKeyDown(Key_SHIFT) * 2.0f) * 1.2f;

		Vector2f mousePos;
		GetMousePos(&mousePos.x, &mousePos.y);
		Vector2f diff = mousePos - mouseOld;

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
	}

	Ray ScreenPointToRay(Vector2f pos) const
	{
		// Vector2f coord = MakeVec2(pos.x / (float)viewportSize.x, pos.y / (float)viewportSize.y);
		// coord.y = 1.0f - coord.y;
		// coord = coord * 2.0f - 1.0f;
		// vec_t target = Matrix4::Vector4Transform(MakeVec4(coord.x, coord.y, 1.0f, 1.0f), inverseProjection);
		// target /= target.w;
		// target = Matrix4::Vector4Transform(target, inverseView);
		// Vector3f rayDir = Vector3f::Normalize(target.xyz());
		return {};//  MakeRay(position, rayDir);
	}
#ifdef AX_SUPPORT_SSE
	Ray ScreenPointToRaySSE(Vector2f pos) const
	{
		// Vector2f coord = MakeVec2(pos.x / (float)viewportSize.x, pos.y / (float)viewportSize.y);
		// coord.y = 1.0f - coord.y;
		// coord = coord * 2.0f - 1.0f;
		// vec_t target = Matrix4::Vector4Transform(MakeVec4(coord.x, coord.y, 1.0f, 1.0f), inverseProjection);
		// target /= target.w;
		// target = Matrix4::Vector4Transform(target, inverseView);
		// Vector3f rayDir = Vector3f::Normalize(target.xyz());
		Ray ray;
		// ray.origin = _mm_load_ps(&position.x);
		// ray.direction = _mm_loadu_ps(&rayDir.x);
		// ray.direction = _mm_insert_ps(ray.direction, _mm_setzero_ps(), 3);
		return ray;
	}
#endif
};