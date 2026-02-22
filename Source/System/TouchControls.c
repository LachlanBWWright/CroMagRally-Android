//
// TouchControls.c
// Virtual touch controls for Cro-Mag Rally Android port.
//
// Implements:
//   - Virtual joystick (left side) for steering
//   - Gyroscope steering (as alternative)
//   - Action buttons (right side): ThrowForward, ThrowBackward, Brakes, Forward, Backward
//   - Gyro recenter button
//   - Overlay drawing using GLES bridge
//

#ifdef __ANDROID__

#include "TouchControls.h"
// Include GLES3 directly - do NOT include game.h/ogl_support.h/gles_compat.h
// because this file uses raw GLES3 and must not have bridge macros active.
#include <GLES3/gl3.h>
#include <SDL3/SDL.h>
#include <android/log.h>
#include <math.h>
#include <string.h>

// Need IDs come from input.h but we only use integer constants here
// to avoid pulling in game.h. Keep in sync with input.h enum.
#include "input.h"

#define TC_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "TouchCtrl", __VA_ARGS__)

// ============================================================
// LAYOUT CONSTANTS (in normalized device coordinates 0..1)
// ============================================================

// Joystick (left side)
#define JOYSTICK_CENTER_X   0.12f
#define JOYSTICK_CENTER_Y   0.65f
#define JOYSTICK_RADIUS     0.10f
#define JOYSTICK_DEAD_ZONE  0.15f

// Game buttons (right side) - 8 action buttons in 2 rows
#define BTN_RADIUS          0.055f
#define BTN_RIGHT_COL4_X    0.93f
#define BTN_RIGHT_COL3_X    0.84f
#define BTN_RIGHT_COL2_X    0.75f
#define BTN_RIGHT_COL1_X    0.66f
#define BTN_ROW1_Y          0.70f
#define BTN_ROW2_Y          0.88f

// Menu navigation buttons (right side of screen)
#define MENU_BTN_RADIUS     0.065f
#define MENU_NAV_X          0.88f    // X center for up/down arrows
#define MENU_UP_Y           0.45f    // Y for UIUp
#define MENU_DOWN_Y         0.62f    // Y for UIDown
#define MENU_CONFIRM_X      0.75f    // X for confirm
#define MENU_CONFIRM_Y      0.53f    // Y for confirm
#define MENU_BACK_X         0.06f    // X for UIBack (top-left, same as pause)
#define MENU_BACK_Y         0.08f    // Y for UIBack

// Gyro recenter button (top-center)
#define GYRO_BTN_X          0.50f
#define GYRO_BTN_Y          0.06f
#define GYRO_BTN_RADIUS     0.04f

// Steering mode toggle button (above joystick area)
#define MODE_BTN_X          0.12f
#define MODE_BTN_Y          0.10f
#define MODE_BTN_RADIUS     0.04f

// Max buttons in each mode
#define MAX_GAME_BUTTONS    8
#define MAX_MENU_BUTTONS    4
#define MAX_BUTTONS         8

// ============================================================
// BUTTON DEFINITIONS
// ============================================================

typedef struct
{
    float       cx, cy;     // center in [0..1] screen space
    float       radius;     // hit radius in [0..1]
    int         needID;     // kNeed_XXX
    int         fingerID;   // SDL finger id holding this button (-1 if none)
    bool        pressed;
    bool        wasPressed;
} TouchButton;

// ============================================================
// STATE
// ============================================================

static struct
{
    // Joystick
    float       joyCX, joyCY;      // reference center (screen [0..1])
    float       joyDX, joyDY;      // current analog value (-1..+1)
    int         joyFingerID;        // which finger is on joystick (-1 = none)
    bool        joyActive;

    // Buttons: separate sets for game and menu modes
    TouchButton gameButtons[MAX_GAME_BUTTONS];
    int         numGameButtons;
    TouchButton menuButtons[MAX_MENU_BUTTONS];
    int         numMenuButtons;

    // Currently active button set (points to either gameButtons or menuButtons)
    TouchButton* buttons;
    int         numButtons;

    // Game mode flag
    bool        inGame;

    // Gyroscope
    SDL_Sensor* gyroSensor;
    float       gyroOffset;         // recenter offset (radians around Z)
    float       gyroValue;          // current steering value (-1..+1)
    SteeringMode steeringMode;

