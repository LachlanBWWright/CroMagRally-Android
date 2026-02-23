// TOUCH CONTROLS FOR ANDROID - CRO-MAG RALLY
// Virtual joystick + action buttons for Cro-Mag Rally on Android.
#pragma once

#ifdef __ANDROID__

#include <stdbool.h>
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Button IDs for touch controls
typedef enum
{
    kTouchBtn_Gas          = 0,   // A / South    (accelerate/forward)
    kTouchBtn_Brake        = 1,   // RT           (brakes)
    kTouchBtn_Reverse      = 2,   // B / East     (reverse/backward)
    kTouchBtn_ThrowForward = 3,   // Y / North    (throw weapon forward)
    kTouchBtn_ThrowBack    = 4,   // X / West     (throw weapon backward)
    kTouchBtn_Camera       = 5,   // LB           (camera mode toggle)
    kTouchBtn_RearView     = 6,   // LT           (rear view)
    kTouchBtn_Pause        = 7,   // Start        (pause)
    kTouchBtn_COUNT
} TouchButtonID;

// Initialize the touch control system (call after GL context is ready)
void TouchControls_Init(void);

// Shutdown the touch control system
void TouchControls_Shutdown(void);

// Process an SDL event (call from DoSDLMaintenance for touch events)
bool TouchControls_ProcessEvent(const SDL_Event *event);

// Push current touch state into the SDL virtual gamepad (call once per frame)
void TouchControls_UpdateVirtualGamepad(void);

// Draw the touch controls overlay (call at end of each frame)
void TouchControls_Draw(void);

#ifdef __cplusplus
}
#endif

#endif // __ANDROID__
