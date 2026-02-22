//
// TouchControls.h
// Virtual touch controls for Cro-Mag Rally Android port.
// Provides virtual joystick (or gyroscope) steering and action buttons.
//
// NOTE: This header intentionally does NOT include ogl_support.h or gles_compat.h
// so that TouchControls.c can use raw GLES3 functions without bridge macros.
//

#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>

// Minimal 2D vector to avoid pulling in ogl_support.h
typedef struct { float x; float y; } TCVector2D;

#ifdef __ANDROID__

// Control types for steering
typedef enum
{
    kSteeringMode_Joystick = 0,
    kSteeringMode_Gyroscope,
    NUM_STEERING_MODES
} SteeringMode;

// Initialize/shutdown touch controls
void TouchControls_Init(void);
void TouchControls_Shutdown(void);

// Call once per frame with the current SDL event (before polling is done)
void TouchControls_ProcessEvent(const SDL_Event* event);

// Call after all events have been processed
void TouchControls_EndFrame(void);

// Returns current steering from touch controls (-1..+1 for X, -1..+1 for Y)
TCVector2D TouchControls_GetSteering(void);

// Returns whether a specific need button (kNeed_ThrowForward etc.) is touched
bool TouchControls_IsNeedPressed(int needID);
bool TouchControls_IsNeedPressedNew(int needID);

// Reset the gyroscope reference angle (re-center)
void TouchControls_RecenterGyro(void);

// Draw the touch controls overlay (call after 3D rendering, before swap)
void TouchControls_Draw(void);

// Set steering mode (joystick vs gyroscope)
void TouchControls_SetSteeringMode(SteeringMode mode);
SteeringMode TouchControls_GetSteeringMode(void);

// Set whether we are currently in gameplay (true) or a menu screen (false).
// In game mode: joystick + game action buttons are shown.
// In menu mode: navigation buttons (UIUp/Down/Confirm/Back) are shown.
void TouchControls_SetGameMode(bool inGame);
bool TouchControls_GetGameMode(void);

#else
// Stubs for non-Android builds
static inline void TouchControls_Init(void) {}
static inline void TouchControls_Shutdown(void) {}
static inline void TouchControls_ProcessEvent(const SDL_Event* e) { (void)e; }
static inline void TouchControls_EndFrame(void) {}
static inline TCVector2D TouchControls_GetSteering(void) { return (TCVector2D){0,0}; }
static inline bool TouchControls_IsNeedPressed(int n) { (void)n; return false; }
static inline bool TouchControls_IsNeedPressedNew(int n) { (void)n; return false; }
static inline void TouchControls_RecenterGyro(void) {}
static inline void TouchControls_Draw(void) {}
typedef int SteeringMode;
static inline void TouchControls_SetSteeringMode(SteeringMode m) { (void)m; }
static inline SteeringMode TouchControls_GetSteeringMode(void) { return 0; }
static inline void TouchControls_SetGameMode(bool b) { (void)b; }
static inline bool TouchControls_GetGameMode(void) { return false; }
#endif
