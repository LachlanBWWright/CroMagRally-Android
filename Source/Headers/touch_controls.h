// touch_controls.h
// Touch controls (virtual joystick + gyroscope) for Android.

#pragma once

#ifdef __ANDROID__

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Call once after SDL init
void TouchControls_Init(void);

// Call on shutdown
void TouchControls_Shutdown(void);

// Handle SDL touch/sensor events. Returns true if the event was consumed.
bool TouchControls_HandleEvent(const SDL_Event *event);

// Call each frame to inject touch state into the virtual gamepad
void TouchControls_Update(void);

// Draw the HUD overlay (call after main 3D scene draw)
void TouchControls_Draw(int viewportW, int viewportH);

// Get raw analog values (-1..1)
float TouchControls_GetSteerAxis(void);   // left/right steering
float TouchControls_GetAccelAxis(void);   // accelerate/brake (vertical axis)

// Recenter gyro baseline
void TouchControls_RecenterGyro(void);

// Returns true if gyro controls are active
bool TouchControls_IsGyroMode(void);

#ifdef __cplusplus
}
#endif

#endif // __ANDROID__
