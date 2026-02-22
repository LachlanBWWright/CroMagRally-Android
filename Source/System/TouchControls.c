// TouchControls.c
// Virtual joystick and gyroscopic controls for Cro-Mag Rally on Android.

#ifdef __ANDROID__

#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <android/log.h>

#include "touch_controls.h"
#include "gles_bridge.h"

#define LOG_TAG "TouchControls"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

// Reference to main window (defined in Boot.cpp)
extern SDL_Window *gSDLWindow;

// ---------------------------------------------------------------------------
// Layout constants (in normalized screen coords, 0..1)
// Designed for landscape orientation.
// ---------------------------------------------------------------------------

#define JOYSTICK_X   0.12f     // center X of left joystick
#define JOYSTICK_Y   0.70f     // center Y
#define JOYSTICK_R   0.12f     // radius (fraction of screen height)

#define BTN_A_X      0.88f     // Accelerate (right side)
#define BTN_A_Y      0.72f
#define BTN_B_X      0.94f     // Brake
#define BTN_B_Y      0.55f
#define BTN_FIRE_X   0.82f     // Fire weapon
#define BTN_FIRE_Y   0.55f
#define BTN_PAUSE_X  0.95f     // Pause
#define BTN_PAUSE_Y  0.08f
#define BTN_RECENTER_X 0.88f   // Recenter gyro
#define BTN_RECENTER_Y 0.08f
#define BTN_TOGGLE_GYRO_X 0.06f  // Toggle gyro/joystick
#define BTN_TOGGLE_GYRO_Y 0.08f
#define BTN_RADIUS   0.055f    // button radius (fraction of screen height)

// Touch hit areas are 30% larger than visual size for better touch feel
#define HIT_AREA_SCALE_FACTOR  1.3f

// Touch IDs
#define MAX_FINGERS 10

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

typedef struct {
    SDL_FingerID id;
    float x, y;           // normalized 0..1
    bool active;
} Finger;

static Finger gFingers[MAX_FINGERS];

// Joystick state
static SDL_FingerID gJoystickFingerId = 0;
static bool         gJoystickActive = false;
static float        gJoystickCenterX = JOYSTICK_X;
static float        gJoystickCenterY = JOYSTICK_Y;
static float        gJoystickDX = 0;  // -1..1
static float        gJoystickDY = 0;  // -1..1

// Button states
static bool gBtnAccel   = false;
static bool gBtnBrake   = false;
static bool gBtnFire    = false;
static bool gBtnPause   = false;

// Gyroscope state
static bool  gGyroMode    = false;
static bool  gGyroAvail   = false;
static float gGyroBaseZ   = 0.0f;  // baseline for recenter
static float gGyroSteer   = 0.0f;  // current steer value -1..1
static SDL_SensorID gGyroSensorId = 0;

// Virtual gamepad
static SDL_JoystickID gVirtualJoystickID = 0;
static SDL_Joystick  *gVirtualJoystick   = NULL;

// Screen dimensions (set each frame)
static int gScreenW = 1280;
static int gScreenH = 720;

// ---------------------------------------------------------------------------
// GL helpers for drawing the HUD
// ---------------------------------------------------------------------------

// We need to draw circles/buttons without using fixed-function GL.
// Since this file is an Android-only file in the bridge context, we call
// GLES3 directly (no bridge macros - this file doesn't include game.h).

static GLuint gHudProgram  = 0;
static GLuint gHudVAO      = 0;
static GLuint gHudVBO      = 0;
static GLint  gHudLocMVP   = -1;
static GLint  gHudLocColor = -1;

static const char *kHudVertSrc =
"#version 300 es\n"
"precision highp float;\n"
"uniform mat4 u_mvp;\n"
"in vec2 a_pos;\n"
"void main() { gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0); }\n";

static const char *kHudFragSrc =
"#version 300 es\n"
"precision mediump float;\n"
"uniform vec4 u_color;\n"
"out vec4 fragColor;\n"
"void main() { fragColor = u_color; }\n";

static GLuint HudCompileShader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

