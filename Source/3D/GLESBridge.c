// GLESBridge.c
// Fixed-function OpenGL 1.x/2.x emulation using OpenGL ES 3.0 shaders.
// (c) 2025 Cro-Mag Rally Android Port

#ifdef __ANDROID__

#include <GLES3/gl3.h>
#include <android/log.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "gles_bridge.h"

#define LOG_TAG "GLESBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Matrix math helpers
// ---------------------------------------------------------------------------

typedef struct { float m[16]; } Mat4;

static const Mat4 kIdentity = {{
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1
}};

static void mat4_multiply(Mat4 *dst, const Mat4 *a, const Mat4 *b)
{
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++) {
            float sum = 0;
            for (int k = 0; k < 4; k++)
                sum += a->m[k*4+row] * b->m[col*4+k];
            dst->m[col*4+row] = sum;
        }
}

static void mat4_translate(Mat4 *m, float x, float y, float z)
{
    Mat4 t = kIdentity;
    t.m[12] = x; t.m[13] = y; t.m[14] = z;
    Mat4 tmp; mat4_multiply(&tmp, m, &t); *m = tmp;
}

static void mat4_scale(Mat4 *m, float x, float y, float z)
{
    Mat4 s = kIdentity;
    s.m[0] = x; s.m[5] = y; s.m[10] = z;
    Mat4 tmp; mat4_multiply(&tmp, m, &s); *m = tmp;
}

static void mat4_rotate(Mat4 *m, float angleDeg, float ax, float ay, float az)
{
    float rad = angleDeg * (3.14159265358979f / 180.0f);
    float c = cosf(rad), s = sinf(rad);
    float len = sqrtf(ax*ax + ay*ay + az*az);
    if (len < 1e-6f) return;
    ax /= len; ay /= len; az /= len;

    Mat4 r;
    r.m[0]  = c + ax*ax*(1-c);      r.m[4]  = ax*ay*(1-c) - az*s; r.m[8]  = ax*az*(1-c) + ay*s; r.m[12] = 0;
    r.m[1]  = ay*ax*(1-c) + az*s;   r.m[5]  = c + ay*ay*(1-c);     r.m[9]  = ay*az*(1-c) - ax*s; r.m[13] = 0;
    r.m[2]  = az*ax*(1-c) - ay*s;   r.m[6]  = az*ay*(1-c) + ax*s;  r.m[10] = c + az*az*(1-c);     r.m[14] = 0;
    r.m[3]  = 0;                     r.m[7]  = 0;                    r.m[11] = 0;                    r.m[15] = 1;

    Mat4 tmp; mat4_multiply(&tmp, m, &r); *m = tmp;
}

static void mat4_ortho(Mat4 *m, float l, float r, float b, float t, float n, float f)
{
    *m = kIdentity;
    m->m[0]  =  2.0f/(r-l);
    m->m[5]  =  2.0f/(t-b);
    m->m[10] = -2.0f/(f-n);
    m->m[12] = -(r+l)/(r-l);
    m->m[13] = -(t+b)/(t-b);
    m->m[14] = -(f+n)/(f-n);
}

static void mat4_frustum(Mat4 *m, float l, float r, float b, float t, float n, float f)
{
    *m = kIdentity;
    m->m[0]  =  2.0f*n/(r-l);
    m->m[5]  =  2.0f*n/(t-b);
    m->m[8]  =  (r+l)/(r-l);
    m->m[9]  =  (t+b)/(t-b);
    m->m[10] = -(f+n)/(f-n);
    m->m[11] = -1.0f;
    m->m[14] = -2.0f*f*n/(f-n);
    m->m[15] =  0.0f;
}

// ---------------------------------------------------------------------------
// Shader source
// ---------------------------------------------------------------------------

static const char *kVertexShaderSrc =
"#version 300 es\n"
"precision highp float;\n"
"\n"
"uniform mat4 u_mvMatrix;\n"
"uniform mat4 u_projMatrix;\n"
"uniform mat4 u_mvpMatrix;\n"
"uniform mat3 u_normalMatrix;\n"
"uniform bool u_lightingEnabled;\n"
"uniform bool u_colorMaterialEnabled;\n"  // use vertex color as material
"\n"
"// Lights (up to 4)\n"
"uniform bool  u_lightEnabled[4];\n"
"uniform vec4  u_lightPosition[4];   // eye-space position (w=0 → directional)\n"
"uniform vec4  u_lightAmbient[4];\n"
"uniform vec4  u_lightDiffuse[4];\n"
"uniform vec4  u_sceneAmbient;\n"
"uniform vec4  u_materialAmbient;\n"
"uniform vec4  u_materialDiffuse;\n"
"uniform vec4  u_materialEmission;\n"
"\n"
"in vec3 a_position;\n"
"in vec3 a_normal;\n"
"in vec2 a_texcoord;\n"
"in vec4 a_color;\n"
"\n"
"out vec4 v_color;\n"
"out vec2 v_texcoord;\n"
"\n"
"void main() {\n"
"    gl_Position = u_mvpMatrix * vec4(a_position, 1.0);\n"
"    v_texcoord = a_texcoord;\n"
"\n"
"    if (u_lightingEnabled) {\n"
"        vec4 matDiffuse = u_colorMaterialEnabled ? a_color : u_materialDiffuse;\n"
"        vec4 matAmbient = u_colorMaterialEnabled ? a_color : u_materialAmbient;\n"
"        vec3 eyeNormal = normalize(u_normalMatrix * a_normal);\n"
"        vec4 color = u_materialEmission + matAmbient * u_sceneAmbient;\n"
"        for (int i = 0; i < 4; i++) {\n"
"            if (!u_lightEnabled[i]) continue;\n"
"            vec3 lightDir;\n"
"            if (u_lightPosition[i].w == 0.0) {\n"
"                lightDir = normalize(u_lightPosition[i].xyz);\n"
"            } else {\n"
"                vec3 eyePos = (u_mvMatrix * vec4(a_position, 1.0)).xyz;\n"
"                lightDir = normalize(u_lightPosition[i].xyz - eyePos);\n"
"            }\n"
"            float diff = max(dot(eyeNormal, lightDir), 0.0);\n"
"            color += matAmbient * u_lightAmbient[i];\n"
"            color += diff * matDiffuse * u_lightDiffuse[i];\n"
"        }\n"
"        v_color = vec4(clamp(color.rgb, 0.0, 1.0), matDiffuse.a);\n"
"    } else {\n"
"        v_color = a_color;\n"
"    }\n"
"}\n";

