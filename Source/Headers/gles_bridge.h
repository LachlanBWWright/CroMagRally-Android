// gles_bridge.h
// Bridge declarations for fixed-function OpenGL emulation on OpenGL ES 3.0.

#pragma once

#ifdef __ANDROID__

#include <GLES3/gl3.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize/shutdown the GLES bridge (call after creating GLES context)
void bridge_Init(void);
void bridge_Shutdown(void);

// Called at start of each frame to sync uniforms
void bridge_BeforeDrawScene(void);

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
void bridge_Ortho(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
void bridge_Frustum(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);

// Matrix upload shortcuts (called from OGL_Camera_SetPlacementAndUpdateMatrices)
void bridge_SetProjectionMatrix(const GLfloat *m);
void bridge_SetModelViewMatrix(const GLfloat *m);

// Immediate mode
void bridge_Begin(GLenum mode);
void bridge_End(void);
void bridge_Vertex2f(GLfloat x, GLfloat y);
void bridge_Vertex3f(GLfloat x, GLfloat y, GLfloat z);
void bridge_Vertex3fv(const GLfloat *v);
void bridge_Normal3f(GLfloat x, GLfloat y, GLfloat z);
void bridge_Normal3fv(const GLfloat *v);
void bridge_TexCoord2f(GLfloat s, GLfloat t);
void bridge_Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void bridge_Color4fv(const GLfloat *v);
void bridge_Color4ubv(const GLubyte *v);

// Client-state vertex arrays (legacy)
void bridge_EnableClientState(GLenum cap);
void bridge_DisableClientState(GLenum cap);
void bridge_VertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void bridge_NormalPointer(GLenum type, GLsizei stride, const void *ptr);
void bridge_TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void bridge_ColorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void bridge_DrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void bridge_DrawArrays(GLenum mode, GLint first, GLsizei count);

// Lighting
void bridge_Lightfv(GLenum light, GLenum pname, const GLfloat *params);
void bridge_Lightf(GLenum light, GLenum pname, GLfloat param);
void bridge_LightModelfv(GLenum pname, const GLfloat *params);
void bridge_Materialfv(GLenum face, GLenum pname, const GLfloat *params);
void bridge_Materialf(GLenum face, GLenum pname, GLfloat param);
void bridge_ColorMaterial(GLenum face, GLenum mode);

// Fog
void bridge_Fogi(GLenum pname, GLint param);
void bridge_Fogf(GLenum pname, GLfloat param);
void bridge_Fogfv(GLenum pname, const GLfloat *params);

// Alpha test
void bridge_AlphaFunc(GLenum func, GLfloat ref);

// Enable/Disable (intercepts fixed-function caps, passes others to GLES)
void bridge_Enable(GLenum cap);
void bridge_Disable(GLenum cap);
GLboolean bridge_IsEnabled(GLenum cap);

// GL getters (intercept params not supported in GLES3)
void bridge_GetFloatv(GLenum pname, GLfloat *data);
void bridge_GetIntegerv(GLenum pname, GLint *data);

// Stubs
void bridge_PolygonMode(GLenum face, GLenum mode);
void bridge_Hint(GLenum target, GLenum hint);

// Texture format conversion helper (called from OGL_TextureMap_Load)
// Converts GL_BGRA+GL_UNSIGNED_SHORT_1_5_5_5_REV → GL_RGBA+GL_UNSIGNED_BYTE
void *bridge_ConvertBGRA1555toRGBA8(const void *src, int width, int height);
void bridge_FreeConvertedPixels(void *ptr);

#ifdef __cplusplus
}
#endif

#endif // __ANDROID__