static void HudInitGL(void)
{
    GLuint vs = HudCompileShader(GL_VERTEX_SHADER,   kHudVertSrc);
    GLuint fs = HudCompileShader(GL_FRAGMENT_SHADER, kHudFragSrc);
    gHudProgram = glCreateProgram();
    glAttachShader(gHudProgram, vs);
    glAttachShader(gHudProgram, fs);
    glLinkProgram(gHudProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    gHudLocMVP   = glGetUniformLocation(gHudProgram, "u_mvp");
    gHudLocColor = glGetUniformLocation(gHudProgram, "u_color");

    glGenVertexArrays(1, &gHudVAO);
    glGenBuffers(1, &gHudVBO);
}

// Build an ortho MVP matrix that maps pixel coordinates to NDC
static void BuildOrtho(float *m, float w, float h)
{
    // Column-major: maps (0..w, 0..h) → (-1..1, 1..-1)
    memset(m, 0, 16*sizeof(float));
    m[0]  =  2.0f/w;
    m[5]  = -2.0f/h;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] =  1.0f;
    m[15] =  1.0f;
}

static void DrawFilledCircle(float cx, float cy, float r, int segs,
                              float red, float green, float blue, float alpha)
{
    if (gHudProgram == 0) return;

    float verts[(64+2)*2];
    int n = 0;
    verts[n++] = cx; verts[n++] = cy;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / (float)segs * 6.28318530718f;
        verts[n++] = cx + cosf(a)*r;
        verts[n++] = cy + sinf(a)*r;
    }

    float mvp[16];
    BuildOrtho(mvp, (float)gScreenW, (float)gScreenH);

    glUseProgram(gHudProgram);
    glUniformMatrix4fv(gHudLocMVP,   1, GL_FALSE, mvp);
    glUniform4f(gHudLocColor, red, green, blue, alpha);

    glBindVertexArray(gHudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gHudVBO);
    glBufferData(GL_ARRAY_BUFFER, n*sizeof(float), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, n/2);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void DrawCircleOutline(float cx, float cy, float r, int segs,
                               float red, float green, float blue, float alpha)
{
    if (gHudProgram == 0) return;

    float verts[(64+1)*2];
    int n = 0;
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / (float)segs * 6.28318530718f;
        verts[n++] = cx + cosf(a)*r;
        verts[n++] = cy + sinf(a)*r;
    }

    float mvp[16];
    BuildOrtho(mvp, (float)gScreenW, (float)gScreenH);

    glUseProgram(gHudProgram);
    glUniformMatrix4fv(gHudLocMVP,   1, GL_FALSE, mvp);
    glUniform4f(gHudLocColor, red, green, blue, alpha);

    glBindVertexArray(gHudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gHudVBO);
    glBufferData(GL_ARRAY_BUFFER, n*sizeof(float), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINE_STRIP, 0, n/2);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Draw text label in pixel-space using a simple 2D quad (no actual text - just a marker)
// For simplicity, buttons are represented by filled circles only.

// ---------------------------------------------------------------------------
// Gyro sensor init
// ---------------------------------------------------------------------------

static void InitGyro(void)
{
    int numSensors = 0;
    SDL_SensorID *sensors = SDL_GetSensors(&numSensors);
    if (!sensors) return;

    for (int i = 0; i < numSensors; i++) {
        if (SDL_GetSensorTypeForID(sensors[i]) == SDL_SENSOR_GYRO) {
            gGyroSensorId = sensors[i];
            break;
        }
    }
    SDL_free(sensors);

    if (gGyroSensorId) {
        SDL_Sensor *sensor = SDL_OpenSensor(gGyroSensorId);
        if (sensor) {
            gGyroAvail = true;
            LOGI("Gyroscope available");
        }
    }
}

// ---------------------------------------------------------------------------
// Virtual gamepad init
// ---------------------------------------------------------------------------

