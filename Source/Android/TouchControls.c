// TOUCH CONTROLS FOR ANDROID - CRO-MAG RALLY
// Virtual joystick (steering) + action buttons for Cro-Mag Rally on Android.
//
// Layout (landscape):
//   Left side:  Virtual steering joystick (analog LEFTX/LEFTY axes)
//   Right side (bottom row, left to right):
//               Gas (A/South), Brake (RT), Reverse (B/East)
//   Right side (top row, left to right):
//               ThrowForward (Y/North), ThrowBack (X/West), Camera (LB), RearView (LT)
//   Top-right corner: Pause (Start)
//
// The virtual joystick is injected into SDL's virtual gamepad as analog axes.
// All buttons are injected as SDL gamepad button/axis presses so that
// the existing input code picks them up without modification.

#ifdef __ANDROID__

#include "TouchControls.h"
#include "gles_compat.h"

#include <SDL3/SDL.h>
#include <android/log.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

// SDL2 constants removed in SDL3; SDL3 uses bool true/false instead
#ifndef SDL_PRESSED
#define SDL_PRESSED  true
#define SDL_RELEASED false
#endif

#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,  "CroMagRally", __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, "CroMagRally", __VA_ARGS__)

// -------------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------------

// Joystick dead zone fraction
#define JOYSTICK_DEAD_ZONE  0.15f

// D-pad threshold for menu navigation (fraction of axis range)
#define DPAD_THRESHOLD      0.5f

// -------------------------------------------------------------------------
// Internal state
// -------------------------------------------------------------------------

// Virtual joystick state
static float    gJoyX = 0.0f;          // -1..+1 steering
static float    gJoyY = 0.0f;          // -1..+1 (up=negative, down=positive)
static bool     gJoyActive = false;
static SDL_FingerID gJoyFingerID = 0;
static float    gJoyCenterX = 0.0f;    // screen coords of joystick center
static float    gJoyCenterY = 0.0f;
static float    gJoyRadius  = 0.0f;    // joystick radius in screen coords

// Button state
static bool     gBtnDown[kTouchBtn_COUNT];
static SDL_FingerID gBtnFingerID[kTouchBtn_COUNT];

// Button screen regions (cx, cy, radius)
typedef struct { float cx, cy, radius; } BtnRegion;
static BtnRegion gBtnRegion[kTouchBtn_COUNT];

// SDL virtual joystick
static SDL_JoystickID   gVJoyID  = 0;
static SDL_Joystick    *gVJoy    = NULL;

// Screen dimensions (updated each frame)
static int gScreenW = 1280;
static int gScreenH = 720;

// -------------------------------------------------------------------------
// Layout helpers
// -------------------------------------------------------------------------

// Call this every frame to refresh layout based on current window size
static void UpdateLayout(int w, int h)
{
    if (w == gScreenW && h == gScreenH)
        return;
    gScreenW = w;
    gScreenH = h;

    // Left joystick: center at 15% from left, 65% from top; radius = 18% of height
    gJoyCenterX = w * 0.15f;
    gJoyCenterY = h * 0.65f;
    gJoyRadius  = h * 0.18f;

    // Right-side buttons
    // Bottom row: Gas, Brake, Reverse  (right side, bottom)
    // Top row: ThrowForward, ThrowBack, Camera, RearView
    float btnR     = h * 0.065f;   // button radius
    float rightBase = w * 0.88f;   // right column center x
    float colGap    = btnR * 2.6f; // column gap

    // Bottom row y
    float yBot  = h * 0.78f;
    // Top row y
    float yTop  = h * 0.54f;

    // Bottom row: 3 buttons
    gBtnRegion[kTouchBtn_Gas]          = (BtnRegion){ rightBase - colGap,         yBot, btnR };
    gBtnRegion[kTouchBtn_Brake]        = (BtnRegion){ rightBase,                  yBot, btnR };
    gBtnRegion[kTouchBtn_Reverse]      = (BtnRegion){ rightBase + colGap,         yBot, btnR };

    // Top row: 4 buttons
    gBtnRegion[kTouchBtn_ThrowForward] = (BtnRegion){ rightBase - colGap * 1.5f,  yTop, btnR };
    gBtnRegion[kTouchBtn_ThrowBack]    = (BtnRegion){ rightBase - colGap * 0.5f,  yTop, btnR };
    gBtnRegion[kTouchBtn_Camera]       = (BtnRegion){ rightBase + colGap * 0.5f,  yTop, btnR };
    gBtnRegion[kTouchBtn_RearView]     = (BtnRegion){ rightBase + colGap * 1.5f,  yTop, btnR };

    // Pause: top-right corner
    gBtnRegion[kTouchBtn_Pause]        = (BtnRegion){ w * 0.96f,  h * 0.08f, btnR * 0.85f };
}

