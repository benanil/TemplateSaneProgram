#pragma once

#include "../ASTL/Math/Matrix.hpp"
#include "Platform.hpp"

/*
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
		pitch = 1.0f, yaw = -160.0f , senstivity = 10.0f;

		viewportSize = xviewPortSize;
		position = MakeVec3(5.5f, 4.0f, 0.0f);
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
    }

	void Update()
	{
		bool pressing = GetMouseDown(MouseButton_Right);
		float dt = (float)GetDeltaTime();
		float speed = dt * (1.0f + GetKeyDown(Key_SHIFT) * 2.0f);

		if (!pressing) { wasPressing = false; return; }

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
		UpdateFrustumPlanes();
	}

	Ray ScreenPointToRay(Vector2f pos) const
	{
		// Vector2f coord = MakeVec2(pos.x / (float)viewportSize.x, pos.y / (float)viewportSize.y);
		// coord.y = 1.0f - coord.y;
		// coord = coord * 2.0f - 1.0f;
		// vec_t target = Matrix4::Vector4Transform(VecSetR(coord.x, coord.y, 1.0f, 1.0f), inverseProjection);
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
		// vec_t target = Matrix4::Vector4Transform(VecSetR(coord.x, coord.y, 1.0f, 1.0f), inverseProjection);
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
*/
struct Camera
{
	Matrix4 projection;
	Matrix4 view;

	// Matrix4 inverseProjection;
	// Matrix4 inverseView;

	float verticalFOV = 65.0f;
	float nearClip = 0.07f;
	float farClip = 500.0f;

	Vector2i viewportSize, monitorSize;
	Vector2f mouseOld;
	float senstivity;
	bool wasPressing;
	FrustumPlanes frustumPlanes;
	
	Vector3f targetPos;
	Vector2f angle; // between 0 and 1 but we will multiply by PI
	Vector3f direction;
	
	void Init(Vector2i xviewPortSize)
	{
		verticalFOV = 65.0f;
		nearClip = 0.1f;
		farClip = 500.0f;
		senstivity = 0.1f;
		angle = MakeVec2(3.12f, 0.0f);
		viewportSize = xviewPortSize;
		targetPos = MakeVec3(0.0f, 0.0f, 0.0f);
		wGetMonitorSize(&monitorSize.x, &monitorSize.y);

		direction = Vector3f::Normalize(MakeVec3(cosf(angle.x * TwoPI), 0.0f, sinf(angle.x * TwoPI)));

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
		float x = angle.x * TwoPI;
		float y = angle.y * PI;

		Quaternion rot = QFromAxisAngle(MakeVec3(1.0f, 0.0f, 0.0f), -y);
		rot = QMul(rot, QFromAxisAngle(MakeVec3(0.0f, 1.0f, 0.0f), -x));
		
		Matrix4 camera = {};
		MatrixFromQuaternion(camera.GetPtr(), rot);
		camera.SetPosition(targetPos + MakeVec3(0.0f, 3.8f, 0.0f)); // camera height from foot
		camera = Matrix4::FromPosition({0.0f, 0.0f, 5.0f}) * camera; // camera distance 
		camera = Matrix4::Inverse(camera);
		view = camera;

		UpdateFrustumPlanes();
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

	void MouseLook(Vector2f dir, float dt)
	{
		if (wasPressing && dir.x + dir.y < 100.0f)
		{
			angle.x += dir.x * dt * senstivity;
			angle.y += dir.y * dt * senstivity;
			angle.x = Fract(angle.x);
			angle.y = Clamp(angle.y, -0.2f, 0.8f);
		}
	}

#ifndef __ANDROID__ /* NOT android */
	void Update()
	{
		bool pressing = GetMouseDown(MouseButton_Right);
		float dt = (float)GetDeltaTime();

		if (!pressing)
		{
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
	void Update()
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