static void InitVirtualGamepad(void)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type     = (Uint16)SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes    = (Uint16)SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = (Uint16)SDL_GAMEPAD_BUTTON_COUNT;
    desc.name     = "CroMagRally Virtual Controller";

    gVirtualJoystickID = SDL_AttachVirtualJoystick(&desc);
    if (gVirtualJoystickID == 0) {
        LOGI("Failed to attach virtual joystick: %s", SDL_GetError());
        return;
    }

    gVirtualJoystick = SDL_OpenJoystick(gVirtualJoystickID);
    if (!gVirtualJoystick) {
        LOGI("Failed to open virtual joystick: %s", SDL_GetError());
        return;
    }

    LOGI("Virtual gamepad created: ID %u", gVirtualJoystickID);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TouchControls_Init(void)
{
    memset(gFingers, 0, sizeof(gFingers));
    gJoystickActive = false;
    gJoystickDX = 0;
    gJoystickDY = 0;
    gBtnAccel = gBtnBrake = gBtnFire = gBtnPause = false;
    gGyroMode = false;
    gGyroSteer = 0.0f;
    gGyroBaseZ = 0.0f;

    // Initialize SDL joystick subsystem for virtual controller
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_SENSOR);

    InitGyro();
    InitVirtualGamepad();
    HudInitGL();

    LOGI("TouchControls_Init done (gyro=%d, vpad=%d)", gGyroAvail, gVirtualJoystickID != 0);
}

void TouchControls_Shutdown(void)
{
    if (gVirtualJoystick) {
        SDL_CloseJoystick(gVirtualJoystick);
        gVirtualJoystick = NULL;
    }
    if (gVirtualJoystickID) {
        SDL_DetachVirtualJoystick(gVirtualJoystickID);
        gVirtualJoystickID = 0;
    }
    if (gHudProgram) { glDeleteProgram(gHudProgram); gHudProgram = 0; }
    if (gHudVAO) { glDeleteVertexArrays(1, &gHudVAO); gHudVAO = 0; }
    if (gHudVBO) { glDeleteBuffers(1, &gHudVBO); gHudVBO = 0; }
}

void TouchControls_RecenterGyro(void)
{
    gGyroBaseZ = 0.0f;  // Will be recalibrated on next sensor read
    gGyroSteer = 0.0f;
    LOGI("Gyro recentered");
}

bool TouchControls_IsGyroMode(void)
{
    return gGyroMode && gGyroAvail;
}

float TouchControls_GetSteerAxis(void)
{
    if (gGyroMode && gGyroAvail)
        return gGyroSteer;
    return gJoystickDX;
}

float TouchControls_GetAccelAxis(void)
{
    return gJoystickDY;
}

// ---------------------------------------------------------------------------
// Hit test helpers
// ---------------------------------------------------------------------------

static float Dist2D(float ax, float ay, float bx, float by)
{
    float dx = ax-bx, dy = ay-by;
    return sqrtf(dx*dx + dy*dy);
}

// Convert absolute pixel coords to 0..1 normalized
static void PixelToNorm(float px, float py, float *nx, float *ny)
{
    *nx = px / (float)gScreenW;
    *ny = py / (float)gScreenH;
}

// Convert normalized to pixel
static void NormToPixel(float nx, float ny, float *px, float *py)
{
    *px = nx * (float)gScreenW;
    *py = ny * (float)gScreenH;
}

// Hit test a button (in normalized coords, radius in normalized height)
static bool HitButton(float touchNX, float touchNY,
                       float btnNX, float btnNY, float btnNR)
{
    float rPixels = btnNR * (float)gScreenH;
    float tx, ty, bx, by;
    NormToPixel(touchNX, touchNY, &tx, &ty);
    NormToPixel(btnNX, btnNY, &bx, &by);
    return Dist2D(tx, ty, bx, by) < rPixels * HIT_AREA_SCALE_FACTOR;  // larger hit area
}

// Hit test joystick region
static bool HitJoystick(float touchNX, float touchNY)
{
    float rPixels = JOYSTICK_R * (float)gScreenH;
    float tx, ty, bx, by;
    NormToPixel(touchNX, touchNY, &tx, &ty);
    NormToPixel(JOYSTICK_X, JOYSTICK_Y, &bx, &by);
    return Dist2D(tx, ty, bx, by) < rPixels * 1.5f;
}

// ---------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------

static int FindFinger(SDL_FingerID id)
{
    for (int i = 0; i < MAX_FINGERS; i++)
        if (gFingers[i].active && gFingers[i].id == id)
            return i;
    return -1;
}