    // Steering output
    TCVector2D  steering;

    // Screen size (updated each frame)
    int         screenW, screenH;

    // Gyro steering recalculation
    float       gyroBias;       // recenter bias
    float       filteredRate;   // low-pass filtered gyro rate

    // Gyro button
    float       gyroRecenterCX, gyroRecenterCY, gyroRecenterR;
    int         gyroRecenterFinger;

    // Mode toggle button
    float       modeBtnCX, modeBtnCY, modeBtnR;
    int         modeBtnFinger;
} gTC;

// ============================================================
// HELPERS
// ============================================================

static float Dist2D(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return sqrtf(dx*dx + dy*dy);
}

static bool HitTest(float nx, float ny, float cx, float cy, float r) {
    return Dist2D(nx, ny, cx, cy) < r;
}

// ============================================================
// INITIALIZATION
// ============================================================

void TouchControls_Init(void)
{
    memset(&gTC, 0, sizeof(gTC));
    gTC.joyFingerID = -1;
    gTC.steeringMode = kSteeringMode_Joystick;
    gTC.gyroRecenterFinger = -1;
    gTC.modeBtnFinger = -1;

    // Joystick center
    gTC.joyCX = JOYSTICK_CENTER_X;
    gTC.joyCY = JOYSTICK_CENTER_Y;

    // Gyro recenter button
    gTC.gyroRecenterCX = GYRO_BTN_X;
    gTC.gyroRecenterCY = GYRO_BTN_Y;
    gTC.gyroRecenterR  = GYRO_BTN_RADIUS;

    // Mode toggle button (joystick/gyro switch)
    gTC.modeBtnCX = MODE_BTN_X;
    gTC.modeBtnCY = MODE_BTN_Y;
    gTC.modeBtnR  = MODE_BTN_RADIUS;

    // ---- Game action buttons (all in-game controls) ----
    // Row 1: ThrowForward, ThrowBackward, CameraMode, RearView
    gTC.gameButtons[0] = (TouchButton){ BTN_RIGHT_COL1_X, BTN_ROW1_Y, BTN_RADIUS, kNeed_ThrowForward,  -1, false, false };
    gTC.gameButtons[1] = (TouchButton){ BTN_RIGHT_COL2_X, BTN_ROW1_Y, BTN_RADIUS, kNeed_ThrowBackward, -1, false, false };
    gTC.gameButtons[2] = (TouchButton){ BTN_RIGHT_COL3_X, BTN_ROW1_Y, BTN_RADIUS, kNeed_CameraMode,    -1, false, false };
    gTC.gameButtons[3] = (TouchButton){ BTN_RIGHT_COL4_X, BTN_ROW1_Y, BTN_RADIUS, kNeed_RearView,      -1, false, false };
    // Row 2: Forward, Backward, Brakes, Pause
    gTC.gameButtons[4] = (TouchButton){ BTN_RIGHT_COL1_X, BTN_ROW2_Y, BTN_RADIUS, kNeed_Forward,       -1, false, false };
    gTC.gameButtons[5] = (TouchButton){ BTN_RIGHT_COL2_X, BTN_ROW2_Y, BTN_RADIUS, kNeed_Backward,      -1, false, false };
    gTC.gameButtons[6] = (TouchButton){ BTN_RIGHT_COL3_X, BTN_ROW2_Y, BTN_RADIUS, kNeed_Brakes,        -1, false, false };
    gTC.gameButtons[7] = (TouchButton){ BTN_RIGHT_COL4_X, BTN_ROW2_Y, BTN_RADIUS, kNeed_UIPause,       -1, false, false };
    gTC.numGameButtons = 8;

    // ---- Menu navigation buttons ----
    gTC.menuButtons[0] = (TouchButton){ MENU_NAV_X,     MENU_UP_Y,      MENU_BTN_RADIUS, kNeed_UIUp,      -1, false, false };
    gTC.menuButtons[1] = (TouchButton){ MENU_CONFIRM_X, MENU_CONFIRM_Y, MENU_BTN_RADIUS, kNeed_UIConfirm, -1, false, false };
    gTC.menuButtons[2] = (TouchButton){ MENU_NAV_X,     MENU_DOWN_Y,    MENU_BTN_RADIUS, kNeed_UIDown,    -1, false, false };
    gTC.menuButtons[3] = (TouchButton){ MENU_BACK_X,    MENU_BACK_Y,    MENU_BTN_RADIUS, kNeed_UIBack,    -1, false, false };
    gTC.numMenuButtons = 4;

    // Default to menu mode; will be switched to game mode when gameplay starts
    gTC.inGame = false;
    gTC.buttons = gTC.menuButtons;
    gTC.numButtons = gTC.numMenuButtons;

    // Try to open gyroscope
    int numSensors = 0;
    SDL_SensorID* sensors = SDL_GetSensors(&numSensors);
    if (sensors)
    {
        for (int i = 0; i < numSensors; i++)
        {
            if (SDL_GetSensorTypeForID(sensors[i]) == SDL_SENSOR_GYRO)
            {
                gTC.gyroSensor = SDL_OpenSensor(sensors[i]);
                if (gTC.gyroSensor)
                {
                    TC_LOG("Gyroscope opened: %s", SDL_GetSensorName(gTC.gyroSensor));
                    break;
                }
            }
        }
        SDL_free(sensors);
    }

    if (!gTC.gyroSensor)
    {
        TC_LOG("No gyroscope found; steering mode set to joystick only");
        gTC.steeringMode = kSteeringMode_Joystick;
    }
}

