// gles_compat.h
// OpenGL ES 3.0 compatibility layer for Cro-Mag Rally Android port.
// On Android, fixed-function OpenGL calls are redirected to GLESBridge.c
// which emulates them using GLES 3.0 shaders and VBOs.

#pragma once

#ifdef __ANDROID__

#include <GLES3/gl3.h>
#include <math.h>

// GLdouble is not defined in GLES3 headers
typedef double GLdouble;

// Desktop GL constants absent from GLES3
#ifndef GL_BGRA
#define GL_BGRA                         0x80E1
#endif
#ifndef GL_BGR
#define GL_BGR                          0x80E0
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE                    0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA              0x190A
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV     0x8367
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV   0x8366
#endif

// Fixed-function state enums
#define GL_LIGHTING                     0x0B50
#define GL_FOG                          0x0B60
#define GL_FOG_DENSITY                  0x0B62
#define GL_FOG_START                    0x0B63
#define GL_FOG_END                      0x0B64
#define GL_FOG_MODE                     0x0B65
#define GL_FOG_COLOR                    0x0B66
#define GL_FOG_HINT                     0x0C54
#define GL_LINEAR                       0x2601
#define GL_EXP                          0x0800
#define GL_EXP2                         0x0801
#define GL_NORMALIZE                    0x0BA1
#define GL_RESCALE_NORMAL               0x803A
#define GL_COLOR_MATERIAL               0x0B57
#define GL_LIGHT_MODEL_AMBIENT          0x0B53
#define GL_LIGHT0                       0x4000
#define GL_LIGHT1                       0x4001
#define GL_LIGHT2                       0x4002
#define GL_LIGHT3                       0x4003
#define GL_AMBIENT                      0x1200
#define GL_DIFFUSE                      0x1201
#define GL_SPECULAR                     0x1202
#define GL_POSITION                     0x1203
#define GL_EMISSION                     0x1600
#define GL_AMBIENT_AND_DIFFUSE          0x1602
#define GL_SHININESS                    0x1601
#define GL_FRONT_AND_BACK               0x0408
#define GL_FRONT                        0x0404
#define GL_BACK                         0x0405
#define GL_ALPHA_TEST                   0x0BC0
#define GL_ALPHA_TEST_FUNC              0x0BC1
#define GL_ALPHA_TEST_REF               0x0BC2
#define GL_NOTEQUAL                     0x0205
#define GL_GREATER                      0x0204
#define GL_GEQUAL                       0x0206
#define GL_LESS                         0x0201
#define GL_LEQUAL                       0x0203
#define GL_EQUAL                        0x0202
#define GL_ALWAYS                       0x0207
#define GL_NEVER                        0x0200
#define GL_TEXTURE_2D                   0x0DE1
#define GL_TEXTURE_ENV                  0x2300
#define GL_TEXTURE_ENV_MODE             0x2200
#define GL_MODULATE                     0x2100
#define GL_DECAL                        0x2101
#define GL_REPLACE                      0x1E01
#define GL_BLEND_SRC                    0x0BE1
#define GL_BLEND_DST                    0x0BE0
#define GL_CURRENT_COLOR                0x0B00
#define GL_VERTEX_ARRAY                 0x8074
#define GL_NORMAL_ARRAY                 0x8075
#define GL_COLOR_ARRAY                  0x8076
#define GL_TEXTURE_COORD_ARRAY          0x8078
#define GL_MATRIX_MODE                  0x0BA0
#define GL_MODELVIEW                    0x1700
#define GL_PROJECTION                   0x1701
#define GL_TEXTURE                      0x1702
#define GL_QUADS                        0x0007
#define GL_QUAD_STRIP                   0x0008
#define GL_POLYGON                      0x0009
#define GL_LINE_LOOP                    0x0002
#define GL_NICEST                       0x1102
#define GL_FASTEST                      0x1101
#define GL_HINT_BIT                     0x00008000

// Polygon mode (not in GLES) - we stub it out
#define GL_FILL                         0x1B02
#define GL_LINE                         0x1B01
#define GL_POINT                        0x1B00

// Forward declarations for bridge functions
#ifdef __cplusplus
extern "C" {
#endif

void bridge_Init(void);
void bridge_Shutdown(void);

// Immediate mode
void bridge_Begin(GLenum mode);
void bridge_End(void);
void bridge_Vertex2f(GLfloat x, GLfloat y);
void bridge_Vertex3f(GLfloat x, GLfloat y, GLfloat z);
void bridge_Vertex3fv(const GLfloat *v);
void bridge_Normal3f(GLfloat x, GLfloat y, GLfloat z);
void bridge_Normal3fv(const GLfloat *v);
void bridge_TexCoord2f(GLfloat s, GLfloat t);
void bridge_TexCoord2fv(const GLfloat *v);
void bridge_Color3f(GLfloat r, GLfloat g, GLfloat b);
void bridge_Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void bridge_Color4fv(const GLfloat *v);
void bridge_Color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);

// Matrix operations
void bridge_MatrixMode(GLenum mode);
void bridge_PushMatrix(void);
void bridge_PopMatrix(void);
void bridge_LoadIdentity(void);
void bridge_LoadMatrixf(const GLfloat *m);
void bridge_MultMatrixf(const GLfloat *m);
void bridge_Translatef(GLfloat x, GLfloat y, GLfloat z);
void bridge_Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void bridge_Scalef(GLfloat x, GLfloat y, GLfloat z);
void bridge_Ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void bridge_Frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);