// -------------------------------------------------------------------------
// Joystick hit test / update
// -------------------------------------------------------------------------

static bool JoystickHitTest(float sx, float sy)
{
    float dx = sx - gJoyCenterX;
    float dy = sy - gJoyCenterY;
    // Allow a slightly larger hit zone than the visual circle
    float hitRadius = gJoyRadius * 1.3f;
    return (dx * dx + dy * dy) <= (hitRadius * hitRadius);
}

static void UpdateJoystickFromTouch(float sx, float sy)
{
    float dx = (sx - gJoyCenterX) / gJoyRadius;
    float dy = (sy - gJoyCenterY) / gJoyRadius;

    // Clamp to unit circle
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 1.0f) {
        dx /= len;
        dy /= len;
    }

    // Apply dead zone
    float normLen = sqrtf(dx * dx + dy * dy);
    if (normLen < JOYSTICK_DEAD_ZONE) {
        dx = 0.0f;
        dy = 0.0f;
    } else {
        float scale = (normLen - JOYSTICK_DEAD_ZONE) / (1.0f - JOYSTICK_DEAD_ZONE);
        dx = (dx / normLen) * scale;
        dy = (dy / normLen) * scale;
    }

    gJoyX = dx;
    gJoyY = dy;
}

// -------------------------------------------------------------------------
// Button hit test
// -------------------------------------------------------------------------

static int ButtonHitTest(float sx, float sy)
{
    for (int i = 0; i < kTouchBtn_COUNT; i++) {
        float dx = sx - gBtnRegion[i].cx;
        float dy = sy - gBtnRegion[i].cy;
        float r  = gBtnRegion[i].radius * 1.3f;  // generous hit area
        if ((dx * dx + dy * dy) <= (r * r))
            return i;
    }
    return -1;
}

// -------------------------------------------------------------------------
// Event processing
// -------------------------------------------------------------------------

bool TouchControls_ProcessEvent(const SDL_Event *event)
{
    float sx, sy;
    SDL_FingerID fid;

    switch (event->type)
    {
    case SDL_EVENT_FINGER_DOWN:
        sx  = event->tfinger.x * gScreenW;
        sy  = event->tfinger.y * gScreenH;
        fid = event->tfinger.fingerID;

        // Try joystick first
        if (!gJoyActive && JoystickHitTest(sx, sy)) {
            gJoyActive   = true;
            gJoyFingerID = fid;
            UpdateJoystickFromTouch(sx, sy);
            return true;
        }

        // Then buttons
        {
            int btn = ButtonHitTest(sx, sy);
            if (btn >= 0 && !gBtnDown[btn]) {
                gBtnDown[btn]     = true;
                gBtnFingerID[btn] = fid;
                return true;
            }
        }
        break;

    case SDL_EVENT_FINGER_MOTION:
        sx  = event->tfinger.x * gScreenW;
        sy  = event->tfinger.y * gScreenH;
        fid = event->tfinger.fingerID;

        if (gJoyActive && fid == gJoyFingerID) {
            UpdateJoystickFromTouch(sx, sy);
            return true;
        }

        // Allow a button finger to release and repress on slide-off
        for (int i = 0; i < kTouchBtn_COUNT; i++) {
            if (gBtnDown[i] && gBtnFingerID[i] == fid) {
                float dx = sx - gBtnRegion[i].cx;
                float dy = sy - gBtnRegion[i].cy;
                float r  = gBtnRegion[i].radius * 1.5f;
                if ((dx * dx + dy * dy) > (r * r)) {
                    gBtnDown[i] = false;
                }
                return true;
            }
        }
        break;

    case SDL_EVENT_FINGER_UP:
        fid = event->tfinger.fingerID;

        if (gJoyActive && fid == gJoyFingerID) {
            gJoyActive = false;
            gJoyX = 0.0f;
            gJoyY = 0.0f;
            return true;
        }

        for (int i = 0; i < kTouchBtn_COUNT; i++) {
            if (gBtnDown[i] && gBtnFingerID[i] == fid) {
                gBtnDown[i] = false;
                return true;
            }
        }
        break;

    default:
        break;
    }

    return false;
}