static int AllocFinger(SDL_FingerID id, float x, float y)
{
    for (int i = 0; i < MAX_FINGERS; i++) {
        if (!gFingers[i].active) {
            gFingers[i].active = true;
            gFingers[i].id = id;
            gFingers[i].x = x;
            gFingers[i].y = y;
            return i;
        }
    }
    return -1;
}

static void UpdateButtonsFromTouch(void)
{
    gBtnAccel   = false;
    gBtnBrake   = false;
    gBtnFire    = false;
    gBtnPause   = false;

    bool prevPause = false;
    bool prevToggle = false;
    bool prevRecenter = false;

    for (int i = 0; i < MAX_FINGERS; i++) {
        if (!gFingers[i].active) continue;
        float nx = gFingers[i].x, ny = gFingers[i].y;

        if (HitButton(nx, ny, BTN_A_X, BTN_A_Y, BTN_RADIUS))
            gBtnAccel = true;
        if (HitButton(nx, ny, BTN_B_X, BTN_B_Y, BTN_RADIUS))
            gBtnBrake = true;
        if (HitButton(nx, ny, BTN_FIRE_X, BTN_FIRE_Y, BTN_RADIUS))
            gBtnFire = true;
        if (HitButton(nx, ny, BTN_PAUSE_X, BTN_PAUSE_Y, BTN_RADIUS))
            gBtnPause = true;
    }
    (void)prevPause; (void)prevToggle; (void)prevRecenter;
}

static void HandleTouchDown(SDL_FingerID id, float x, float y)
{
    float nx = x / (float)gScreenW;
    float ny = y / (float)gScreenH;

    // Recenter gyro button
    if (HitButton(nx, ny, BTN_RECENTER_X, BTN_RECENTER_Y, BTN_RADIUS)) {
        TouchControls_RecenterGyro();
        return;
    }

    // Toggle gyro/joystick button
    if (HitButton(nx, ny, BTN_TOGGLE_GYRO_X, BTN_TOGGLE_GYRO_Y, BTN_RADIUS)) {
        if (gGyroAvail) {
            gGyroMode = !gGyroMode;
            LOGI("Gyro mode: %d", gGyroMode);
        }
        return;
    }

    AllocFinger(id, nx, ny);

    // Check if this starts a joystick drag (only if not gyro mode)
    if (!gGyroMode && !gJoystickActive && HitJoystick(nx, ny)) {
        gJoystickActive   = true;
        gJoystickFingerId = id;
        gJoystickCenterX  = JOYSTICK_X;
        gJoystickCenterY  = JOYSTICK_Y;
        // Compute initial offset
        float dx = nx - gJoystickCenterX;
        float dy = ny - gJoystickCenterY;
        float rn = JOYSTICK_R;
        float dist = sqrtf(dx*dx*((float)gScreenW/(float)gScreenH)*((float)gScreenW/(float)gScreenH) + dy*dy);
        if (dist > rn) { float s = rn/dist; dx*=s; dy*=s; }
        gJoystickDX = dx / rn;
        gJoystickDY = dy / rn;
    }

    UpdateButtonsFromTouch();
}

static void HandleTouchMove(SDL_FingerID id, float x, float y)
{
    int idx = FindFinger(id);
    if (idx < 0) return;

    float nx = x / (float)gScreenW;
    float ny = y / (float)gScreenH;
    gFingers[idx].x = nx;
    gFingers[idx].y = ny;

    if (gJoystickActive && gJoystickFingerId == id) {
        float dx = nx - gJoystickCenterX;
        float dy = ny - gJoystickCenterY;
        // Scale by radius in normalized space
        float rn = JOYSTICK_R;
        // Clamp to unit circle
        float aspR = (float)gScreenW / (float)gScreenH;
        float dxP = dx * aspR;  // approximate pixel-space correction
        float len = sqrtf(dxP*dxP + dy*dy) / rn;
        if (len > 1.0f) { dxP /= len; dy /= len; }
        gJoystickDX = dxP;
        gJoystickDY = dy;
        // Apply dead zone
        float dz = 0.15f;
        if (fabsf(gJoystickDX) < dz) gJoystickDX = 0;
        if (fabsf(gJoystickDY) < dz) gJoystickDY = 0;
    }

    UpdateButtonsFromTouch();
}