void TouchControls_Shutdown(void)
{
    if (gTC.gyroSensor)
    {
        SDL_CloseSensor(gTC.gyroSensor);
        gTC.gyroSensor = NULL;
    }
}

// ============================================================
// EVENT PROCESSING
// ============================================================

static void ProcessFingerDown(float nx, float ny, SDL_FingerID fingerID)
{
    // Check gyro recenter button
    if (gTC.steeringMode == kSteeringMode_Gyroscope &&
        HitTest(nx, ny, gTC.gyroRecenterCX, gTC.gyroRecenterCY, gTC.gyroRecenterR))
    {
        gTC.gyroRecenterFinger = (int)fingerID;
        TouchControls_RecenterGyro();
        return;
    }

    // Check mode toggle button
    if (HitTest(nx, ny, gTC.modeBtnCX, gTC.modeBtnCY, gTC.modeBtnR))
    {
        gTC.modeBtnFinger = (int)fingerID;
        SteeringMode newMode = (gTC.steeringMode == kSteeringMode_Joystick)
                             ? kSteeringMode_Gyroscope
                             : kSteeringMode_Joystick;
        if (newMode == kSteeringMode_Gyroscope && !gTC.gyroSensor)
            newMode = kSteeringMode_Joystick;  // no gyro available
        TouchControls_SetSteeringMode(newMode);
        return;
    }

    // Check joystick zone (only in joystick mode)
    if (gTC.steeringMode == kSteeringMode_Joystick && !gTC.joyActive)
    {
        if (nx < 0.35f) // left side of screen = joystick zone
        {
            gTC.joyActive = true;
            gTC.joyFingerID = (int)fingerID;
            gTC.joyCX = nx;
            gTC.joyCY = ny;
            gTC.joyDX = 0;
            gTC.joyDY = 0;
            return;
        }
    }

    // Check action buttons
    for (int i = 0; i < gTC.numButtons; i++)
    {
        TouchButton* btn = &gTC.buttons[i];
        if (btn->fingerID < 0 && HitTest(nx, ny, btn->cx, btn->cy, btn->radius))
        {
            btn->fingerID = (int)fingerID;
            btn->pressed = true;
            return;
        }
    }
}

static void ProcessFingerUp(SDL_FingerID fingerID)
{
    // Gyro recenter finger
    if (gTC.gyroRecenterFinger == (int)fingerID)
    {
        gTC.gyroRecenterFinger = -1;
        return;
    }

    // Mode toggle finger
    if (gTC.modeBtnFinger == (int)fingerID)
    {
        gTC.modeBtnFinger = -1;
        return;
    }

    // Joystick
    if (gTC.joyActive && gTC.joyFingerID == (int)fingerID)
    {
        gTC.joyActive = false;
        gTC.joyFingerID = -1;
        gTC.joyDX = 0;
        gTC.joyDY = 0;
        // Reset reference to default position
        gTC.joyCX = JOYSTICK_CENTER_X;
        gTC.joyCY = JOYSTICK_CENTER_Y;
        return;
    }

    // Buttons
    for (int i = 0; i < gTC.numButtons; i++)
    {
        TouchButton* btn = &gTC.buttons[i];
        if (btn->fingerID == (int)fingerID)
        {
            btn->fingerID = -1;
            btn->pressed = false;
            return;
        }
    }
}