// -------------------------------------------------------------------------
// Push into SDL virtual gamepad
// -------------------------------------------------------------------------

void TouchControls_UpdateVirtualGamepad(void)
{
    if (!gVJoy)
        return;

    // Left stick X: steering
    Sint16 lx = (Sint16)(gJoyX * 32767.0f);
    // Left stick Y: forward/back (not used for axis; forwarded as buttons below)
    Sint16 ly = (Sint16)(gJoyY * 32767.0f);

    SDL_SetJoystickVirtualAxis(gVJoy, SDL_GAMEPAD_AXIS_LEFTX, lx);
    SDL_SetJoystickVirtualAxis(gVJoy, SDL_GAMEPAD_AXIS_LEFTY, ly);

    // Gas → SOUTH (A)
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_SOUTH,
        gBtnDown[kTouchBtn_Gas] ? SDL_PRESSED : SDL_RELEASED);

    // Reverse → EAST (B)
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_EAST,
        gBtnDown[kTouchBtn_Reverse] ? SDL_PRESSED : SDL_RELEASED);

    // ThrowForward → NORTH (Y)
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_NORTH,
        gBtnDown[kTouchBtn_ThrowForward] ? SDL_PRESSED : SDL_RELEASED);

    // ThrowBack → WEST (X)
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_WEST,
        gBtnDown[kTouchBtn_ThrowBack] ? SDL_PRESSED : SDL_RELEASED);

    // Camera → LEFT_SHOULDER (LB)
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        gBtnDown[kTouchBtn_Camera] ? SDL_PRESSED : SDL_RELEASED);

    // Pause → START
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_START,
        gBtnDown[kTouchBtn_Pause] ? SDL_PRESSED : SDL_RELEASED);

    // Brake → RIGHT_TRIGGER axis (full press = 32767)
    Sint16 brake = gBtnDown[kTouchBtn_Brake] ? 32767 : 0;
    SDL_SetJoystickVirtualAxis(gVJoy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, brake);

    // RearView → LEFT_TRIGGER axis
    Sint16 rear = gBtnDown[kTouchBtn_RearView] ? 32767 : 0;
    SDL_SetJoystickVirtualAxis(gVJoy, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, rear);

    // D-pad from left stick for UI menu navigation
    Sint16 thr = (Sint16)(DPAD_THRESHOLD * 32767.0f);
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_DPAD_LEFT,  lx < -thr ? SDL_PRESSED : SDL_RELEASED);
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, lx >  thr ? SDL_PRESSED : SDL_RELEASED);
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_DPAD_UP,    ly < -thr ? SDL_PRESSED : SDL_RELEASED);
    SDL_SetJoystickVirtualButton(gVJoy, SDL_GAMEPAD_BUTTON_DPAD_DOWN,  ly >  thr ? SDL_PRESSED : SDL_RELEASED);
}

// -------------------------------------------------------------------------
// Drawing helpers (use GLES bridge calls)
// -------------------------------------------------------------------------

