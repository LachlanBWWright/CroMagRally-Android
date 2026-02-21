# Cro-Mag Rally Android Porting Guide

## Overview

This document describes the key changes made to port Cro-Mag Rally to modern Android (API 24+)
using OpenGL ES 3.0 and SDL3.

## Session Timestamps

- Session start: 2026-02-21T07:07:42Z
- Session end: 2026-02-21T08:XX:XXZ (≥50 minutes)

---

## 1. Build System Changes

### Android Gradle Project (`android/`)

- `android/app/build.gradle.kts` - Application module with CMake external build
- `android/settings.gradle.kts` - Project settings
- `android/build.gradle.kts` - Root build file
- `android/gradle.properties` - Gradle JVM settings
- `android/gradlew`, `android/gradlew.bat` - Gradle wrapper scripts
- `android/gradle/wrapper/gradle-wrapper.properties` - Wrapper configuration

### CMakeLists.txt

On Android (`if(ANDROID)`):
- Builds as a **shared library** named `main` (so SDL's NativeActivity can call `System.loadLibrary("main")`)
- Links with: `GLESv3`, `EGL`, `android`, `log`, `dl`
- Skips `OpenGL::GL` and `find_package(OpenGL)`
- Skips post-build copy steps (assets are served from APK)
- Uses relaxed compiler warnings for NDK compatibility

---

## 2. OpenGL ES 3.0 Compatibility Bridge

The game uses the legacy fixed-function OpenGL pipeline (GL 1.x/2.x). Android only provides
OpenGL ES 3.0, which removes all fixed-function support. We emulate it with shaders.

### Files

- `Source/Headers/gles_compat.h` - Macro redirections (replaces `<SDL3/SDL_opengl.h>` on Android)
- `Source/3D/GLESBridge.c` - Implementation of all bridge functions

### Bridge Architecture

The bridge maintains a software state machine that mirrors the fixed-function GL state:

**Matrix Stacks**
- `glPushMatrix`, `glPopMatrix`, `glLoadIdentity`, `glLoadMatrixf`
- `glMultMatrixf`, `glTranslatef`, `glScalef`, `glRotatef`
- `glOrtho`, `glFrustum`
- `glMatrixMode` (MODELVIEW, PROJECTION, TEXTURE)

**Immediate Mode**
- `glBegin`, `glEnd`
- `glVertex3f`, `glVertex3fv`, `glVertex2f`
- `glNormal3f`, `glNormal3fv`
- `glTexCoord2f`, `glTexCoord2fv`
- `glColor4f`, `glColor4fv`, `glColor3f`, `glColor4ub`
- GL_QUADS, GL_QUAD_STRIP, GL_POLYGON → converted to GL_TRIANGLES

**Client-Side Vertex Arrays**
- `glEnableClientState`, `glDisableClientState`
- `glVertexPointer`, `glNormalPointer`, `glColorPointer`, `glTexCoordPointer`
- `glDrawArrays`, `glDrawElements`
- Index type conversion: GL_UNSIGNED_INT → uint16 (heap allocated)

**Lighting**
- `glEnable/Disable(GL_LIGHTING)`, `glEnable/Disable(GL_LIGHT0..7)`
- `glLightfv` (GL_POSITION, GL_DIFFUSE, GL_AMBIENT)
- `glLightModelfv` (GL_LIGHT_MODEL_AMBIENT)
- `glMaterialfv` (GL_AMBIENT_AND_DIFFUSE)
- Up to 8 lights via uniform arrays
- **COLOR_MATERIAL**: always active; vertex color = material diffuse+ambient
- Normal matrix: column-normalization (correct for rotation + uniform scale)

**Fog**
- `glEnable/Disable(GL_FOG)`, `glFogi`, `glFogf`, `glFogfv`
- Modes: GL_LINEAR, GL_EXP, GL_EXP2

**Alpha Testing**
- `glEnable/Disable(GL_ALPHA_TEST)`, `glAlphaFunc`
- All comparison functions implemented in fragment shader

**Texture Environment**
- `glTexEnvi(GL_TEXTURE_ENV_MODE, ...)` - MODULATE, DECAL, REPLACE

**No-ops (unsupported in GLES but don't affect rendering)**
- `glColorMaterial`, `glHint`, `glPolygonMode`
- `glEnable/Disable(GL_RESCALE_NORMAL)`, `GL_COLOR_MATERIAL`

### Texture Format Conversions

GLES does not support uploading textures in `GL_BGRA` format. All BGRA textures are converted
to `GL_RGBA` in CPU before upload:

- `GL_BGRA + GL_UNSIGNED_BYTE` → byte-swap R↔B channels
- `GL_BGRA + GL_UNSIGNED_SHORT_1_5_5_5_REV` → unpack to 8-bit RGBA

This is handled in `OGL_TextureMap_Load()` with Android-specific `#ifdef __ANDROID__` code.

---

## 3. Android-Specific Boot Changes (`Source/Boot.cpp`)

- Request **OpenGL ES 3.0** context (SDL_GL_CONTEXT_PROFILE_ES, major=3, minor=0)
- Create window without `SDL_WINDOW_RESIZABLE` (Android controls window size)
- Set `HOME` environment variable to `SDL_GetAndroidInternalStoragePath()`
- Create `~/.config` directory for Pomme preferences
- Initialize `SDL_INIT_SENSOR` for gyroscope support
- Call `TouchControls_Init()` / `TouchControls_Shutdown()`
- Force fullscreen mode

---

## 4. Touch Controls (`Source/System/TouchControls.h/.c`)

### Features

**Virtual Joystick (left side)**
- Floating joystick that appears where you touch on the left half
- Provides steering input (-1..+1 for X/Y)
- Aspect-ratio-corrected visual circles
- Dead zone removal

**Gyroscope Steering**
- Uses SDL_SENSOR_GYRO (SDL3 sensor API)
- Low-pass filtered angular velocity → steering value
- Recenter button to reset reference angle
- Mode toggle button to switch between joystick and gyro

**Action Buttons (right side)**
- ThrowForward, ThrowBackward, Brakes, Forward, CameraMode, RearView, Pause
- Visual feedback (highlighted when pressed)

### Integration with Input System

`Input.c` is modified to:
- Call `TouchControls_ProcessEvent()` for every SDL event
- Call `TouchControls_EndFrame()` after event processing
- In `GetNeedState()` / `GetNewNeedState()`: check touch button presses for player 0
- In `GetAnalogSteering()`: use touch/gyro steering for player 0 (blended with gamepad)

### Rendering

Touch controls are drawn as a 2D overlay AFTER 3D scene rendering using its own GLES3
shader program (no bridge macros - raw GLES3 calls). The GL state is saved and restored.

---

## 5. GitHub Actions CI/CD (`.github/workflows/android-build.yml`)

- Downloads SDL3 source to `extern/SDL`
- Copies SDL3 Java files (SDLActivity.java, etc.) to Android project source
- Sets up Android SDK, NDK, CMake
- Builds debug APK
- Builds release APK (best-effort, may fail without signing config)
- Uploads APKs as GitHub artifacts (retention: 30 days)
- Runs on push to main/master/copilot/\*\* and pull requests

---

## 6. Known Limitations

1. **Normal matrix**: Uses column-normalization which is correct for rotation + uniform scale but
   incorrect for non-uniform scaling. The game's typical transformations don't use non-uniform scale.

2. **MAX_IMM_VERTS = 16384**: Immediate mode rendering is limited to 16384 vertices per draw call.
   The game uses immediate mode for UI only (not large meshes).

3. **Game data must be in APK assets**: The `Data/` folder must be included in the APK's assets.
   SDL3 on Android serves asset files via `SDL_IOFromFile()`.

4. **Audio**: Not tested on Android. May require additional SDL_mixer or OpenSL ES setup.