// Lighting
void bridge_Lightfv(GLenum light, GLenum pname, const GLfloat *params);
void bridge_LightModelfv(GLenum pname, const GLfloat *params);
void bridge_Materialfv(GLenum face, GLenum pname, const GLfloat *params);

// Fog
void bridge_Fogfv(GLenum pname, const GLfloat *params);
void bridge_Fogf(GLenum pname, GLfloat param);
void bridge_Fogi(GLenum pname, GLint param);

// Alpha test
void bridge_AlphaFunc(GLenum func, GLfloat ref);

// Texture environment
void bridge_TexEnvi(GLenum target, GLenum pname, GLint param);

// State enable/disable (re-routed for GLES unsupported states)
void bridge_Enable(GLenum cap);
void bridge_Disable(GLenum cap);
GLboolean bridge_IsEnabled(GLenum cap);
void bridge_GetFloatv(GLenum pname, GLfloat *params);
void bridge_GetIntegerv(GLenum pname, GLint *params);
void bridge_GetBooleanv(GLenum pname, GLboolean *params);

// Client-side vertex arrays (upload to VBOs)
void bridge_EnableClientState(GLenum array);
void bridge_DisableClientState(GLenum array);
void bridge_VertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void bridge_NormalPointer(GLenum type, GLsizei stride, const void *ptr);
void bridge_ColorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void bridge_TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void bridge_DrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void bridge_DrawArrays(GLenum mode, GLint first, GLsizei count);

// Color4fv alias (needed for state stack restore)
// Blend function (tracked for GL_BLEND_SRC/DST queries via GetIntegerv)
void bridge_BlendFunc(GLenum sfactor, GLenum dfactor);

// Hint (no-op stub)
static inline void bridge_Hint(GLenum target, GLenum mode) { (void)target; (void)mode; }

// PolygonMode (stub - no wireframe in GLES)
static inline void bridge_PolygonMode(GLenum face, GLenum mode) { (void)face; (void)mode; }

// ColorMaterial (no-op, always enabled in shader)
static inline void bridge_ColorMaterial(GLenum face, GLenum mode) { (void)face; (void)mode; }

#ifdef __cplusplus
}
#endif

// ============================================================
// REDIRECT MACROS: map all fixed-function calls to bridge
// ============================================================

// Override glEnable/glDisable/glIsEnabled to intercept GLES-unsupported caps
#define glEnable                bridge_Enable
#define glDisable               bridge_Disable
#define glIsEnabled             bridge_IsEnabled
#define glGetFloatv             bridge_GetFloatv
#define glGetIntegerv           bridge_GetIntegerv
#define glGetBooleanv           bridge_GetBooleanv

// Immediate mode
#define glBegin                 bridge_Begin
#define glEnd                   bridge_End
#define glVertex2f              bridge_Vertex2f
#define glVertex3f              bridge_Vertex3f
#define glVertex3fv             bridge_Vertex3fv
#define glNormal3f              bridge_Normal3f
#define glNormal3fv             bridge_Normal3fv
#define glTexCoord2f            bridge_TexCoord2f
#define glTexCoord2fv           bridge_TexCoord2fv
#define glColor3f(r,g,b)        bridge_Color3f(r,g,b)
#define glColor4f               bridge_Color4f
#define glColor4fv              bridge_Color4fv
#define glColor4ub              bridge_Color4ub

// Matrix
#define glMatrixMode            bridge_MatrixMode
#define glPushMatrix            bridge_PushMatrix
#define glPopMatrix             bridge_PopMatrix
#define glLoadIdentity          bridge_LoadIdentity
#define glLoadMatrixf           bridge_LoadMatrixf
#define glMultMatrixf           bridge_MultMatrixf
#define glTranslatef            bridge_Translatef
#define glRotatef               bridge_Rotatef
#define glScalef                bridge_Scalef
#define glOrtho(l,r,b,t,n,f)    bridge_Ortho((GLdouble)(l),(GLdouble)(r),(GLdouble)(b),(GLdouble)(t),(GLdouble)(n),(GLdouble)(f))
#define glFrustum(l,r,b,t,n,f)  bridge_Frustum((GLdouble)(l),(GLdouble)(r),(GLdouble)(b),(GLdouble)(t),(GLdouble)(n),(GLdouble)(f))

// Lighting
#define glLightfv               bridge_Lightfv
#define glLightModelfv          bridge_LightModelfv
#define glMaterialfv            bridge_Materialfv
#define glColorMaterial         bridge_ColorMaterial

// Fog
#define glFogfv                 bridge_Fogfv
#define glFogf                  bridge_Fogf
#define glFogi                  bridge_Fogi

// Alpha test
#define glAlphaFunc             bridge_AlphaFunc

// Texture env
#define glTexEnvi               bridge_TexEnvi

// Client arrays
#define glEnableClientState     bridge_EnableClientState
#define glDisableClientState    bridge_DisableClientState
#define glVertexPointer         bridge_VertexPointer
#define glNormalPointer         bridge_NormalPointer
#define glColorPointer          bridge_ColorPointer
#define glTexCoordPointer       bridge_TexCoordPointer
#define glDrawElements          bridge_DrawElements
#define glDrawArrays            bridge_DrawArrays

// Misc stubs
#define glHint                  bridge_Hint
#define glPolygonMode           bridge_PolygonMode

// Blend function tracking
#define glBlendFunc             bridge_BlendFunc

#else  // !__ANDROID__

// On non-Android, include the standard OpenGL header
#include <SDL3/SDL_opengl.h>

// On non-Android, OGL_Boot/Shutdown don't need to init the bridge
#define bridge_Init()           ((void)0)
#define bridge_Shutdown()       ((void)0)

#endif // __ANDROID__