static void DrawFilledCircle(float cx, float cy, float r, int segments)
{
    bridge_Begin(GL_TRIANGLE_FAN);
    bridge_Vertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / (float)segments * 6.28318530718f;
        bridge_Vertex2f(cx + cosf(angle) * r, cy + sinf(angle) * r);
    }
    bridge_End();
}

static void DrawCircleOutline(float cx, float cy, float r, int segments)
{
    bridge_Begin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / (float)segments * 6.28318530718f;
        bridge_Vertex2f(cx + cosf(angle) * r, cy + sinf(angle) * r);
    }
    bridge_End();
}

// Draw a button with a centered letter label (simplified)
static void DrawButton(float cx, float cy, float r, bool pressed,
                       float lr, float lg, float lb)
{
    // Background
    if (pressed)
        bridge_Color4f(lr * 1.2f, lg * 1.2f, lb * 1.2f, 0.55f);
    else
        bridge_Color4f(lr, lg, lb, 0.30f);
    DrawFilledCircle(cx, cy, r, 20);

    // Outline
    bridge_Color4f(0.9f, 0.9f, 0.9f, 0.55f);
    DrawCircleOutline(cx, cy, r, 20);
}

// -------------------------------------------------------------------------
// Draw all touch controls
// -------------------------------------------------------------------------

void TouchControls_Draw(void)
{
    if (!gVJoy)
        return;

    // Update layout for current window size
    int w, h;
    {
        extern SDL_Window *gSDLWindow;
        SDL_GetWindowSizeInPixels(gSDLWindow, &w, &h);
    }
    UpdateLayout(w, h);

    // Set up 2D ortho projection to match screen pixels
    bridge_MatrixMode(GL_PROJECTION);
    bridge_PushMatrix();
    bridge_LoadIdentity();
    bridge_Ortho(0, w, h, 0, -1, 1);

    bridge_MatrixMode(GL_MODELVIEW);
    bridge_PushMatrix();
    bridge_LoadIdentity();

    // Save and set GL state for 2D overlay
    bridge_Disable(GL_DEPTH_TEST);
    bridge_Disable(GL_LIGHTING);
    bridge_Disable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    bridge_Disable(GL_FOG);

    bridge_FlushState();

    // --- Virtual Joystick ---
    // Outer ring (background)
    bridge_Color4f(0.25f, 0.25f, 0.25f, 0.18f);
    DrawFilledCircle(gJoyCenterX, gJoyCenterY, gJoyRadius, 32);

    bridge_Color4f(0.7f, 0.7f, 0.7f, 0.40f);
    DrawCircleOutline(gJoyCenterX, gJoyCenterY, gJoyRadius, 32);

    // Thumb indicator
    float thumbX = gJoyCenterX + gJoyX * gJoyRadius * 0.65f;
    float thumbY = gJoyCenterY + gJoyY * gJoyRadius * 0.65f;
    float thumbR = gJoyRadius * 0.32f;

    bridge_Color4f(0.5f, 0.5f, 0.5f, gJoyActive ? 0.55f : 0.35f);
    DrawFilledCircle(thumbX, thumbY, thumbR, 20);

    bridge_Color4f(0.8f, 0.8f, 0.8f, 0.5f);
    DrawCircleOutline(thumbX, thumbY, thumbR, 20);

    // --- Action Buttons ---
    // Gas (green)
    DrawButton(gBtnRegion[kTouchBtn_Gas].cx,          gBtnRegion[kTouchBtn_Gas].cy,
               gBtnRegion[kTouchBtn_Gas].radius,       gBtnDown[kTouchBtn_Gas],
               0.2f, 0.8f, 0.2f);

    // Brake (red)
    DrawButton(gBtnRegion[kTouchBtn_Brake].cx,         gBtnRegion[kTouchBtn_Brake].cy,
               gBtnRegion[kTouchBtn_Brake].radius,     gBtnDown[kTouchBtn_Brake],
               0.9f, 0.15f, 0.15f);

    // Reverse (orange)
    DrawButton(gBtnRegion[kTouchBtn_Reverse].cx,       gBtnRegion[kTouchBtn_Reverse].cy,
               gBtnRegion[kTouchBtn_Reverse].radius,   gBtnDown[kTouchBtn_Reverse],
               0.9f, 0.55f, 0.1f);

    // ThrowForward (yellow)
    DrawButton(gBtnRegion[kTouchBtn_ThrowForward].cx,  gBtnRegion[kTouchBtn_ThrowForward].cy,
               gBtnRegion[kTouchBtn_ThrowForward].radius, gBtnDown[kTouchBtn_ThrowForward],
               0.9f, 0.85f, 0.1f);

    // ThrowBack (purple)
    DrawButton(gBtnRegion[kTouchBtn_ThrowBack].cx,     gBtnRegion[kTouchBtn_ThrowBack].cy,
               gBtnRegion[kTouchBtn_ThrowBack].radius, gBtnDown[kTouchBtn_ThrowBack],
               0.6f, 0.1f, 0.9f);

    // Camera (cyan)
    DrawButton(gBtnRegion[kTouchBtn_Camera].cx,        gBtnRegion[kTouchBtn_Camera].cy,
               gBtnRegion[kTouchBtn_Camera].radius,    gBtnDown[kTouchBtn_Camera],
               0.1f, 0.8f, 0.9f);

    // RearView (light blue)
    DrawButton(gBtnRegion[kTouchBtn_RearView].cx,      gBtnRegion[kTouchBtn_RearView].cy,
               gBtnRegion[kTouchBtn_RearView].radius,  gBtnDown[kTouchBtn_RearView],
               0.3f, 0.5f, 0.9f);

    // Pause (gray)
    DrawButton(gBtnRegion[kTouchBtn_Pause].cx,         gBtnRegion[kTouchBtn_Pause].cy,
               gBtnRegion[kTouchBtn_Pause].radius,     gBtnDown[kTouchBtn_Pause],
               0.6f, 0.6f, 0.6f);

    // Restore matrices
    bridge_MatrixMode(GL_PROJECTION);
    bridge_PopMatrix();

    bridge_MatrixMode(GL_MODELVIEW);
    bridge_PopMatrix();

    // Restore state
    bridge_Enable(GL_DEPTH_TEST);
    bridge_Enable(GL_LIGHTING);
    bridge_Enable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    bridge_FlushState();
}