static void HandleTouchUp(SDL_FingerID id)
{
    int idx = FindFinger(id);
    if (idx >= 0) {
        gFingers[idx].active = false;
    }

    if (gJoystickActive && gJoystickFingerId == id) {
        gJoystickActive = false;
        gJoystickDX     = 0;
        gJoystickDY     = 0;
    }

    UpdateButtonsFromTouch();
}

bool TouchControls_HandleEvent(const SDL_Event *event)
{
    // Ignore touch-synthesised mouse events
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (event->button.which == SDL_TOUCH_MOUSEID)
            return true;  // consume
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION &&
        event->motion.which == SDL_TOUCH_MOUSEID)
    {
        return true;
    }

    switch (event->type)
    {
        case SDL_EVENT_FINGER_DOWN:
        {
            int w, h;
            if (gSDLWindow) SDL_GetWindowSizeInPixels(gSDLWindow, &w, &h);
            else { w = gScreenW; h = gScreenH; }
            if (w > 0) gScreenW = w;
            if (h > 0) gScreenH = h;
            // In SDL3, tfinger.x/y are normalized 0..1
            HandleTouchDown(event->tfinger.fingerID,
                event->tfinger.x * gScreenW,
                event->tfinger.y * gScreenH);
            return false;  // don't consume (let SDL process it too)
        }
        case SDL_EVENT_FINGER_MOTION:
            HandleTouchMove(event->tfinger.fingerID,
                event->tfinger.x * gScreenW,
                event->tfinger.y * gScreenH);
            return false;
        case SDL_EVENT_FINGER_UP:
            HandleTouchUp(event->tfinger.fingerID);
            return false;

        case SDL_EVENT_SENSOR_UPDATE:
        {
            SDL_Sensor *sensor = SDL_GetSensorFromID(event->sensor.which);
            if (sensor && SDL_GetSensorType(sensor) == SDL_SENSOR_GYRO) {
                // Gyro data: [pitch, yaw, roll] in radians/second
                // For a landscape phone, yaw (index 1) = steering
                float yawRate = event->sensor.data[1];
                // Integrate (simple: use rate directly as steer, damped)
                float dt = 0.016f;  // assume ~60fps
                float steerDelta = yawRate * dt * 2.0f;
                gGyroSteer += steerDelta;
                gGyroSteer *= 0.92f;  // damping
                if (gGyroSteer >  1.0f) gGyroSteer =  1.0f;
                if (gGyroSteer < -1.0f) gGyroSteer = -1.0f;
            }
            return false;
        }

        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Update virtual gamepad
// ---------------------------------------------------------------------------

void TouchControls_Update(void)
{
    if (!gVirtualJoystick) return;

    // Steering axis
    float steer = gGyroMode ? gGyroSteer : gJoystickDX;

    // Left stick X = steering
    Sint16 lx = (Sint16)(steer * 32767.0f);
    SDL_SetJoystickVirtualAxis(gVirtualJoystick, SDL_GAMEPAD_AXIS_LEFTX, lx);

    // Left stick Y = accel/brake
    Sint16 ly = (Sint16)(gJoystickDY * 32767.0f);
    SDL_SetJoystickVirtualAxis(gVirtualJoystick, SDL_GAMEPAD_AXIS_LEFTY, ly);

    // Buttons
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_SOUTH,  gBtnAccel);
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_EAST,   gBtnBrake);
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_WEST,   gBtnFire);
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_START,  gBtnPause);

    // D-pad from left stick for menu navigation
    Sint16 thr = 16384;
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_DPAD_LEFT,  lx < -thr);
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, lx >  thr);
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_DPAD_UP,    ly < -thr);
    SDL_SetJoystickVirtualButton(gVirtualJoystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN,  ly >  thr);
}

// ---------------------------------------------------------------------------
// Draw HUD
// ---------------------------------------------------------------------------