static const char *kFragmentShaderSrc =
"#version 300 es\n"
"precision mediump float;\n"
"\n"
"uniform sampler2D u_texture0;\n"
"uniform bool u_textureEnabled;\n"
"\n"
"// Alpha test\n"
"uniform bool  u_alphaTestEnabled;\n"
"uniform int   u_alphaFunc;   // GL_NEVER=0x200 etc.\n"
"uniform float u_alphaRef;\n"
"\n"
"// Fog\n"
"uniform bool  u_fogEnabled;\n"
"uniform int   u_fogMode;     // GL_LINEAR=0x2601, GL_EXP=0x800, GL_EXP2=0x801\n"
"uniform float u_fogStart;\n"
"uniform float u_fogEnd;\n"
"uniform float u_fogDensity;\n"
"uniform vec4  u_fogColor;\n"
"\n"
"in vec4 v_color;\n"
"in vec2 v_texcoord;\n"
"\n"
"out vec4 fragColor;\n"
"\n"
"void main() {\n"
"    vec4 color = v_color;\n"
"\n"
"    if (u_textureEnabled) {\n"
"        vec4 texColor = texture(u_texture0, v_texcoord);\n"
"        color *= texColor;\n"
"    }\n"
"\n"
"    // Alpha test\n"
"    if (u_alphaTestEnabled) {\n"
"        float a = color.a;\n"
"        bool pass;\n"
"        if      (u_alphaFunc == 0x0200) pass = false;          // GL_NEVER\n"
"        else if (u_alphaFunc == 0x0201) pass = (a <  u_alphaRef); // GL_LESS\n"
"        else if (u_alphaFunc == 0x0202) pass = (a == u_alphaRef); // GL_EQUAL\n"
"        else if (u_alphaFunc == 0x0203) pass = (a <= u_alphaRef); // GL_LEQUAL\n"
"        else if (u_alphaFunc == 0x0204) pass = (a >  u_alphaRef); // GL_GREATER\n"
"        else if (u_alphaFunc == 0x0205) pass = (a != u_alphaRef); // GL_NOTEQUAL\n"
"        else if (u_alphaFunc == 0x0206) pass = (a >= u_alphaRef); // GL_GEQUAL\n"
"        else                            pass = true;            // GL_ALWAYS\n"
"        if (!pass) discard;\n"
"    }\n"
"\n"
"    // Fog\n"
"    if (u_fogEnabled) {\n"
"        float depth = gl_FragCoord.z / gl_FragCoord.w;\n"
"        float fogFactor;\n"
"        if (u_fogMode == 0x2601) { // GL_LINEAR\n"
"            fogFactor = clamp((u_fogEnd - depth) / (u_fogEnd - u_fogStart), 0.0, 1.0);\n"
"        } else if (u_fogMode == 0x0800) { // GL_EXP\n"
"            fogFactor = clamp(exp(-u_fogDensity * depth), 0.0, 1.0);\n"
"        } else { // GL_EXP2\n"
"            float d = u_fogDensity * depth;\n"
"            fogFactor = clamp(exp(-d * d), 0.0, 1.0);\n"
"        }\n"
"        color.rgb = mix(u_fogColor.rgb, color.rgb, fogFactor);\n"
"    }\n"
"\n"
"    fragColor = color;\n"
"}\n";

// ---------------------------------------------------------------------------
// Shader program
// ---------------------------------------------------------------------------

static GLuint gProgram = 0;

// Attribute locations
static GLint gAttrPosition = -1;
static GLint gAttrNormal   = -1;
static GLint gAttrTexCoord = -1;
static GLint gAttrColor    = -1;

// Uniform locations
static GLint gUniMVMatrix        = -1;
static GLint gUniProjMatrix      = -1;
static GLint gUniMVPMatrix       = -1;
static GLint gUniNormalMatrix    = -1;
static GLint gUniLightingEnabled = -1;
static GLint gUniColorMaterial   = -1;
static GLint gUniLightEnabled[4];
static GLint gUniLightPosition[4];
static GLint gUniLightAmbient[4];
static GLint gUniLightDiffuse[4];
static GLint gUniSceneAmbient    = -1;
static GLint gUniMatAmbient      = -1;
static GLint gUniMatDiffuse      = -1;
static GLint gUniMatEmission     = -1;
static GLint gUniTexture0        = -1;
static GLint gUniTextureEnabled  = -1;
static GLint gUniAlphaTestEnabled= -1;
static GLint gUniAlphaFunc       = -1;
static GLint gUniAlphaRef        = -1;
static GLint gUniFogEnabled      = -1;
static GLint gUniFogMode         = -1;
static GLint gUniFogStart        = -1;
static GLint gUniFogEnd          = -1;
static GLint gUniFogDensity      = -1;
static GLint gUniFogColor        = -1;