static void ProcessFingerMotion(float nx, float ny, SDL_FingerID fingerID)
{
    // Joystick move
    if (gTC.joyActive && gTC.joyFingerID == (int)fingerID)
    {
        float dx = nx - gTC.joyCX;
        float dy = ny - gTC.joyCY;

        // Normalize to joystick radius
        float mag = sqrtf(dx*dx + dy*dy);
        float maxRadius = JOYSTICK_RADIUS;

        if (mag > maxRadius)
        {
            dx = dx / mag * maxRadius;
            dy = dy / mag * maxRadius;
        }

        // Apply dead zone
        float ndx = dx / maxRadius;
        float ndy = dy / maxRadius;
        float nmag = sqrtf(ndx*ndx + ndy*ndy);
        if (nmag < JOYSTICK_DEAD_ZONE)
        {
            gTC.joyDX = 0;
            gTC.joyDY = 0;
        }
        else
        {
            float scale = (nmag - JOYSTICK_DEAD_ZONE) / (1.0f - JOYSTICK_DEAD_ZONE);
            scale = fminf(scale, 1.0f);
            gTC.joyDX = ndx / nmag * scale;
            gTC.joyDY = ndy / nmag * scale;
        }
    }
}

void TouchControls_ProcessEvent(const SDL_Event* event)
{
    if (!gTC.screenW || !gTC.screenH)
        return;

    switch (event->type)
    {
        case SDL_EVENT_FINGER_DOWN:
        {
            float nx = event->tfinger.x;
            float ny = event->tfinger.y;
            ProcessFingerDown(nx, ny, event->tfinger.fingerID);
            break;
        }

        case SDL_EVENT_FINGER_UP:
            ProcessFingerUp(event->tfinger.fingerID);
            break;

        case SDL_EVENT_FINGER_MOTION:
        {
            float nx = event->tfinger.x;
            float ny = event->tfinger.y;
            ProcessFingerMotion(nx, ny, event->tfinger.fingerID);
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION:
            // Ignore touch-synthesized mouse events on Android
            if (event->button.which == SDL_TOUCH_MOUSEID)
                return;
            break;

        default:
            break;
    }
}

// ============================================================
// END-OF-FRAME UPDATE
// ============================================================

void TouchControls_EndFrame(void)
{
    // Update button "was pressed" states
    for (int i = 0; i < gTC.numButtons; i++)
    {
        gTC.buttons[i].wasPressed = gTC.buttons[i].pressed;
    }

    // Get screen size
    extern SDL_Window* gSDLWindow;
    if (gSDLWindow)
    {
        SDL_GetWindowSizeInPixels(gSDLWindow, &gTC.screenW, &gTC.screenH);
    }

    // Read gyroscope if active
    if (gTC.gyroSensor && gTC.steeringMode == kSteeringMode_Gyroscope)
    {
        float data[3];
        if (SDL_GetSensorData(gTC.gyroSensor, data, 3))
        {
            // Use gyro angular velocity (data[1] = pitch around X axis = roll while in landscape)
            // Apply low-pass filter to remove noise, then scale to steering value.
            gTC.filteredRate = gTC.filteredRate * 0.8f + data[1] * 0.2f;

            float rate = gTC.filteredRate - gTC.gyroBias - gTC.gyroOffset;
            float steering = rate * 0.3f;
            steering = fmaxf(-1.0f, fminf(1.0f, steering));
            gTC.gyroValue = steering;
        }
    }

    // Build final steering
    if (gTC.steeringMode == kSteeringMode_Gyroscope && gTC.gyroSensor)
    {
        gTC.steering.x = gTC.gyroValue;
        gTC.steering.y = 0; // no forward/back from gyro
    }
    else
    {
        gTC.steering.x = gTC.joyDX;
        gTC.steering.y = gTC.joyDY;
    }
}

// ============================================================
// QUERIES
// ============================================================

TCVector2D TouchControls_GetSteering(void)
{
    return gTC.steering;
}

bool TouchControls_IsNeedPressed(int needID)
{
    for (int i = 0; i < gTC.numButtons; i++)
    {
        if (gTC.buttons[i].needID == needID && gTC.buttons[i].pressed)
            return true;
    }
    return false;
}

bool TouchControls_IsNeedPressedNew(int needID)
{
    for (int i = 0; i < gTC.numButtons; i++)
    {
        if (gTC.buttons[i].needID == needID
            && gTC.buttons[i].pressed
            && !gTC.buttons[i].wasPressed)
            return true;
    }
    return false;
}

void TouchControls_RecenterGyro(void)
{
    if (gTC.gyroSensor)
    {
        // Reset gyro reference
        gTC.gyroOffset = 0.0f;
        gTC.gyroValue = 0.0f;
        TC_LOG("Gyro recentered");
    }
}

void TouchControls_SetSteeringMode(SteeringMode mode)
{
    gTC.steeringMode = mode;
    // Reset joystick state when switching
    gTC.joyActive = false;
    gTC.joyFingerID = -1;
    gTC.joyDX = 0;
    gTC.joyDY = 0;
    gTC.joyCX = JOYSTICK_CENTER_X;
    gTC.joyCY = JOYSTICK_CENTER_Y;
    TC_LOG("Steering mode: %s", mode == kSteeringMode_Gyroscope ? "gyro" : "joystick");
}

SteeringMode TouchControls_GetSteeringMode(void)
{
    return gTC.steeringMode;
}

void TouchControls_SetGameMode(bool inGame)
{
    gTC.inGame = inGame;
    // Release all buttons on mode switch to avoid stuck inputs
    for (int i = 0; i < gTC.numGameButtons; i++)
        gTC.gameButtons[i].pressed = gTC.gameButtons[i].wasPressed = false;
    for (int i = 0; i < gTC.numMenuButtons; i++)
        gTC.menuButtons[i].pressed = gTC.menuButtons[i].wasPressed = false;
    if (inGame)
    {
        gTC.buttons = gTC.gameButtons;
        gTC.numButtons = gTC.numGameButtons;
    }
    else
    {
        gTC.buttons = gTC.menuButtons;
        gTC.numButtons = gTC.numMenuButtons;
    }
    TC_LOG("TouchControls mode: %s", inGame ? "game" : "menu");
}

bool TouchControls_GetGameMode(void)
{
    return gTC.inGame;
}

// ============================================================
// DRAWING
// ============================================================

// Max segments for circle drawing
#define MAX_CIRCLE_SEGMENTS 64

// Draw a filled circle using triangle fan (aspect-ratio corrected)
static void DrawCircleFilled(float cx, float cy, float r, int segments)
{
    if (segments > MAX_CIRCLE_SEGMENTS) segments = MAX_CIRCLE_SEGMENTS;
    // Correct for aspect ratio: in [0..1] screen space, y covers a taller portion
    // of the screen than x in landscape mode. To get circular dots in pixels:
    // ry = rx * (screenW / screenH)
    float ry = (gTC.screenH > 0) ? r * (float)gTC.screenW / (float)gTC.screenH : r;
    // triangle fan: center + (segments+1) rim points, 2 floats each
    float verts[2 + (MAX_CIRCLE_SEGMENTS + 1) * 2];
    int vi = 0;
    verts[vi++] = cx; verts[vi++] = cy;
    for (int i = 0; i <= segments; i++)
    {
        float a = (float)i / (float)segments * 2.0f * 3.14159265f;
        verts[vi++] = cx + cosf(a) * r;
        verts[vi++] = cy + sinf(a) * ry;
    }

    glEnableVertexAttribArray(0);
    // Use a simple temporary VBO
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vi * sizeof(float), verts, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, vi/2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);
}

