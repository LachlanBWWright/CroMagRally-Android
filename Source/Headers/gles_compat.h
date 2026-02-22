// gles_compat.h
// OpenGL ES 3.0 compatibility layer for Cro-Mag Rally.
// Redirects fixed-function OpenGL 1.x/2.x calls to the GLES bridge.

#pragma once

#ifdef __ANDROID__

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

// GLdouble is not defined in GLES3 headers
typedef double GLdouble;
typedef double GLclampd;

// Desktop GL enums missing from GLES3
#ifndef GL_BGRA
#define GL_BGRA                     0x80E1
#endif
#ifndef GL_BGR
#define GL_BGR                      0x80E0
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV  0x8366
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV    0x8367
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE                0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA          0x190A
#endif
#ifndef GL_INTENSITY
#define GL_INTENSITY                0x8049
#endif
#ifndef GL_ALPHA
#define GL_ALPHA                    0x1906
#endif

// Fixed-function lighting enums
#ifndef GL_LIGHTING
#define GL_LIGHTING                 0x0B50
#endif
#ifndef GL_LIGHT0
#define GL_LIGHT0                   0x4000
#endif
#ifndef GL_LIGHT1
#define GL_LIGHT1                   0x4001
#endif
#ifndef GL_LIGHT2
#define GL_LIGHT2                   0x4002
#endif
#ifndef GL_LIGHT3
#define GL_LIGHT3                   0x4003
#endif
#ifndef GL_AMBIENT
#define GL_AMBIENT                  0x1200
#endif
#ifndef GL_DIFFUSE
#define GL_DIFFUSE                  0x1201
#endif
#ifndef GL_SPECULAR
#define GL_SPECULAR                 0x1202
#endif
#ifndef GL_POSITION
#define GL_POSITION                 0x1203
#endif
#ifndef GL_EMISSION
#define GL_EMISSION                 0x1600
#endif
#ifndef GL_AMBIENT_AND_DIFFUSE
#define GL_AMBIENT_AND_DIFFUSE      0x1602
#endif
#ifndef GL_LIGHT_MODEL_AMBIENT
#define GL_LIGHT_MODEL_AMBIENT      0x0B53
#endif
#ifndef GL_LIGHT_MODEL_TWO_SIDE
#define GL_LIGHT_MODEL_TWO_SIDE     0x0B52
#endif
#ifndef GL_NORMALIZE
#define GL_NORMALIZE                0x0BA1
#endif
#ifndef GL_RESCALE_NORMAL
#define GL_RESCALE_NORMAL           0x803A
#endif
#ifndef GL_COLOR_MATERIAL
#define GL_COLOR_MATERIAL           0x0B57
#endif
#ifndef GL_FRONT_AND_BACK
#define GL_FRONT_AND_BACK           0x0408
#endif
#ifndef GL_FRONT
#define GL_FRONT                    0x0404
#endif

// Fog enums
#ifndef GL_FOG
#define GL_FOG                      0x0B60
#endif
#ifndef GL_FOG_MODE
#define GL_FOG_MODE                 0x0B65
#endif
#ifndef GL_FOG_DENSITY
#define GL_FOG_DENSITY              0x0B62
#endif
#ifndef GL_FOG_START
#define GL_FOG_START                0x0B63
#endif
#ifndef GL_FOG_END
#define GL_FOG_END                  0x0B64
#endif
#ifndef GL_FOG_COLOR
#define GL_FOG_COLOR                0x0B66
#endif
#ifndef GL_LINEAR
#define GL_LINEAR                   0x2601
#endif
#ifndef GL_EXP
#define GL_EXP                      0x0800
#endif
#ifndef GL_EXP2
#define GL_EXP2                     0x0801
#endif

// Alpha test enums
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST               0x0BC0
#endif
#ifndef GL_NEVER
#define GL_NEVER                    0x0200
#endif
#ifndef GL_LESS
#define GL_LESS                     0x0201
#endif
#ifndef GL_EQUAL
#define GL_EQUAL                    0x0202
#endif
#ifndef GL_LEQUAL
#define GL_LEQUAL                   0x0203
#endif
#ifndef GL_GREATER
#define GL_GREATER                  0x0204
#endif
#ifndef GL_NOTEQUAL
#define GL_NOTEQUAL                 0x0205
#endif
#ifndef GL_GEQUAL
#define GL_GEQUAL                   0x0206
#endif
#ifndef GL_ALWAYS
#define GL_ALWAYS                   0x0207
#endif

// Matrix enums
#ifndef GL_MODELVIEW
#define GL_MODELVIEW                0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION               0x1701
#endif
#ifndef GL_TEXTURE
#define GL_TEXTURE                  0x1702
#endif

// Immediate mode primitives
#ifndef GL_QUADS
#define GL_QUADS                    0x0007
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP               0x0008
#endif
#ifndef GL_POLYGON
#define GL_POLYGON                  0x0009
#endif