void TouchControls_Draw(int viewportW, int viewportH)
{
    if (gHudProgram == 0) return;
    gScreenW = viewportW;
    gScreenH = viewportH;

    // Set up 2D rendering state
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float rPixels = JOYSTICK_R * (float)gScreenH;
    float jcxP = JOYSTICK_X  * (float)gScreenW;
    float jcyP = JOYSTICK_Y  * (float)gScreenH;

    // Joystick background (only if not in gyro steering mode)
    if (!gGyroMode) {
        DrawFilledCircle (jcxP, jcyP, rPixels,      32, 0.3f, 0.3f, 0.3f, 0.15f);
        DrawCircleOutline(jcxP, jcyP, rPixels,      32, 0.7f, 0.7f, 0.7f, 0.40f);

        // Thumb indicator
        float thumbX = jcxP + gJoystickDX * rPixels * 0.6f;
        float thumbY = jcyP + gJoystickDY * rPixels * 0.6f;
        DrawFilledCircle (thumbX, thumbY, rPixels*0.3f, 24, 0.5f, 0.5f, 0.5f, 0.35f);
        DrawCircleOutline(thumbX, thumbY, rPixels*0.3f, 24, 0.8f, 0.8f, 0.8f, 0.50f);
    }

    // Action buttons
    float br = BTN_RADIUS * (float)gScreenH;
    float acx = BTN_A_X * (float)gScreenW, acy = BTN_A_Y * (float)gScreenH;
    float bcx = BTN_B_X * (float)gScreenW, bcy = BTN_B_Y * (float)gScreenH;
    float fcx = BTN_FIRE_X * (float)gScreenW, fcy = BTN_FIRE_Y * (float)gScreenH;

    // Accel button (green)
    float aAlpha = gBtnAccel ? 0.55f : 0.20f;
    DrawFilledCircle (acx, acy, br, 24, 0.0f, 0.8f, 0.0f, aAlpha);
    DrawCircleOutline(acx, acy, br, 24, 0.0f, 1.0f, 0.0f, 0.60f);

    // Brake button (red)
    float bAlpha = gBtnBrake ? 0.55f : 0.20f;
    DrawFilledCircle (bcx, bcy, br, 24, 0.8f, 0.0f, 0.0f, bAlpha);
    DrawCircleOutline(bcx, bcy, br, 24, 1.0f, 0.0f, 0.0f, 0.60f);

    // Fire button (orange)
    float fAlpha = gBtnFire ? 0.55f : 0.20f;
    DrawFilledCircle (fcx, fcy, br, 24, 0.9f, 0.5f, 0.0f, fAlpha);
    DrawCircleOutline(fcx, fcy, br, 24, 1.0f, 0.6f, 0.0f, 0.60f);

    // Pause button (top right, small)
    float pcx = BTN_PAUSE_X * (float)gScreenW, pcy = BTN_PAUSE_Y * (float)gScreenH;
    float pAlpha = gBtnPause ? 0.55f : 0.20f;
    DrawFilledCircle (pcx, pcy, br*0.8f, 20, 0.5f, 0.5f, 0.5f, pAlpha);
    DrawCircleOutline(pcx, pcy, br*0.8f, 20, 0.8f, 0.8f, 0.8f, 0.60f);

    // Recenter button (if gyro mode)
    float recx = BTN_RECENTER_X * (float)gScreenW, recy = BTN_RECENTER_Y * (float)gScreenH;
    float gyroColor = gGyroMode ? 0.0f : 0.5f;
    DrawFilledCircle (recx, recy, br*0.8f, 20, gyroColor, 0.7f, 1.0f, 0.25f);
    DrawCircleOutline(recx, recy, br*0.8f, 20, gyroColor, 0.9f, 1.0f, 0.60f);

    // Toggle gyro button (top left)
    float tgcx = BTN_TOGGLE_GYRO_X * (float)gScreenW, tgcy = BTN_TOGGLE_GYRO_Y * (float)gScreenH;
    if (gGyroAvail) {
        float tgFill = gGyroMode ? 0.45f : 0.15f;
        DrawFilledCircle (tgcx, tgcy, br*0.8f, 20, 0.2f, 0.8f, 1.0f, tgFill);
        DrawCircleOutline(tgcx, tgcy, br*0.8f, 20, 0.2f, 0.9f, 1.0f, 0.60f);
    }

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

#endif // __ANDROID__