static GLuint CompileShader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint ok; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(shader, sizeof(buf), NULL, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint CreateProgram(void)
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER,   kVertexShaderSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetProgramInfoLog(prog, sizeof(buf), NULL, buf);
        LOGE("Program link error: %s", buf);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ---------------------------------------------------------------------------
// Bridge state
// ---------------------------------------------------------------------------

#define MATRIX_STACK_DEPTH 32

typedef struct {
    Mat4 stack[MATRIX_STACK_DEPTH];
    int top;
} MatrixStack;

static GLenum    gCurrentMatrixMode = GL_MODELVIEW;
static MatrixStack gModelviewStack;
static MatrixStack gProjectionStack;
static MatrixStack gTextureStack;
static bool gMVPDirty = true;

// GL state
static bool gLightingEnabled    = false;
static bool gColorMaterialEnabled = true;
static bool gTextureEnabled     = false;
static bool gFogEnabled         = false;
static bool gAlphaTestEnabled   = false;
static bool gNormalizeEnabled   = false;
static bool gLightEnabled[4]    = {false,false,false,false};

// Lighting uniforms state
static float gSceneAmbient[4]   = {0.2f, 0.2f, 0.2f, 1.0f};
static float gMatAmbient[4]     = {0.2f, 0.2f, 0.2f, 1.0f};
static float gMatDiffuse[4]     = {0.8f, 0.8f, 0.8f, 1.0f};
static float gMatEmission[4]    = {0.0f, 0.0f, 0.0f, 0.0f};
static float gLightPosition[4][4];
static float gLightAmbient[4][4];
static float gLightDiffuse[4][4];

// Fog state
static int   gFogMode    = 0x2601; // GL_LINEAR
static float gFogStart   = 0.0f;
static float gFogEnd     = 1.0f;
static float gFogDensity = 1.0f;
static float gFogColor[4]= {0,0,0,0};

// Alpha test state
static int   gAlphaFunc = 0x0207; // GL_ALWAYS
static float gAlphaRef  = 0.0f;

// Current color (for immediate mode + non-lighting)
static float gCurColor[4] = {1,1,1,1};
// Current normal (for immediate mode)
static float gCurNormal[3] = {0,0,1};
// Current texcoord (for immediate mode)
static float gCurTexCoord[2] = {0,0};

// VAO/VBO for streaming draws
static GLuint gStreamVAO      = 0;
static GLuint gStreamVBO      = 0;

// Separate VAO for vertex-array draws (client state)
static GLuint gArrayVAO       = 0;
static GLuint gArrayVBO       = 0;
static GLuint gArrayIBO       = 0;

// Immediate mode buffer
#define IMMED_MAX_VERTS 65536

typedef struct {
    float pos[3];
    float normal[3];
    float uv[2];
    float color[4];
} BridgeVertex;

static BridgeVertex gImmVerts[IMMED_MAX_VERTS];
static int          gImmVertCount = 0;
static GLenum       gImmPrimitive = GL_TRIANGLES;

// Client-array state
static bool   gCSVertexEnabled   = false;
static bool   gCSNormalEnabled   = false;
static bool   gCSTexCoordEnabled = false;
static bool   gCSColorEnabled    = false;

static GLint   gCSVertexSize  = 3;
static GLenum  gCSVertexType  = GL_FLOAT;
static GLsizei gCSVertexStride= 0;
static const void *gCSVertexPtr = NULL;

static GLenum  gCSNormalType  = GL_FLOAT;
static GLsizei gCSNormalStride= 0;
static const void *gCSNormalPtr = NULL;

static GLint   gCSTexCoordSize = 2;
static GLenum  gCSTexCoordType = GL_FLOAT;
static GLsizei gCSTexCoordStride = 0;
static const void *gCSTexCoordPtr = NULL;

static GLint   gCSColorSize  = 4;
static GLenum  gCSColorType  = GL_FLOAT;
static GLsizei gCSColorStride= 0;
static const void *gCSColorPtr = NULL;

// Uniform dirty flags
static bool gUniformsDirty = true;

// ---------------------------------------------------------------------------
// Helpers: get current matrix stack
// ---------------------------------------------------------------------------

static MatrixStack *GetCurrentStack(void)
{
    if (gCurrentMatrixMode == GL_PROJECTION) return &gProjectionStack;
    if (gCurrentMatrixMode == GL_TEXTURE)    return &gTextureStack;
    return &gModelviewStack;
}

static Mat4 *CurrentMatrix(void)
{
    MatrixStack *s = GetCurrentStack();
    return &s->stack[s->top];
}

static void MarkMVPDirty(void)
{
    gMVPDirty = true;
    gUniformsDirty = true;
}

// ---------------------------------------------------------------------------
// Normal matrix (upper-left 3x3 of inverse-transpose of modelview)
// ---------------------------------------------------------------------------

static void ComputeNormalMatrix(const Mat4 *mv, float nm[9])
{
    // For uniform scaling, just take upper-left 3x3 (simpler, usually correct)
    nm[0] = mv->m[0]; nm[1] = mv->m[1]; nm[2] = mv->m[2];
    nm[3] = mv->m[4]; nm[4] = mv->m[5]; nm[5] = mv->m[6];
    nm[6] = mv->m[8]; nm[7] = mv->m[9]; nm[8] = mv->m[10];
}

// ---------------------------------------------------------------------------
// Upload uniforms to shader
// ---------------------------------------------------------------------------

static void FlushUniforms(void)
{
    if (!gUniformsDirty) return;
    gUniformsDirty = false;

    const Mat4 *mv   = &gModelviewStack.stack[gModelviewStack.top];
    const Mat4 *proj = &gProjectionStack.stack[gProjectionStack.top];

    // MVP
    if (gMVPDirty) {
        Mat4 mvp;
        mat4_multiply(&mvp, proj, mv);
        glUniformMatrix4fv(gUniMVPMatrix,   1, GL_FALSE, mvp.m);
        glUniformMatrix4fv(gUniMVMatrix,    1, GL_FALSE, mv->m);
        glUniformMatrix4fv(gUniProjMatrix,  1, GL_FALSE, proj->m);

        float nm[9];
        ComputeNormalMatrix(mv, nm);
        glUniformMatrix3fv(gUniNormalMatrix, 1, GL_FALSE, nm);
        gMVPDirty = false;
    }

    glUniform1i(gUniLightingEnabled, gLightingEnabled);
    glUniform1i(gUniColorMaterial,   gColorMaterialEnabled);
    glUniform1i(gUniTextureEnabled,  gTextureEnabled);
    glUniform1i(gUniAlphaTestEnabled,gAlphaTestEnabled);
    glUniform1i(gUniAlphaFunc,       gAlphaFunc);
    glUniform1f(gUniAlphaRef,        gAlphaRef);
    glUniform1i(gUniFogEnabled,      gFogEnabled);
    glUniform1i(gUniFogMode,         gFogMode);
    glUniform1f(gUniFogStart,        gFogStart);
    glUniform1f(gUniFogEnd,          gFogEnd);
    glUniform1f(gUniFogDensity,      gFogDensity);
    glUniform4fv(gUniFogColor,       1, gFogColor);
    glUniform4fv(gUniSceneAmbient,   1, gSceneAmbient);
    glUniform4fv(gUniMatAmbient,     1, gMatAmbient);
    glUniform4fv(gUniMatDiffuse,     1, gMatDiffuse);
    glUniform4fv(gUniMatEmission,    1, gMatEmission);

    for (int i = 0; i < 4; i++) {
        glUniform1i(gUniLightEnabled[i],  gLightEnabled[i]);
        glUniform4fv(gUniLightPosition[i],1, gLightPosition[i]);
        glUniform4fv(gUniLightAmbient[i], 1, gLightAmbient[i]);
        glUniform4fv(gUniLightDiffuse[i], 1, gLightDiffuse[i]);
    }

    glUniform1i(gUniTexture0, 0);
}

// ---------------------------------------------------------------------------
// bridge_Init
// ---------------------------------------------------------------------------

void bridge_Init(void)
{
    gProgram = CreateProgram();
    if (!gProgram) {
        LOGE("bridge_Init: failed to create shader program");
        return;
    }

    glUseProgram(gProgram);

    // Get attribute locations
    gAttrPosition = glGetAttribLocation(gProgram, "a_position");
    gAttrNormal   = glGetAttribLocation(gProgram, "a_normal");
    gAttrTexCoord = glGetAttribLocation(gProgram, "a_texcoord");
    gAttrColor    = glGetAttribLocation(gProgram, "a_color");

    // Get uniform locations
    gUniMVMatrix         = glGetUniformLocation(gProgram, "u_mvMatrix");
    gUniProjMatrix       = glGetUniformLocation(gProgram, "u_projMatrix");
    gUniMVPMatrix        = glGetUniformLocation(gProgram, "u_mvpMatrix");
    gUniNormalMatrix     = glGetUniformLocation(gProgram, "u_normalMatrix");
    gUniLightingEnabled  = glGetUniformLocation(gProgram, "u_lightingEnabled");
    gUniColorMaterial    = glGetUniformLocation(gProgram, "u_colorMaterialEnabled");
    gUniSceneAmbient     = glGetUniformLocation(gProgram, "u_sceneAmbient");
    gUniMatAmbient       = glGetUniformLocation(gProgram, "u_materialAmbient");
    gUniMatDiffuse       = glGetUniformLocation(gProgram, "u_materialDiffuse");
    gUniMatEmission      = glGetUniformLocation(gProgram, "u_materialEmission");
    gUniTexture0         = glGetUniformLocation(gProgram, "u_texture0");
    gUniTextureEnabled   = glGetUniformLocation(gProgram, "u_textureEnabled");
    gUniAlphaTestEnabled = glGetUniformLocation(gProgram, "u_alphaTestEnabled");
    gUniAlphaFunc        = glGetUniformLocation(gProgram, "u_alphaFunc");
    gUniAlphaRef         = glGetUniformLocation(gProgram, "u_alphaRef");
    gUniFogEnabled       = glGetUniformLocation(gProgram, "u_fogEnabled");
    gUniFogMode          = glGetUniformLocation(gProgram, "u_fogMode");
    gUniFogStart         = glGetUniformLocation(gProgram, "u_fogStart");
    gUniFogEnd           = glGetUniformLocation(gProgram, "u_fogEnd");
    gUniFogDensity       = glGetUniformLocation(gProgram, "u_fogDensity");
    gUniFogColor         = glGetUniformLocation(gProgram, "u_fogColor");

    char buf[64];
    for (int i = 0; i < 4; i++) {
        snprintf(buf, sizeof(buf), "u_lightEnabled[%d]",  i);
        gUniLightEnabled[i]  = glGetUniformLocation(gProgram, buf);
        snprintf(buf, sizeof(buf), "u_lightPosition[%d]", i);
        gUniLightPosition[i] = glGetUniformLocation(gProgram, buf);
        snprintf(buf, sizeof(buf), "u_lightAmbient[%d]",  i);
        gUniLightAmbient[i]  = glGetUniformLocation(gProgram, buf);
        snprintf(buf, sizeof(buf), "u_lightDiffuse[%d]",  i);
        gUniLightDiffuse[i]  = glGetUniformLocation(gProgram, buf);
    }

    // Initialize matrix stacks
    gModelviewStack.top  = 0;
    gProjectionStack.top = 0;
    gTextureStack.top    = 0;
    gModelviewStack.stack[0]  = kIdentity;
    gProjectionStack.stack[0] = kIdentity;
    gTextureStack.stack[0]    = kIdentity;

    // Initialize light state
    for (int i = 0; i < 4; i++) {
        gLightEnabled[i] = false;
        gLightPosition[i][0] = 0; gLightPosition[i][1] = 0; gLightPosition[i][2] = 1; gLightPosition[i][3] = 0;
        gLightAmbient[i][0] = 0; gLightAmbient[i][1] = 0; gLightAmbient[i][2] = 0; gLightAmbient[i][3] = 1;
        gLightDiffuse[i][0] = (i==0)?1:0; gLightDiffuse[i][1] = (i==0)?1:0; gLightDiffuse[i][2] = (i==0)?1:0; gLightDiffuse[i][3] = 1;
    }

    // Create streaming VAO/VBO for immediate mode
    glGenVertexArrays(1, &gStreamVAO);
    glGenBuffers(1, &gStreamVBO);

    // Create vertex array VAO/VBO for client-state draws
    glGenVertexArrays(1, &gArrayVAO);
    glGenBuffers(1, &gArrayVBO);
    glGenBuffers(1, &gArrayIBO);

    gUniformsDirty = true;
    gMVPDirty = true;

    LOGI("bridge_Init: shader program %u compiled OK", gProgram);
}

void bridge_Shutdown(void)
{
    if (gProgram) { glDeleteProgram(gProgram); gProgram = 0; }
    if (gStreamVAO) { glDeleteVertexArrays(1, &gStreamVAO); gStreamVAO = 0; }
    if (gStreamVBO) { glDeleteBuffers(1, &gStreamVBO); gStreamVBO = 0; }
    if (gArrayVAO) { glDeleteVertexArrays(1, &gArrayVAO); gArrayVAO = 0; }
    if (gArrayVBO) { glDeleteBuffers(1, &gArrayVBO); gArrayVBO = 0; }
    if (gArrayIBO) { glDeleteBuffers(1, &gArrayIBO); gArrayIBO = 0; }
}

void bridge_BeforeDrawScene(void)
{
    glUseProgram(gProgram);
    gUniformsDirty = true;
    gMVPDirty = true;
}

// ---------------------------------------------------------------------------
// Matrix ops
// ---------------------------------------------------------------------------

void bridge_MatrixMode(GLenum mode)
{
    gCurrentMatrixMode = mode;
}

void bridge_PushMatrix(void)
{
    MatrixStack *s = GetCurrentStack();
    if (s->top + 1 >= MATRIX_STACK_DEPTH) { LOGW("Matrix stack overflow"); return; }
    s->stack[s->top + 1] = s->stack[s->top];
    s->top++;
}

void bridge_PopMatrix(void)
{
    MatrixStack *s = GetCurrentStack();
    if (s->top == 0) { LOGW("Matrix stack underflow"); return; }
    s->top--;
    MarkMVPDirty();
}

void bridge_LoadIdentity(void)
{
    *CurrentMatrix() = kIdentity;
    MarkMVPDirty();
}

void bridge_LoadMatrixf(const GLfloat *m)
{
    memcpy(CurrentMatrix()->m, m, 16*sizeof(float));
    MarkMVPDirty();
}

void bridge_MultMatrixf(const GLfloat *m)
{
    Mat4 b; memcpy(b.m, m, 16*sizeof(float));
    Mat4 tmp; mat4_multiply(&tmp, CurrentMatrix(), &b);
    *CurrentMatrix() = tmp;
    MarkMVPDirty();
}

void bridge_Translatef(GLfloat x, GLfloat y, GLfloat z)
{
    mat4_translate(CurrentMatrix(), x, y, z);
    MarkMVPDirty();
}

void bridge_Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    mat4_rotate(CurrentMatrix(), angle, x, y, z);
    MarkMVPDirty();
}