// Client state enums
#ifndef GL_VERTEX_ARRAY
#define GL_VERTEX_ARRAY             0x8074
#endif
#ifndef GL_NORMAL_ARRAY
#define GL_NORMAL_ARRAY             0x8075
#endif
#ifndef GL_COLOR_ARRAY
#define GL_COLOR_ARRAY              0x8076
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
#define GL_TEXTURE_COORD_ARRAY      0x8078
#endif

// Misc
#ifndef GL_RGB5_A1
#define GL_RGB5_A1                  0x8057
#endif
#ifndef GL_FOG_HINT
#define GL_FOG_HINT                 0x0C54
#endif
#ifndef GL_CLIP_VOLUME_CLIPPING_HINT_EXT
#define GL_CLIP_VOLUME_CLIPPING_HINT_EXT  0x80F0
#endif
#ifndef GL_POLYGON_OFFSET_FILL
// already in GLES3
#endif

// Polygon mode constants (no-ops in GLES)
#ifndef GL_LINE
#define GL_LINE                     0x1B01
#endif
#ifndef GL_FILL
#define GL_FILL                     0x1B02
#endif
#ifndef GL_POINT
#define GL_POINT                    0x1B00
#endif

// Include the bridge declarations
#include "gles_bridge.h"

// ---- Redirect fixed-function calls to bridge ----

// Matrix operations
#define glMatrixMode                bridge_MatrixMode
#define glPushMatrix                bridge_PushMatrix
#define glPopMatrix                 bridge_PopMatrix
#define glLoadIdentity              bridge_LoadIdentity
#define glLoadMatrixf               bridge_LoadMatrixf
#define glMultMatrixf               bridge_MultMatrixf
#define glTranslatef                bridge_Translatef
#define glRotatef                   bridge_Rotatef
#define glScalef                    bridge_Scalef
#define glOrtho(l,r,b,t,n,f)        bridge_Ortho((float)(l),(float)(r),(float)(b),(float)(t),(float)(n),(float)(f))
#define glFrustum(l,r,b,t,n,f)      bridge_Frustum((float)(l),(float)(r),(float)(b),(float)(t),(float)(n),(float)(f))

// Immediate mode
#define glBegin                     bridge_Begin
#define glEnd                       bridge_End
#define glVertex2f                  bridge_Vertex2f
#define glVertex3f                  bridge_Vertex3f
#define glVertex3fv                 bridge_Vertex3fv
#define glNormal3f                  bridge_Normal3f
#define glNormal3fv                 bridge_Normal3fv
#define glTexCoord2f                bridge_TexCoord2f
#define glColor3f(r,g,b)            bridge_Color4f(r,g,b,1.0f)
#define glColor4f                   bridge_Color4f
#define glColor4fv                  bridge_Color4fv
#define glColor4ub(r,g,b,a)         bridge_Color4f((r)/255.0f,(g)/255.0f,(b)/255.0f,(a)/255.0f)
#define glColor4ubv                 bridge_Color4ubv

// Client state arrays (legacy)
#define glEnableClientState         bridge_EnableClientState
#define glDisableClientState        bridge_DisableClientState
#define glVertexPointer             bridge_VertexPointer
#define glNormalPointer             bridge_NormalPointer
#define glTexCoordPointer           bridge_TexCoordPointer
#define glColorPointer              bridge_ColorPointer
#define glDrawElements              bridge_DrawElements
#define glDrawArrays                bridge_DrawArrays

// Lighting
#define glLightfv                   bridge_Lightfv
#define glLightf                    bridge_Lightf
#define glLightModelfv              bridge_LightModelfv
#define glMaterialfv                bridge_Materialfv
#define glMaterialf                 bridge_Materialf
#define glColorMaterial(f,m)        bridge_ColorMaterial(f,m)

// Fog
#define glFogi                      bridge_Fogi
#define glFogf                      bridge_Fogf
#define glFogfv                     bridge_Fogfv

// Alpha test
#define glAlphaFunc                 bridge_AlphaFunc

// Enable/Disable (intercept fixed-function tokens)
#define glEnable                    bridge_Enable
#define glDisable                   bridge_Disable
#define glIsEnabled                 bridge_IsEnabled

// Texture enable/disable handled by bridge (GL_TEXTURE_2D)
// Other GL enables/disables are passed through to native GLES

// Stubs for unsupported features
#define glPolygonMode(face,mode)    bridge_PolygonMode(face,mode)
#define glHint(target,hint)         bridge_Hint(target,hint)

// glColor4f also needs to update bridge state (but not redirect glGetFloatv)
// The bridge tracks current color for lighting / material

// Shader program access for the game's matrix uploads
#define OGL_BridgeBeforeDrawScene   bridge_BeforeDrawScene
#define OGL_BridgeSetProjection     bridge_SetProjectionMatrix
#define OGL_BridgeSetModelView      bridge_SetModelViewMatrix

#endif // __ANDROID__