// Draw a circle outline (aspect-ratio corrected)
static void DrawCircleOutline(float cx, float cy, float r, int segments)
{
    if (segments > MAX_CIRCLE_SEGMENTS) segments = MAX_CIRCLE_SEGMENTS;
    float ry = (gTC.screenH > 0) ? r * (float)gTC.screenW / (float)gTC.screenH : r;
    float verts[MAX_CIRCLE_SEGMENTS * 2];
    for (int i = 0; i < segments; i++)
    {
        float a = (float)i / (float)segments * 2.0f * 3.14159265f;
        verts[i*2+0] = cx + cosf(a) * r;
        verts[i*2+1] = cy + sinf(a) * ry;
    }

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINE_LOOP, 0, segments);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);
}

// Simple 2D shader for drawing touch controls overlay
static GLuint gTC_Program = 0;
static GLint gTC_uColor = -1;
static GLint gTC_uOffset = -1;

static const char* kTC_VertSrc =
"#version 300 es\n"
"in vec2 a_pos;\n"
"uniform vec2 u_offset;\n"
"void main() {\n"
"    vec2 ndc = (a_pos + u_offset) * 2.0 - 1.0;\n"
"    ndc.y = -ndc.y;\n"  // flip Y (screen Y increases down, NDC Y increases up)
"    gl_Position = vec4(ndc, 0.0, 1.0);\n"
"}\n";