void bridge_Scalef(GLfloat x, GLfloat y, GLfloat z)
{
    mat4_scale(CurrentMatrix(), x, y, z);
    MarkMVPDirty();
}

void bridge_Ortho(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    mat4_ortho(CurrentMatrix(), l, r, b, t, n, f);
    MarkMVPDirty();
}

void bridge_Frustum(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    mat4_frustum(CurrentMatrix(), l, r, b, t, n, f);
    MarkMVPDirty();
}

void bridge_SetProjectionMatrix(const GLfloat *m)
{
    memcpy(gProjectionStack.stack[gProjectionStack.top].m, m, 16*sizeof(float));
    MarkMVPDirty();
}

void bridge_SetModelViewMatrix(const GLfloat *m)
{
    memcpy(gModelviewStack.stack[gModelviewStack.top].m, m, 16*sizeof(float));
    MarkMVPDirty();
}

// ---------------------------------------------------------------------------
// Immediate mode
// ---------------------------------------------------------------------------

void bridge_Begin(GLenum mode)
{
    gImmPrimitive  = mode;
    gImmVertCount  = 0;
}

static void FlushImmediate(GLenum prim, BridgeVertex *verts, int n)
{
    if (n == 0) return;
    FlushUniforms();

    glBindVertexArray(gStreamVAO);

    // Check if we need to convert quads to triangles
    int drawCount = n;
    BridgeVertex *drawVerts = verts;
    BridgeVertex *tmpVerts = NULL;

    if (prim == 0x0007 /*GL_QUADS*/) {
        int numQuads = n / 4;
        drawCount = numQuads * 6;
        tmpVerts = (BridgeVertex*)malloc(drawCount * sizeof(BridgeVertex));
        for (int q = 0; q < numQuads; q++) {
            tmpVerts[q*6+0] = verts[q*4+0];
            tmpVerts[q*6+1] = verts[q*4+1];
            tmpVerts[q*6+2] = verts[q*4+2];
            tmpVerts[q*6+3] = verts[q*4+0];
            tmpVerts[q*6+4] = verts[q*4+2];
            tmpVerts[q*6+5] = verts[q*4+3];
        }
        drawVerts = tmpVerts;
        prim = GL_TRIANGLES;
    }

    // Upload to VBO
    glBindBuffer(GL_ARRAY_BUFFER, gStreamVBO);
    glBufferData(GL_ARRAY_BUFFER, drawCount * sizeof(BridgeVertex), drawVerts, GL_STREAM_DRAW);

    // Set attribs
    int stride = sizeof(BridgeVertex);
    glEnableVertexAttribArray(gAttrPosition);
    glVertexAttribPointer(gAttrPosition, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, pos));
    glEnableVertexAttribArray(gAttrNormal);
    glVertexAttribPointer(gAttrNormal, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, normal));
    glEnableVertexAttribArray(gAttrTexCoord);
    glVertexAttribPointer(gAttrTexCoord, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, uv));
    glEnableVertexAttribArray(gAttrColor);
    glVertexAttribPointer(gAttrColor, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, color));

    glDrawArrays(prim, 0, drawCount);

    // Cleanup
    glDisableVertexAttribArray(gAttrPosition);
    glDisableVertexAttribArray(gAttrNormal);
    glDisableVertexAttribArray(gAttrTexCoord);
    glDisableVertexAttribArray(gAttrColor);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (tmpVerts) free(tmpVerts);
}