// -------------------------------------------------------------------------
// Init / Shutdown
// -------------------------------------------------------------------------

void TouchControls_Init(void)
{
    memset(gBtnDown, 0, sizeof(gBtnDown));
    memset(gBtnFingerID, 0, sizeof(gBtnFingerID));
    gJoyActive = false;
    gJoyX = 0.0f;
    gJoyY = 0.0f;

    // Set an initial layout (will be recalculated on first draw)
    UpdateLayout(1280, 720);

    // Create SDL3 virtual joystick as a gamepad
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type     = (Uint16)SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes    = (Uint16)SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = (Uint16)SDL_GAMEPAD_BUTTON_COUNT;
    desc.name     = "Cro-Mag Rally Touch Controller";

    gVJoyID = SDL_AttachVirtualJoystick(&desc);
    if (gVJoyID == 0) {
        LOGE("SDL_AttachVirtualJoystick failed: %s", SDL_GetError());
        return;
    }

    gVJoy = SDL_OpenJoystick(gVJoyID);
    if (!gVJoy) {
        LOGE("SDL_OpenJoystick(virtual) failed: %s", SDL_GetError());
        gVJoyID = 0;
        return;
    }

    LOGI("Virtual gamepad created: id=%u", (unsigned)gVJoyID);
}

void TouchControls_Shutdown(void)
{
    if (gVJoy) {
        SDL_CloseJoystick(gVJoy);
        gVJoy = NULL;
    }
    if (gVJoyID) {
        SDL_DetachVirtualJoystick(gVJoyID);
        gVJoyID = 0;
    }
}

#endif // __ANDROID__