static const char* kTC_FragSrc =
"#version 300 es\n"
"precision mediump float;\n"
"uniform vec4 u_color;\n"
"out vec4 fragColor;\n"
"void main() { fragColor = u_color; }\n";

static GLuint TC_CompileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        TC_LOG("TC shader error: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static void TC_InitShader(void)
{
    if (gTC_Program) return;

    GLuint vert = TC_CompileShader(GL_VERTEX_SHADER,   kTC_VertSrc);
    GLuint frag = TC_CompileShader(GL_FRAGMENT_SHADER, kTC_FragSrc);
    if (!vert || !frag) return;

    gTC_Program = glCreateProgram();
    glBindAttribLocation(gTC_Program, 0, "a_pos");
    glAttachShader(gTC_Program, vert);
    glAttachShader(gTC_Program, frag);
    glLinkProgram(gTC_Program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    // Check link status. Using an unlinked program with glUseProgram generates GL_INVALID_OPERATION.
    {
        GLint ok;
        glGetProgramiv(gTC_Program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(gTC_Program, sizeof(log), NULL, log);
            TC_LOG("TC program link error: %s", log);
            glDeleteProgram(gTC_Program);
            gTC_Program = 0;
            return;
        }
    }

    gTC_uColor  = glGetUniformLocation(gTC_Program, "u_color");
    gTC_uOffset = glGetUniformLocation(gTC_Program, "u_offset");
}

static void TC_SetColor(float r, float g, float b, float a)
{
    glUniform4f(gTC_uColor, r, g, b, a);
}

static void TC_SetOffset(float ox, float oy)
{
    glUniform2f(gTC_uOffset, ox, oy);
}

void TouchControls_Draw(void)
{
    if (!gTC.screenW || !gTC.screenH) return;

    TC_InitShader();
    if (!gTC_Program) return;

    // Save GL state
    GLint savedProgram;
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);

    glUseProgram(gTC_Program);
    TC_SetOffset(0, 0);

    // Disable depth test and cull face for 2D overlay
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Vertex array
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    float ar = (gTC.screenH > 0) ? (float)gTC.screenW / (float)gTC.screenH : 1.0f;
    float rx = JOYSTICK_RADIUS;
    float ry_hit = JOYSTICK_RADIUS * ar;

    if (gTC.inGame)
    {
        // ---- Draw joystick (game mode only) ----
        float jcx = gTC.joyActive ? gTC.joyCX : JOYSTICK_CENTER_X;
        float jcy = gTC.joyActive ? gTC.joyCY : JOYSTICK_CENTER_Y;

        TC_SetColor(0.3f, 0.3f, 0.3f, 0.15f);
        DrawCircleFilled(jcx, jcy, rx, 32);
        TC_SetColor(0.7f, 0.7f, 0.7f, 0.4f);
        DrawCircleOutline(jcx, jcy, rx, 32);

        if (gTC.steeringMode == kSteeringMode_Joystick)
        {
            float tx = jcx + gTC.joyDX * rx;
            float ty = jcy + gTC.joyDY * ry_hit;
            TC_SetColor(0.5f, 0.7f, 1.0f, 0.5f);
            DrawCircleFilled(tx, ty, rx * 0.4f, 16);
            TC_SetColor(0.8f, 0.9f, 1.0f, 0.6f);
            DrawCircleOutline(tx, ty, rx * 0.4f, 16);
        }

        // ---- Draw game action buttons ----
        for (int i = 0; i < gTC.numButtons; i++)
        {
            TouchButton* btn = &gTC.buttons[i];
            if (btn->pressed)
                TC_SetColor(0.6f, 0.8f, 1.0f, 0.5f);
            else
                TC_SetColor(0.3f, 0.3f, 0.3f, 0.2f);
            DrawCircleFilled(btn->cx, btn->cy, btn->radius, 24);
            TC_SetColor(0.7f, 0.7f, 0.7f, 0.5f);
            DrawCircleOutline(btn->cx, btn->cy, btn->radius, 24);
        }

        // ---- Draw mode toggle button (joystick/gyro) ----
        {
            bool isGyro = (gTC.steeringMode == kSteeringMode_Gyroscope);
            TC_SetColor(isGyro ? 0.2f : 0.5f, isGyro ? 0.8f : 0.5f, isGyro ? 0.2f : 0.5f, 0.35f);
            DrawCircleFilled(gTC.modeBtnCX, gTC.modeBtnCY, gTC.modeBtnR, 20);
            TC_SetColor(0.8f, 0.8f, 0.8f, 0.5f);
            DrawCircleOutline(gTC.modeBtnCX, gTC.modeBtnCY, gTC.modeBtnR, 20);
        }

        // ---- Draw gyro recenter button (only in gyro mode) ----
        if (gTC.steeringMode == kSteeringMode_Gyroscope && gTC.gyroSensor)
        {
            TC_SetColor(0.2f, 0.6f, 1.0f, 0.4f);
            DrawCircleFilled(gTC.gyroRecenterCX, gTC.gyroRecenterCY, gTC.gyroRecenterR, 20);
            TC_SetColor(0.8f, 0.9f, 1.0f, 0.6f);
            DrawCircleOutline(gTC.gyroRecenterCX, gTC.gyroRecenterCY, gTC.gyroRecenterR, 20);
        }
    }
    else
    {
        // ---- Menu navigation buttons ----
        // Use distinct colors to hint purpose:
        //   UIUp    = green-ish (top circle)
        //   UIDown  = red-ish   (bottom circle)
        //   Confirm = blue-ish  (middle circle)
        //   UIBack  = grey      (corner circle)
        float menuColors[4][4] = {
            {0.2f, 0.8f, 0.3f, 0.45f},  // [0] UIUp    = green
            {0.3f, 0.5f, 1.0f, 0.45f},  // [1] UIConfirm = blue
            {0.9f, 0.3f, 0.2f, 0.45f},  // [2] UIDown   = red
            {0.5f, 0.5f, 0.5f, 0.40f},  // [3] UIBack   = grey
        };
        for (int i = 0; i < gTC.numMenuButtons; i++)
        {
            TouchButton* btn = &gTC.menuButtons[i];
            float* c = menuColors[i];
            if (btn->pressed)
                TC_SetColor(c[0]*1.4f > 1.f ? 1.f : c[0]*1.4f,
                            c[1]*1.4f > 1.f ? 1.f : c[1]*1.4f,
                            c[2]*1.4f > 1.f ? 1.f : c[2]*1.4f, 0.7f);
            else
                TC_SetColor(c[0], c[1], c[2], c[3]);
            DrawCircleFilled(btn->cx, btn->cy, btn->radius, 24);
            TC_SetColor(c[0], c[1], c[2], 0.7f);
            DrawCircleOutline(btn->cx, btn->cy, btn->radius, 24);
        }
    }

    // Restore state
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);

    // Restore GL state
    if (depthTestWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFaceWasEnabled)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);
    if (!blendWasEnabled)    glDisable(GL_BLEND);
    glUseProgram(savedProgram);

    // Drain any GL errors generated by TouchControls drawing so they don't
    // accumulate into the next frame and get mis-attributed to other GL calls.
    {
        GLenum _e;
        while ((_e = glGetError()) != GL_NO_ERROR)
            TC_LOG("TouchControls_Draw: flushing GL error 0x%x", (unsigned)_e);
    }
}

#endif // __ANDROID__