void bridge_End(void)
{
    FlushImmediate(gImmPrimitive, gImmVerts, gImmVertCount);
    gImmVertCount = 0;
}

static BridgeVertex *AllocImmVert(void)
{
    if (gImmVertCount >= IMMED_MAX_VERTS) {
        LOGW("Immediate mode buffer overflow");
        return &gImmVerts[IMMED_MAX_VERTS-1];
    }
    BridgeVertex *v = &gImmVerts[gImmVertCount++];
    v->normal[0] = gCurNormal[0];
    v->normal[1] = gCurNormal[1];
    v->normal[2] = gCurNormal[2];
    v->uv[0] = gCurTexCoord[0];
    v->uv[1] = gCurTexCoord[1];
    v->color[0] = gCurColor[0];
    v->color[1] = gCurColor[1];
    v->color[2] = gCurColor[2];
    v->color[3] = gCurColor[3];
    return v;
}

void bridge_Vertex2f(GLfloat x, GLfloat y)
{
    BridgeVertex *v = AllocImmVert();
    v->pos[0] = x; v->pos[1] = y; v->pos[2] = 0;
}

void bridge_Vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    BridgeVertex *v = AllocImmVert();
    v->pos[0] = x; v->pos[1] = y; v->pos[2] = z;
}

void bridge_Vertex3fv(const GLfloat *v_) {
    bridge_Vertex3f(v_[0], v_[1], v_[2]);
}

void bridge_Normal3f(GLfloat x, GLfloat y, GLfloat z)
{
    gCurNormal[0] = x; gCurNormal[1] = y; gCurNormal[2] = z;
}

void bridge_Normal3fv(const GLfloat *v) {
    gCurNormal[0] = v[0]; gCurNormal[1] = v[1]; gCurNormal[2] = v[2];
}

void bridge_TexCoord2f(GLfloat s, GLfloat t)
{
    gCurTexCoord[0] = s; gCurTexCoord[1] = t;
}

void bridge_Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gCurColor[0] = r; gCurColor[1] = g; gCurColor[2] = b; gCurColor[3] = a;
    // Also update material diffuse when GL_COLOR_MATERIAL is active
    if (gColorMaterialEnabled) {
        gMatDiffuse[0] = r; gMatDiffuse[1] = g; gMatDiffuse[2] = b; gMatDiffuse[3] = a;
        gMatAmbient[0] = r; gMatAmbient[1] = g; gMatAmbient[2] = b; gMatAmbient[3] = a;
    }
    gUniformsDirty = true;
}

