// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// =======================================================================
// DirectInput user interface class
// =======================================================================

#ifndef __INPUT_H
#define __INPUT_H

#include "Di7frame.h"

class DInput {
	friend class Orbiter;

public:
	DInput (Orbiter *pOrbiter);
	~DInput ();

	HRESULT Create (HINSTANCE hInst);
	void Destroy ();

	void SetRenderWindow(HWND hWnd);

	bool CreateKbdDevice();
	bool CreateJoyDevice ();
	void DestroyDevices ();

	inline CDIFramework7 *GetDIFrame() const { return diframe; }

#ifdef _WIN32
	inline const LPDIRECTINPUTDEVICE8 GetKbdDevice() const { return diframe->GetKbdDevice(); }
	inline const LPDIRECTINPUTDEVICE8 GetJoyDevice() const { return diframe->GetJoyDevice(); }
	bool PollJoystick (DIJOYSTATE2 *js);
#else
	bool PollJoystick (void *js);
#endif

	void OptionChanged(DWORD cat, DWORD item);

	struct JoyProp {
		bool bThrottle;  // joystick has throttle control
		bool bRudder;    // joystick has rudder control
		int ThrottleOfs; // throttle data offset
	};

protected:
	HRESULT SetJoystickProperties ();

private:
	Orbiter *orbiter;
	CDIFramework7 *diframe;
	JoyProp joyprop;
	HWND m_hWnd;
};

#endif // !__INPUT_H
