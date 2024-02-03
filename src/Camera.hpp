#pragma once

#include <math.h>
#include "../ASTL/Math/Transform.hpp"
#include "Platform.hpp"

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

	float pitch = 0.0f, yaw = -90.0f , senstivity = 10.0f;

	bool wasPressing = false;

	void Init(Vector2i xviewPortSize)
	{
		verticalFOV = 65.0f;
		nearClip = 0.1f;
		farClip = 500.0f;
		pitch = 0.0f, yaw = -45.0f , senstivity = 10.0f;

		viewportSize = xviewPortSize;
		position = MakeVec3(0.0f, 3.0f, 0.0f);
		Front = MakeVec3(0.5f, 0.0f, -0.5f);

		wGetMonitorSize(&monitorSize.x, &monitorSize.y);

		RecalculateProjection(xviewPortSize.x, xviewPortSize.y);
		Update(true);
		RecalculateView();
	}

	void RecalculateProjection(int width, int height)
	{
		viewportSize.x = width; viewportSize.y = height;
		projection = Matrix4::PerspectiveFovRH(verticalFOV * DegToRad, (float)width, (float)height, nearClip, farClip);
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

	void Update(bool pass=false)
	{
		bool pressing = GetMouseDown(MouseButton_Right);
		if (!pressing && !pass) { wasPressing = false; return; }

		float dt = pass ? 0.05f : (float)GetDeltaTime() * 2.0f;
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

			Front.x = cosf(yaw * DegToRad) * cosf(pitch * DegToRad);
			Front.y = sinf(pitch * DegToRad);
			Front.z = sinf(yaw * DegToRad) * cosf(pitch * DegToRad);
			Front.NormalizeSelf();
			// also re-calculate the Right and Up vector
			// normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
			Right = Vector3f::Normalize(Vector3f::Cross(Front, Vector3f::Up()));
			Up = Vector3f::Cross(Right, Front);
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
		// Vector4f target = Matrix4::Vector4Transform(MakeVec4(coord.x, coord.y, 1.0f, 1.0f), inverseProjection);
		// target /= target.w;
		// target = Matrix4::Vector4Transform(target, inverseView);
		// Vector3f rayDir = Vector3f::Normalize(target.xyz());
		return {};//  MakeRay(position, rayDir);
	}
#ifdef AX_SUPPORT_SSE
	RaySSE ScreenPointToRaySSE(Vector2f pos) const
	{
		// Vector2f coord = MakeVec2(pos.x / (float)viewportSize.x, pos.y / (float)viewportSize.y);
		// coord.y = 1.0f - coord.y;
		// coord = coord * 2.0f - 1.0f;
		// Vector4f target = Matrix4::Vector4Transform(MakeVec4(coord.x, coord.y, 1.0f, 1.0f), inverseProjection);
		// target /= target.w;
		// target = Matrix4::Vector4Transform(target, inverseView);
		// Vector3f rayDir = Vector3f::Normalize(target.xyz());
		RaySSE ray;
		// ray.origin = _mm_load_ps(&position.x);
		// ray.direction = _mm_loadu_ps(&rayDir.x);
		// ray.direction = _mm_insert_ps(ray.direction, _mm_setzero_ps(), 3);
		return ray;
	}
#endif
};