void bridge_Color4fv(const GLfloat *v) {
    bridge_Color4f(v[0], v[1], v[2], v[3]);
}

void bridge_Color4ubv(const GLubyte *v) {
    bridge_Color4f(v[0]/255.0f, v[1]/255.0f, v[2]/255.0f, v[3]/255.0f);
}

// ---------------------------------------------------------------------------
// Client-state vertex arrays
// ---------------------------------------------------------------------------

void bridge_EnableClientState(GLenum cap)
{
    switch (cap) {
        case 0x8074: gCSVertexEnabled   = true; break; // GL_VERTEX_ARRAY
        case 0x8075: gCSNormalEnabled   = true; break; // GL_NORMAL_ARRAY
        case 0x8076: gCSColorEnabled    = true; break; // GL_COLOR_ARRAY
        case 0x8078: gCSTexCoordEnabled = true; break; // GL_TEXTURE_COORD_ARRAY
    }
}

void bridge_DisableClientState(GLenum cap)
{
    switch (cap) {
        case 0x8074: gCSVertexEnabled   = false; break;
        case 0x8075: gCSNormalEnabled   = false; break;
        case 0x8076: gCSColorEnabled    = false; break;
        case 0x8078: gCSTexCoordEnabled = false; break;
    }
}

void bridge_VertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    gCSVertexSize   = size;
    gCSVertexType   = type;
    gCSVertexStride = stride;
    gCSVertexPtr    = ptr;
}

void bridge_NormalPointer(GLenum type, GLsizei stride, const void *ptr)
{
    gCSNormalType   = type;
    gCSNormalStride = stride;
    gCSNormalPtr    = ptr;
}

void bridge_TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    gCSTexCoordSize   = size;
    gCSTexCoordType   = type;
    gCSTexCoordStride = stride;
    gCSTexCoordPtr    = ptr;
}

void bridge_ColorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
    gCSColorSize   = size;
    gCSColorType   = type;
    gCSColorStride = stride;
    gCSColorPtr    = ptr;
}

// Helper: get float from type/stride/ptr/index
static float GetFloat(const void *base, GLint size, GLenum type, GLsizei stride, int idx, int component)
{
    int elementSize = (type == GL_FLOAT) ? 4 : (type == GL_UNSIGNED_BYTE) ? 1 : 4;
    int actualStride = (stride == 0) ? (size * elementSize) : stride;
    const uint8_t *ptr = (const uint8_t*)base + (size_t)idx * actualStride;

    switch (type) {
        case GL_FLOAT:
            return ((const float*)ptr)[component];
        case GL_UNSIGNED_BYTE:
            return ((const uint8_t*)ptr)[component] / 255.0f;
        default:
            return 0.0f;
    }
}

// Helper: build interleaved BridgeVertex array from client arrays
static BridgeVertex *BuildVertexArray(int count)
{
    BridgeVertex *out = (BridgeVertex*)malloc(count * sizeof(BridgeVertex));
    for (int i = 0; i < count; i++) {
        BridgeVertex *v = &out[i];

        // Position
        if (gCSVertexEnabled && gCSVertexPtr) {
            v->pos[0] = GetFloat(gCSVertexPtr, gCSVertexSize, gCSVertexType, gCSVertexStride, i, 0);
            v->pos[1] = GetFloat(gCSVertexPtr, gCSVertexSize, gCSVertexType, gCSVertexStride, i, 1);
            v->pos[2] = (gCSVertexSize >= 3) ? GetFloat(gCSVertexPtr, gCSVertexSize, gCSVertexType, gCSVertexStride, i, 2) : 0.0f;
        } else {
            v->pos[0] = v->pos[1] = v->pos[2] = 0;
        }

        // Normal
        if (gCSNormalEnabled && gCSNormalPtr) {
            v->normal[0] = GetFloat(gCSNormalPtr, 3, gCSNormalType, gCSNormalStride, i, 0);
            v->normal[1] = GetFloat(gCSNormalPtr, 3, gCSNormalType, gCSNormalStride, i, 1);
            v->normal[2] = GetFloat(gCSNormalPtr, 3, gCSNormalType, gCSNormalStride, i, 2);
        } else {
            v->normal[0] = 0; v->normal[1] = 0; v->normal[2] = 1;
        }

        // Texcoord
        if (gCSTexCoordEnabled && gCSTexCoordPtr) {
            v->uv[0] = GetFloat(gCSTexCoordPtr, gCSTexCoordSize, gCSTexCoordType, gCSTexCoordStride, i, 0);
            v->uv[1] = GetFloat(gCSTexCoordPtr, gCSTexCoordSize, gCSTexCoordType, gCSTexCoordStride, i, 1);
        } else {
            v->uv[0] = v->uv[1] = 0;
        }

        // Color
        if (gCSColorEnabled && gCSColorPtr) {
            v->color[0] = GetFloat(gCSColorPtr, gCSColorSize, gCSColorType, gCSColorStride, i, 0);
            v->color[1] = GetFloat(gCSColorPtr, gCSColorSize, gCSColorType, gCSColorStride, i, 1);
            v->color[2] = GetFloat(gCSColorPtr, gCSColorSize, gCSColorType, gCSColorStride, i, 2);
            v->color[3] = GetFloat(gCSColorPtr, gCSColorSize, gCSColorType, gCSColorStride, i, 3);
        } else {
            v->color[0] = gCurColor[0];
            v->color[1] = gCurColor[1];
            v->color[2] = gCurColor[2];
            v->color[3] = gCurColor[3];
        }
    }
    return out;
}

void bridge_DrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    if (!gCSVertexEnabled || !gCSVertexPtr || count <= 0) return;

    // Determine index range to find max vertex index
    int maxIdx = 0;
    if (type == GL_UNSIGNED_INT) {
        const uint32_t *idx = (const uint32_t*)indices;
        for (int i = 0; i < count; i++) if ((int)idx[i] > maxIdx) maxIdx = (int)idx[i];
    } else if (type == GL_UNSIGNED_SHORT) {
        const uint16_t *idx = (const uint16_t*)indices;
        for (int i = 0; i < count; i++) if ((int)idx[i] > maxIdx) maxIdx = (int)idx[i];
    } else if (type == GL_UNSIGNED_BYTE) {
        const uint8_t *idx = (const uint8_t*)indices;
        for (int i = 0; i < count; i++) if ((int)idx[i] > maxIdx) maxIdx = (int)idx[i];
    }

    int numVerts = maxIdx + 1;
    BridgeVertex *verts = BuildVertexArray(numVerts);

    FlushUniforms();
    glBindVertexArray(gArrayVAO);

    // Upload vertices
    glBindBuffer(GL_ARRAY_BUFFER, gArrayVBO);
    glBufferData(GL_ARRAY_BUFFER, numVerts * sizeof(BridgeVertex), verts, GL_STREAM_DRAW);
    free(verts);

    int stride = sizeof(BridgeVertex);
    glEnableVertexAttribArray(gAttrPosition);
    glVertexAttribPointer(gAttrPosition, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, pos));
    glEnableVertexAttribArray(gAttrNormal);
    glVertexAttribPointer(gAttrNormal, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, normal));
    glEnableVertexAttribArray(gAttrTexCoord);
    glVertexAttribPointer(gAttrTexCoord, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, uv));
    glEnableVertexAttribArray(gAttrColor);
    glVertexAttribPointer(gAttrColor, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BridgeVertex, color));

    // Upload indices - convert to uint32 if needed (GLES3 supports GL_UNSIGNED_INT)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gArrayIBO);
    if (type == GL_UNSIGNED_INT) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * 4, indices, GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)0);
    } else if (type == GL_UNSIGNED_SHORT) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * 2, indices, GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (void*)0);
    } else if (type == GL_UNSIGNED_BYTE) {
        uint16_t *idx16 = (uint16_t*)malloc(count * 2);
        const uint8_t *idx8 = (const uint8_t*)indices;
        for (int i = 0; i < count; i++) idx16[i] = idx8[i];
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * 2, idx16, GL_STREAM_DRAW);
        free(idx16);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (void*)0);
    }

    glDisableVertexAttribArray(gAttrPosition);
    glDisableVertexAttribArray(gAttrNormal);
    glDisableVertexAttribArray(gAttrTexCoord);
    glDisableVertexAttribArray(gAttrColor);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void bridge_DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (count <= 0) return;

    BridgeVertex *verts = BuildVertexArray(first + count);
    FlushImmediate(mode, verts + first, count);
    free(verts);
}

// ---------------------------------------------------------------------------
// Lighting
// ---------------------------------------------------------------------------

void bridge_Lightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    int idx = (int)(light - 0x4000); // GL_LIGHT0
    if (idx < 0 || idx >= 4) return;

    switch (pname) {
        case 0x1200: // GL_AMBIENT
            memcpy(gLightAmbient[idx], params, 4*sizeof(float));
            break;
        case 0x1201: // GL_DIFFUSE
            memcpy(gLightDiffuse[idx], params, 4*sizeof(float));
            break;
        case 0x1203: // GL_POSITION
        {
            // Transform position by current modelview matrix
            const Mat4 *mv = &gModelviewStack.stack[gModelviewStack.top];
            float w = params[3];
            float x = params[0]*mv->m[0] + params[1]*mv->m[4] + params[2]*mv->m[8]  + w*mv->m[12];
            float y = params[0]*mv->m[1] + params[1]*mv->m[5] + params[2]*mv->m[9]  + w*mv->m[13];
            float z = params[0]*mv->m[2] + params[1]*mv->m[6] + params[2]*mv->m[10] + w*mv->m[14];
            gLightPosition[idx][0] = x;
            gLightPosition[idx][1] = y;
            gLightPosition[idx][2] = z;
            gLightPosition[idx][3] = w;
            break;
        }
    }
    gUniformsDirty = true;
}

void bridge_Lightf(GLenum light, GLenum pname, GLfloat param)
{
    (void)light; (void)pname; (void)param;
}

void bridge_LightModelfv(GLenum pname, const GLfloat *params)
{
    if (pname == 0x0B53) { // GL_LIGHT_MODEL_AMBIENT
        memcpy(gSceneAmbient, params, 4*sizeof(float));
        gUniformsDirty = true;
    }
}

void bridge_Materialfv(GLenum face, GLenum pname, const GLfloat *params)
{
    (void)face;
    switch (pname) {
        case 0x1200: memcpy(gMatAmbient,  params, 4*sizeof(float)); break; // GL_AMBIENT
        case 0x1201: memcpy(gMatDiffuse,  params, 4*sizeof(float)); break; // GL_DIFFUSE
        case 0x1600: memcpy(gMatEmission, params, 4*sizeof(float)); break; // GL_EMISSION
        case 0x1602: // GL_AMBIENT_AND_DIFFUSE
            memcpy(gMatAmbient, params, 4*sizeof(float));
            memcpy(gMatDiffuse, params, 4*sizeof(float));
            break;
    }
    gUniformsDirty = true;
}

void bridge_Materialf(GLenum face, GLenum pname, GLfloat param)
{
    (void)face; (void)pname; (void)param;
}

void bridge_ColorMaterial(GLenum face, GLenum mode)
{
    (void)face; (void)mode;
    // We handle GL_COLOR_MATERIAL in bridge_Enable/Disable
}

// ---------------------------------------------------------------------------
// Fog
// ---------------------------------------------------------------------------

void bridge_Fogi(GLenum pname, GLint param)
{
    if (pname == 0x0B65) { gFogMode = param; gUniformsDirty = true; } // GL_FOG_MODE
}

void bridge_Fogf(GLenum pname, GLfloat param)
{
    switch (pname) {
        case 0x0B62: gFogDensity = param; break; // GL_FOG_DENSITY
        case 0x0B63: gFogStart   = param; break; // GL_FOG_START
        case 0x0B64: gFogEnd     = param; break; // GL_FOG_END
    }
    gUniformsDirty = true;
}

void bridge_Fogfv(GLenum pname, const GLfloat *params)
{
    if (pname == 0x0B66) { // GL_FOG_COLOR
        memcpy(gFogColor, params, 4*sizeof(float));
        gUniformsDirty = true;
    }
}

// ---------------------------------------------------------------------------
// Alpha test
// ---------------------------------------------------------------------------

void bridge_AlphaFunc(GLenum func, GLfloat ref)
{
    gAlphaFunc = (int)func;
    gAlphaRef  = ref;
    gUniformsDirty = true;
}

// ---------------------------------------------------------------------------
// Enable/Disable
// ---------------------------------------------------------------------------

void bridge_Enable(GLenum cap)
{
    switch (cap) {
        case 0x0B50: // GL_LIGHTING
            gLightingEnabled = true;
            gUniformsDirty = true;
            return;
        case 0x4000: case 0x4001: case 0x4002: case 0x4003: // GL_LIGHT0..3
            gLightEnabled[cap - 0x4000] = true;
            gUniformsDirty = true;
            return;
        case 0x0B60: // GL_FOG
            gFogEnabled = true;
            gUniformsDirty = true;
            return;
        case 0x0BC0: // GL_ALPHA_TEST
            gAlphaTestEnabled = true;
            gUniformsDirty = true;
            return;
        case 0x0BA1: // GL_NORMALIZE
        case 0x803A: // GL_RESCALE_NORMAL
            gNormalizeEnabled = true;
            return;
        case 0x0B57: // GL_COLOR_MATERIAL
            gColorMaterialEnabled = true;
            gUniformsDirty = true;
            return;
        case 0x0DE1: // GL_TEXTURE_2D
            gTextureEnabled = true;
            gUniformsDirty = true;
            return;
        // Pass through valid GLES3 caps
        default:
            glEnable(cap);
            return;
    }
}

void bridge_Disable(GLenum cap)
{
    switch (cap) {
        case 0x0B50: // GL_LIGHTING
            gLightingEnabled = false;
            gUniformsDirty = true;
            return;
        case 0x4000: case 0x4001: case 0x4002: case 0x4003: // GL_LIGHT0..3
            gLightEnabled[cap - 0x4000] = false;
            gUniformsDirty = true;
            return;
        case 0x0B60: // GL_FOG
            gFogEnabled = false;
            gUniformsDirty = true;
            return;
        case 0x0BC0: // GL_ALPHA_TEST
            gAlphaTestEnabled = false;
            gUniformsDirty = true;
            return;
        case 0x0BA1: // GL_NORMALIZE
        case 0x803A: // GL_RESCALE_NORMAL
            gNormalizeEnabled = false;
            return;
        case 0x0B57: // GL_COLOR_MATERIAL
            gColorMaterialEnabled = false;
            gUniformsDirty = true;
            return;
        case 0x0DE1: // GL_TEXTURE_2D
            gTextureEnabled = false;
            gUniformsDirty = true;
            return;
        // Pass through valid GLES3 caps
        default:
            glDisable(cap);
            return;
    }
}

GLboolean bridge_IsEnabled(GLenum cap)
{
    switch (cap) {
        case 0x0B50: return gLightingEnabled ? GL_TRUE : GL_FALSE;
        case 0x0B60: return gFogEnabled      ? GL_TRUE : GL_FALSE;
        case 0x0BC0: return gAlphaTestEnabled? GL_TRUE : GL_FALSE;
        case 0x0BA1: return gNormalizeEnabled? GL_TRUE : GL_FALSE;
        case 0x0B57: return gColorMaterialEnabled ? GL_TRUE : GL_FALSE;
        case 0x0DE1: return gTextureEnabled  ? GL_TRUE : GL_FALSE;
        default:     return glIsEnabled(cap);
    }
}

// ---------------------------------------------------------------------------
// GL getter wrappers for GLES3 incompatibilities
// ---------------------------------------------------------------------------

#ifndef GL_CURRENT_COLOR
#define GL_CURRENT_COLOR  0x0B00
#endif
#ifndef GL_BLEND_SRC
#define GL_BLEND_SRC      0x0BE1
#endif
#ifndef GL_BLEND_DST
#define GL_BLEND_DST      0x0BE0
#endif

void bridge_GetFloatv(GLenum pname, GLfloat *data)
{
    if (pname == GL_CURRENT_COLOR) {
        data[0] = gCurColor[0];
        data[1] = gCurColor[1];
        data[2] = gCurColor[2];
        data[3] = gCurColor[3];
        return;
    }
    glGetFloatv(pname, data);
}

void bridge_GetIntegerv(GLenum pname, GLint *data)
{
    if (pname == GL_BLEND_SRC) {
        // GLES3: use GL_BLEND_SRC_RGB
        glGetIntegerv(0x80C9 /*GL_BLEND_SRC_RGB*/, data);
        return;
    }
    if (pname == GL_BLEND_DST) {
        // GLES3: use GL_BLEND_DST_RGB
        glGetIntegerv(0x80C8 /*GL_BLEND_DST_RGB*/, data);
        return;
    }
    glGetIntegerv(pname, data);
}

// ---------------------------------------------------------------------------
// Stubs
// ---------------------------------------------------------------------------

void bridge_PolygonMode(GLenum face, GLenum mode)
{
    (void)face; (void)mode;
    // GLES3 does not support glPolygonMode; ignore wireframe toggle
}

void bridge_Hint(GLenum target, GLenum hint)
{
    // Only pass through hints that are valid in GLES3
    // GL_FOG_HINT, GL_CLIP_VOLUME_CLIPPING_HINT_EXT are not valid in GLES3
    switch (target) {
        case 0x8192: // GL_GENERATE_MIPMAP_HINT
            glHint(target, hint);
            break;
        default:
            break; // Ignore unknown hints
    }
}

// ---------------------------------------------------------------------------
// Texture conversion
// ---------------------------------------------------------------------------

void *bridge_ConvertBGRA1555toRGBA8(const void *src, int width, int height)
{
    // Convert GL_BGRA + GL_UNSIGNED_SHORT_1_5_5_5_REV  →  GL_RGBA + GL_UNSIGNED_BYTE
    // Input: 16-bit pixels, layout (LSB to MSB): BBBBB GGGGG RRRRR A
    // The _REV means it's stored in little-endian order on the machine.
    uint8_t *dst = (uint8_t*)malloc(width * height * 4);
    if (!dst) return NULL;

    const uint16_t *s = (const uint16_t*)src;
    uint8_t *d = dst;
    for (int i = 0; i < width * height; i++) {
        uint16_t p = s[i];
        // GL_UNSIGNED_SHORT_1_5_5_5_REV: A=bit15, R=bits14-10, G=bits9-5, B=bits4-0
        uint8_t b = (uint8_t)((p & 0x001F) << 3);       // B bits 4-0
        uint8_t g = (uint8_t)(((p >> 5)  & 0x1F) << 3); // G bits 9-5
        uint8_t r = (uint8_t)(((p >> 10) & 0x1F) << 3); // R bits 14-10
        uint8_t a = (uint8_t)(((p >> 15) & 0x01) * 255); // A bit 15
        d[0] = r; d[1] = g; d[2] = b; d[3] = a;
        d += 4;
    }
    return dst;
}

void bridge_FreeConvertedPixels(void *ptr)
{
    free(ptr);
}

#endif // __ANDROID__
