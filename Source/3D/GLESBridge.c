// GLESBridge.c
// OpenGL ES 3.0 emulation layer for Cro-Mag Rally Android port.
// Emulates the fixed-function OpenGL pipeline using GLES 3.0 shaders and VBOs.

#ifdef __ANDROID__

#include <GLES3/gl3.h>
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define BRIDGE_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "GLESBridge", __VA_ARGS__)
#define BRIDGE_ERR(...) __android_log_print(ANDROID_LOG_ERROR, "GLESBridge", __VA_ARGS__)

// ===========================
// CONSTANTS
// ===========================

#define MAX_IMM_VERTS       16384
#define MAX_IMM_INDICES     65536
#define MATRIX_STACK_DEPTH  32
#define MAX_LIGHTS          8

// Fixed-function constants (duplicated here to avoid header dependency)
#define BRIDGE_MODELVIEW    0x1700
#define BRIDGE_PROJECTION   0x1701
#define BRIDGE_TEXTURE_MAT  0x1702

#define BRIDGE_FOG_LINEAR   0x2601
#define BRIDGE_FOG_EXP      0x0800
#define BRIDGE_FOG_EXP2     0x0801

// ===========================
// MATRIX MATH
// ===========================

typedef float Mat4[16];

static void mat4_identity(Mat4 m) {
    memset(m, 0, sizeof(Mat4));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_multiply(const Mat4 a, const Mat4 b, Mat4 out) {
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0;
            for (int k = 0; k < 4; k++) {
                sum += a[k*4+row] * b[col*4+k];
            }
            out[col*4+row] = sum;
        }
    }
}

static void mat4_copy(const Mat4 src, Mat4 dst) {
    memcpy(dst, src, sizeof(Mat4));
}

static void mat4_translate(Mat4 m, float x, float y, float z) {
    Mat4 t;
    mat4_identity(t);
    t[12] = x; t[13] = y; t[14] = z;
    Mat4 tmp;
    mat4_multiply(m, t, tmp);
    mat4_copy(tmp, m);
}

static void mat4_scale(Mat4 m, float x, float y, float z) {
    Mat4 s;
    mat4_identity(s);
    s[0] = x; s[5] = y; s[10] = z;
    Mat4 tmp;
    mat4_multiply(m, s, tmp);
    mat4_copy(tmp, m);
}

static void mat4_rotate(Mat4 m, float angle_deg, float x, float y, float z) {
    float angle = angle_deg * (float)(M_PI / 180.0);
    float c = cosf(angle), s = sinf(angle);
    float len = sqrtf(x*x + y*y + z*z);
    if (len > 0) { x /= len; y /= len; z /= len; }

    Mat4 r;
    r[0] = c + x*x*(1-c);   r[4] = x*y*(1-c)-z*s;  r[8]  = x*z*(1-c)+y*s;  r[12] = 0;
    r[1] = y*x*(1-c)+z*s;   r[5] = c + y*y*(1-c);  r[9]  = y*z*(1-c)-x*s;  r[13] = 0;
    r[2] = z*x*(1-c)-y*s;   r[6] = y*z*(1-c)+x*s;  r[10] = c + z*z*(1-c);  r[14] = 0;
    r[3] = 0;                r[7] = 0;               r[11] = 0;               r[15] = 1;

    Mat4 tmp;
    mat4_multiply(m, r, tmp);
    mat4_copy(tmp, m);
}

static void mat4_ortho(Mat4 m, double l, double r, double b, double t, double n, double f) {
    memset(m, 0, sizeof(Mat4));
    m[0]  = (float)(2.0/(r-l));
    m[5]  = (float)(2.0/(t-b));
    m[10] = (float)(-2.0/(f-n));
    m[12] = (float)(-(r+l)/(r-l));
    m[13] = (float)(-(t+b)/(t-b));
    m[14] = (float)(-(f+n)/(f-n));
    m[15] = 1.0f;
}

static void mat4_frustum(Mat4 m, double l, double r, double b, double t, double n, double f) {
    memset(m, 0, sizeof(Mat4));
    m[0]  = (float)(2.0*n/(r-l));
    m[5]  = (float)(2.0*n/(t-b));
    m[8]  = (float)((r+l)/(r-l));
    m[9]  = (float)((t+b)/(t-b));
    m[10] = (float)(-(f+n)/(f-n));
    m[11] = -1.0f;
    m[14] = (float)(-2.0*f*n/(f-n));
}

// Get the normal matrix (inverse transpose of upper-left 3x3 of modelview).
// LIMITATION: This implementation uses only column-normalization, which is
// correct for rotations and uniform scaling but may produce incorrect
// normals under non-uniform scaling. For the game's typical transformations
// (rotation + uniform scale) this is sufficient.
static void mat4_normal_matrix(const Mat4 mv, float nm[9]) {
    // For orthogonal matrices, the normal matrix equals the upper-left 3x3 of the modelview
    // For the general case, we'd need to compute the inverse transpose
    // As an optimization, we use the upper-left 3x3 directly (good enough for uniform scaling)
    nm[0] = mv[0]; nm[1] = mv[1]; nm[2] = mv[2];
    nm[3] = mv[4]; nm[4] = mv[5]; nm[5] = mv[6];
    nm[6] = mv[8]; nm[7] = mv[9]; nm[8] = mv[10];
    // Normalize each column
    for (int i = 0; i < 3; i++) {
        float len = sqrtf(nm[i]*nm[i] + nm[i+3]*nm[i+3] + nm[i+6]*nm[i+6]);
        if (len > 0.0001f) {
            nm[i] /= len; nm[i+3] /= len; nm[i+6] /= len;
        }
    }
}

// ===========================
// SHADER SOURCES
// ===========================

static const char* kVertexShaderSrc =
"#version 300 es\n"
"precision highp float;\n"
"\n"
"// Attributes with explicit locations\n"
"layout(location = 0) in vec3 a_position;\n"
"layout(location = 1) in vec3 a_normal;\n"
"layout(location = 2) in vec2 a_texcoord;\n"
"layout(location = 3) in vec4 a_color;\n"
"\n"
"// Uniforms - matrices\n"
"uniform mat4 u_mvMatrix;\n"
"uniform mat4 u_projMatrix;\n"
"uniform mat3 u_normalMatrix;\n"
"uniform mat4 u_texMatrix;\n"
"\n"
"// Uniforms - lighting\n"
"uniform bool u_lightingEnabled;\n"
"uniform vec4 u_ambientLight;\n"
"uniform int  u_numLights;\n"
"uniform vec4 u_lightPos[8];    // w=0 means directional\n"
"uniform vec4 u_lightDiffuse[8];\n"
"uniform vec4 u_lightAmbient[8];\n"
"\n"
"// Outputs\n"
"out vec4 v_color;\n"
"out vec2 v_texcoord;\n"
"out float v_fogDepth;\n"
"\n"
"void main() {\n"
"    vec4 eyePos = u_mvMatrix * vec4(a_position, 1.0);\n"
"    gl_Position = u_projMatrix * eyePos;\n"
"    v_fogDepth = -eyePos.z;\n"
"\n"
"    if (u_lightingEnabled) {\n"
"        vec3 eyeNormal = normalize(u_normalMatrix * a_normal);\n"
"        // GL_COLOR_MATERIAL GL_AMBIENT_AND_DIFFUSE: vertex color = material color\n"
"        vec4 matColor = a_color;\n"
"        vec4 color = u_ambientLight * matColor;\n"
"        for (int i = 0; i < u_numLights; i++) {\n"
"            vec3 lightDir;\n"
"            if (u_lightPos[i].w == 0.0) {\n"
"                lightDir = normalize(u_lightPos[i].xyz);\n"
"            } else {\n"
"                lightDir = normalize(u_lightPos[i].xyz - eyePos.xyz);\n"
"            }\n"
"            float nDotL = max(dot(eyeNormal, lightDir), 0.0);\n"
"            color += (u_lightAmbient[i] * matColor + nDotL * u_lightDiffuse[i] * matColor);\n"
"        }\n"
"        v_color = clamp(color, 0.0, 1.0);\n"
"        v_color.a = matColor.a;\n"
"    } else {\n"
"        v_color = a_color;\n"
"    }\n"
"\n"
"    vec4 tc = u_texMatrix * vec4(a_texcoord, 0.0, 1.0);\n"
"    v_texcoord = tc.xy;\n"
"}\n";

static const char* kFragmentShaderSrc =
"#version 300 es\n"
"precision mediump float;\n"
"\n"
"in vec4 v_color;\n"
"in vec2 v_texcoord;\n"
"in float v_fogDepth;\n"
"\n"
"uniform sampler2D u_texture0;\n"
"uniform bool u_textureEnabled;\n"
"uniform int  u_texEnvMode;   // 0=MODULATE, 1=DECAL, 2=REPLACE\n"
"\n"
"uniform bool  u_alphaTestEnabled;\n"
"uniform int   u_alphaFunc;   // GL_NOTEQUAL=0x205, GL_GREATER=0x204, etc.\n"
"uniform float u_alphaRef;\n"
"\n"
"uniform bool  u_fogEnabled;\n"
"uniform int   u_fogMode;     // 0=LINEAR, 1=EXP, 2=EXP2\n"
"uniform float u_fogStart;\n"
"uniform float u_fogEnd;\n"
"uniform float u_fogDensity;\n"
"uniform vec4  u_fogColor;\n"
"\n"
"out vec4 fragColor;\n"
"\n"
"void main() {\n"
"    vec4 color = v_color;\n"
"\n"
"    if (u_textureEnabled) {\n"
"        vec4 texColor = texture(u_texture0, v_texcoord);\n"
"        if (u_texEnvMode == 1) {         // GL_DECAL\n"
"            color.rgb = mix(color.rgb, texColor.rgb, texColor.a);\n"
"        } else if (u_texEnvMode == 2) {  // GL_REPLACE\n"
"            color = texColor;\n"
"        } else {                          // GL_MODULATE (default)\n"
"            color *= texColor;\n"
"        }\n"
"    }\n"
"\n"
"    if (u_alphaTestEnabled) {\n"
"        float a = color.a;\n"
"        float ref = u_alphaRef;\n"
"        bool pass;\n"
"        if      (u_alphaFunc == 0x0205) pass = (a != ref);   // GL_NOTEQUAL\n"
"        else if (u_alphaFunc == 0x0204) pass = (a >  ref);   // GL_GREATER\n"
"        else if (u_alphaFunc == 0x0206) pass = (a >= ref);   // GL_GEQUAL\n"
"        else if (u_alphaFunc == 0x0201) pass = (a <  ref);   // GL_LESS\n"
"        else if (u_alphaFunc == 0x0203) pass = (a <= ref);   // GL_LEQUAL\n"
"        else if (u_alphaFunc == 0x0202) pass = (a == ref);   // GL_EQUAL\n"
"        else if (u_alphaFunc == 0x0207) pass = true;          // GL_ALWAYS\n"
"        else                             pass = false;          // GL_NEVER\n"
"        if (!pass) discard;\n"
"    }\n"
"\n"
"    if (u_fogEnabled) {\n"
"        float fogFactor;\n"
"        float depth = v_fogDepth;\n"
"        if (u_fogMode == 1) {        // EXP\n"
"            fogFactor = exp(-u_fogDensity * depth);\n"
"        } else if (u_fogMode == 2) { // EXP2\n"
"            fogFactor = exp(-(u_fogDensity * depth) * (u_fogDensity * depth));\n"
"        } else {                     // LINEAR\n"
"            fogFactor = (u_fogEnd - depth) / (u_fogEnd - u_fogStart);\n"
"        }\n"
"        fogFactor = clamp(fogFactor, 0.0, 1.0);\n"
"        color.rgb = mix(u_fogColor.rgb, color.rgb, fogFactor);\n"
"    }\n"
"\n"
"    fragColor = color;\n"
"}\n";

// ===========================
// BRIDGE STATE
// ===========================

typedef struct {
    Mat4     stack[MATRIX_STACK_DEPTH];
    int      top;
} MatrixStack;

typedef struct {
    float pos[4];   // position (w=0 for directional)
    float diffuse[4];
    float ambient[4];
    bool  enabled;
} LightState;

typedef struct {
    float position[3];
    float normal[3];
    float texcoord[2];
    float color[4];
} ImmVertex;

typedef struct {
    // Shader program
    GLuint  program;
    // Attribute locations
    GLint   aPosition;
    GLint   aNormal;
    GLint   aTexCoord;
    GLint   aColor;
    // Uniform locations
    GLint   uMVMatrix;
    GLint   uProjMatrix;
    GLint   uNormalMatrix;
    GLint   uTexMatrix;
    GLint   uLightingEnabled;
    GLint   uAmbientLight;
    GLint   uNumLights;
    GLint   uLightPos;
    GLint   uLightDiffuse;
    GLint   uLightAmbient;
    GLint   uTextureEnabled;
    GLint   uTexEnvMode;
    GLint   uAlphaTestEnabled;
    GLint   uAlphaFunc;
    GLint   uAlphaRef;
    GLint   uFogEnabled;
    GLint   uFogMode;
    GLint   uFogStart;
    GLint   uFogEnd;
    GLint   uFogDensity;
    GLint   uFogColor;
    GLint   uTexture0;
} ShaderState;

static struct {
    ShaderState shader;

    // Matrix stacks
    MatrixStack mv;    // modelview
    MatrixStack proj;  // projection
    MatrixStack tex;   // texture
    int         activeStack; // which stack is current: BRIDGE_MODELVIEW, BRIDGE_PROJECTION, BRIDGE_TEXTURE_MAT

    // Lights
    float       ambientLight[4];
    LightState  lights[MAX_LIGHTS];
    int         numEnabledLights;

    // Material
    float       materialDiffuse[4];
    float       materialAmbient[4];

    // Fog
    bool        fogEnabled;
    int         fogMode;      // 0=LINEAR, 1=EXP, 2=EXP2
    float       fogStart;
    float       fogEnd;
    float       fogDensity;
    float       fogColor[4];

    // Lighting enable
    bool        lightingEnabled;

    // Texture
    bool        textureEnabled;
    int         texEnvMode;   // 0=MODULATE, 1=DECAL, 2=REPLACE

    // Alpha test
    bool        alphaTestEnabled;
    int         alphaFunc;
    float       alphaRef;

    // Blend state (tracked because GL_BLEND_SRC/DST are desktop-only enums)
    GLenum      blendSrc;       // default GL_SRC_ALPHA
    GLenum      blendDst;       // default GL_ONE_MINUS_SRC_ALPHA

    // Normalize (no-op in GLES, handled by normalMatrix)
    bool        normalizeEnabled;

    // Current vertex attributes (for immediate mode)
    float       currentNormal[3];
    float       currentTexCoord[2];
    float       currentColor[4];

    // Immediate mode
    GLenum      immMode;
    bool        inImmMode;
    ImmVertex   immVerts[MAX_IMM_VERTS];
    uint16_t    convertedIndices[MAX_IMM_INDICES]; // for GL_QUADS / mode conversion
    int         immVertCount;

    // VBOs
    GLuint      immVBO;
    GLuint      immIBO;
    GLuint      arrVBO;
    GLuint      arrIBO;

    // VAOs
    GLuint      immVAO;
    GLuint      arrVAO;

    // Vertex array state
    bool        vaEnabled[4];  // 0=vertex,1=normal,2=color,3=texcoord
    const void* vaPtrs[4];
    GLint       vaSizes[4];
    GLenum      vaTypes[4];
    GLsizei     vaStrides[4];

    // Dirty flag for uniforms
    bool        uniformsDirty;

    // Dirty flag for each uniform category
    bool        matricesDirty;
    bool        lightsDirty;
    bool        fogDirty;
    bool        alphaDirty;
    bool        texEnvDirty;
} gBridge;

// ===========================
// SHADER COMPILATION
// ===========================

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        BRIDGE_ERR("Shader compile error: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    // Bind attribute locations before linking
    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_normal");
    glBindAttribLocation(prog, 2, "a_texcoord");
    glBindAttribLocation(prog, 3, "a_color");
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        BRIDGE_ERR("Program link error: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void GetUniformLocations(ShaderState* s) {
#define U(name) s->u##name = glGetUniformLocation(s->program, "u_" #name)
    U(MVMatrix);
    U(ProjMatrix);
    U(NormalMatrix);
    U(TexMatrix);
    U(LightingEnabled);
    U(AmbientLight);
    U(NumLights);
    U(LightPos);
    U(LightDiffuse);
    U(LightAmbient);
    U(TextureEnabled);
    U(TexEnvMode);
    U(AlphaTestEnabled);
    U(AlphaFunc);
    U(AlphaRef);
    U(FogEnabled);
    U(FogMode);
    U(FogStart);
    U(FogEnd);
    U(FogDensity);
    U(FogColor);
    U(Texture0);
#undef U
    s->aPosition = 0;
    s->aNormal   = 1;
    s->aTexCoord = 2;
    s->aColor    = 3;
}

// ===========================
// UPLOAD UNIFORMS
// ===========================

static void UploadUniforms(void) {
    ShaderState* s = &gBridge.shader;

    // Matrices
    {
        // MVP = proj * mv
        float mv[16], proj[16], nm[9], tex[16];
        memcpy(mv,   gBridge.mv.stack[gBridge.mv.top],   16*sizeof(float));
        memcpy(proj, gBridge.proj.stack[gBridge.proj.top], 16*sizeof(float));
        memcpy(tex,  gBridge.tex.stack[gBridge.tex.top],  16*sizeof(float));

        glUniformMatrix4fv(s->uMVMatrix,   1, GL_FALSE, mv);
        glUniformMatrix4fv(s->uProjMatrix, 1, GL_FALSE, proj);
        glUniformMatrix4fv(s->uTexMatrix,  1, GL_FALSE, tex);

        mat4_normal_matrix(mv, nm);
        glUniformMatrix3fv(s->uNormalMatrix, 1, GL_FALSE, nm);
    }

    // Lighting
    glUniform1i(s->uLightingEnabled, gBridge.lightingEnabled ? 1 : 0);
    glUniform4fv(s->uAmbientLight, 1, gBridge.ambientLight);

    {
        int n = 0;
        float lightPos[MAX_LIGHTS*4];
        float lightDiff[MAX_LIGHTS*4];
        float lightAmb[MAX_LIGHTS*4];
        for (int i = 0; i < MAX_LIGHTS; i++) {
            if (gBridge.lights[i].enabled) {
                memcpy(lightPos  + n*4, gBridge.lights[i].pos,     4*sizeof(float));
                memcpy(lightDiff + n*4, gBridge.lights[i].diffuse,  4*sizeof(float));
                memcpy(lightAmb  + n*4, gBridge.lights[i].ambient,  4*sizeof(float));
                n++;
            }
        }
        glUniform1i(s->uNumLights, n);
        if (n > 0) {
            glUniform4fv(s->uLightPos,     n, lightPos);
            glUniform4fv(s->uLightDiffuse, n, lightDiff);
            glUniform4fv(s->uLightAmbient, n, lightAmb);
        }
    }

    // Texture
    glUniform1i(s->uTextureEnabled, gBridge.textureEnabled ? 1 : 0);
    glUniform1i(s->uTexEnvMode,     gBridge.texEnvMode);
    glUniform1i(s->uTexture0,       0);

    // Alpha test
    glUniform1i(s->uAlphaTestEnabled, gBridge.alphaTestEnabled ? 1 : 0);
    glUniform1i(s->uAlphaFunc,        gBridge.alphaFunc);
    glUniform1f(s->uAlphaRef,         gBridge.alphaRef);

    // Fog
    glUniform1i(s->uFogEnabled,  gBridge.fogEnabled ? 1 : 0);
    glUniform1i(s->uFogMode,     gBridge.fogMode);
    glUniform1f(s->uFogStart,    gBridge.fogStart);
    glUniform1f(s->uFogEnd,      gBridge.fogEnd);
    glUniform1f(s->uFogDensity,  gBridge.fogDensity);
    glUniform4fv(s->uFogColor,   1, gBridge.fogColor);
}

// ===========================
// BRIDGE INIT / SHUTDOWN
// ===========================

void bridge_Init(void) {
    memset(&gBridge, 0, sizeof(gBridge));

    // Initialize matrix stacks to identity
    mat4_identity(gBridge.mv.stack[0]);
    mat4_identity(gBridge.proj.stack[0]);
    mat4_identity(gBridge.tex.stack[0]);
    gBridge.activeStack = BRIDGE_MODELVIEW;

    // Default material
    gBridge.materialDiffuse[0] = gBridge.materialDiffuse[1] = gBridge.materialDiffuse[2] = 1.0f;
    gBridge.materialDiffuse[3] = 1.0f;
    gBridge.materialAmbient[0] = gBridge.materialAmbient[1] = gBridge.materialAmbient[2] = 1.0f;
    gBridge.materialAmbient[3] = 1.0f;
    gBridge.ambientLight[0] = gBridge.ambientLight[1] = gBridge.ambientLight[2] = 0.2f;
    gBridge.ambientLight[3] = 1.0f;

    // Default current color (white)
    gBridge.currentColor[0] = gBridge.currentColor[1] = gBridge.currentColor[2] = gBridge.currentColor[3] = 1.0f;

    // Default fog
    gBridge.fogMode = 0; // LINEAR
    gBridge.fogStart = 0; gBridge.fogEnd = 1; gBridge.fogDensity = 1;

    // Default alpha test (NOTEQUAL 0 = draw pixels where alpha != 0)
    gBridge.alphaFunc = 0x0205; // GL_NOTEQUAL
    gBridge.alphaRef = 0.0f;

    // Default blend
    gBridge.blendSrc = GL_SRC_ALPHA;
    gBridge.blendDst = GL_ONE_MINUS_SRC_ALPHA;

    // Default texture env
    gBridge.texEnvMode = 0; // MODULATE

    // Build shader
    GLuint vert = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (!vert || !frag) {
        BRIDGE_ERR("Failed to compile shaders!");
        return;
    }
    gBridge.shader.program = LinkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (!gBridge.shader.program) {
        BRIDGE_ERR("Failed to link program!");
        return;
    }
    GetUniformLocations(&gBridge.shader);

    // Create VBOs/VAOs for immediate mode
    glGenVertexArrays(1, &gBridge.immVAO);
    glGenBuffers(1, &gBridge.immVBO);
    glGenBuffers(1, &gBridge.immIBO);

    // Create VBOs/VAOs for vertex array mode
    glGenVertexArrays(1, &gBridge.arrVAO);
    glGenBuffers(1, &gBridge.arrVBO);
    glGenBuffers(1, &gBridge.arrIBO);

    BRIDGE_LOG("bridge_Init: OK (program=%u)", gBridge.shader.program);
}

void bridge_Shutdown(void) {
    if (gBridge.shader.program) {
        glDeleteProgram(gBridge.shader.program);
        gBridge.shader.program = 0;
    }
    glDeleteVertexArrays(1, &gBridge.immVAO);
    glDeleteVertexArrays(1, &gBridge.arrVAO);
    glDeleteBuffers(1, &gBridge.immVBO);
    glDeleteBuffers(1, &gBridge.immIBO);
    glDeleteBuffers(1, &gBridge.arrVBO);
    glDeleteBuffers(1, &gBridge.arrIBO);
}

// ===========================
// HELPER: USE PROGRAM + UPLOAD
// ===========================

static void ActivateProgram(void) {
    glUseProgram(gBridge.shader.program);
    UploadUniforms();
}

// ===========================
// MATRIX STACK HELPERS
// ===========================

static MatrixStack* ActiveStack(void) {
    if (gBridge.activeStack == BRIDGE_PROJECTION)  return &gBridge.proj;
    if (gBridge.activeStack == BRIDGE_TEXTURE_MAT) return &gBridge.tex;
    return &gBridge.mv;
}

void bridge_MatrixMode(GLenum mode) {
    gBridge.activeStack = (int)mode;
}

void bridge_PushMatrix(void) {
    MatrixStack* ms = ActiveStack();
    if (ms->top >= MATRIX_STACK_DEPTH-1) { BRIDGE_ERR("Matrix stack overflow"); return; }
    mat4_copy(ms->stack[ms->top], ms->stack[ms->top+1]);
    ms->top++;
}

void bridge_PopMatrix(void) {
    MatrixStack* ms = ActiveStack();
    if (ms->top <= 0) { BRIDGE_ERR("Matrix stack underflow"); return; }
    ms->top--;
}

void bridge_LoadIdentity(void) {
    mat4_identity(ActiveStack()->stack[ActiveStack()->top]);
}

void bridge_LoadMatrixf(const GLfloat* m) {
    mat4_copy(m, ActiveStack()->stack[ActiveStack()->top]);
}

void bridge_MultMatrixf(const GLfloat* m) {
    Mat4 tmp;
    mat4_multiply(ActiveStack()->stack[ActiveStack()->top], m, tmp);
    mat4_copy(tmp, ActiveStack()->stack[ActiveStack()->top]);
}

void bridge_Translatef(GLfloat x, GLfloat y, GLfloat z) {
    mat4_translate(ActiveStack()->stack[ActiveStack()->top], x, y, z);
}

void bridge_Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    mat4_rotate(ActiveStack()->stack[ActiveStack()->top], angle, x, y, z);
}

void bridge_Scalef(GLfloat x, GLfloat y, GLfloat z) {
    mat4_scale(ActiveStack()->stack[ActiveStack()->top], x, y, z);
}

void bridge_Ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    Mat4 m;
    mat4_ortho(m, l, r, b, t, n, f);
    Mat4 tmp;
    mat4_multiply(ActiveStack()->stack[ActiveStack()->top], m, tmp);
    mat4_copy(tmp, ActiveStack()->stack[ActiveStack()->top]);
}

void bridge_Frustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    Mat4 m;
    mat4_frustum(m, l, r, b, t, n, f);
    Mat4 tmp;
    mat4_multiply(ActiveStack()->stack[ActiveStack()->top], m, tmp);
    mat4_copy(tmp, ActiveStack()->stack[ActiveStack()->top]);
}

// ===========================
// ENABLE / DISABLE
// ===========================

void bridge_Enable(GLenum cap) {
    switch (cap) {
        case 0x0B50: gBridge.lightingEnabled = true;  return; // GL_LIGHTING
        case 0x0B60: gBridge.fogEnabled      = true;  return; // GL_FOG
        case 0x0BC0: gBridge.alphaTestEnabled = true; return; // GL_ALPHA_TEST
        case 0x0DE1: gBridge.textureEnabled  = true;  return; // GL_TEXTURE_2D
        case 0x0BA1: gBridge.normalizeEnabled = true; return; // GL_NORMALIZE
        case 0x803A: return;  // GL_RESCALE_NORMAL - no-op
        case 0x0B57: return;  // GL_COLOR_MATERIAL - no-op (always on)
        default:
            // Handle GL_LIGHT0..GL_LIGHT7
            if (cap >= 0x4000 && cap <= 0x4007) {
                gBridge.lights[cap - 0x4000].enabled = true;
                return;
            }
            glEnable(cap); return;
    }
}

void bridge_Disable(GLenum cap) {
    switch (cap) {
        case 0x0B50: gBridge.lightingEnabled  = false; return;
        case 0x0B60: gBridge.fogEnabled       = false; return;
        case 0x0BC0: gBridge.alphaTestEnabled = false; return;
        case 0x0DE1: gBridge.textureEnabled   = false; return;
        case 0x0BA1: gBridge.normalizeEnabled = false; return;
        case 0x803A: return;  // GL_RESCALE_NORMAL
        case 0x0B57: return;  // GL_COLOR_MATERIAL
        default:
            // Handle GL_LIGHT0..GL_LIGHT7
            if (cap >= 0x4000 && cap <= 0x4007) {
                gBridge.lights[cap - 0x4000].enabled = false;
                return;
            }
            glDisable(cap); return;
    }
}

GLboolean bridge_IsEnabled(GLenum cap) {
    switch (cap) {
        case 0x0B50: return gBridge.lightingEnabled  ? GL_TRUE : GL_FALSE;
        case 0x0B60: return gBridge.fogEnabled       ? GL_TRUE : GL_FALSE;
        case 0x0BC0: return gBridge.alphaTestEnabled ? GL_TRUE : GL_FALSE;
        case 0x0DE1: return gBridge.textureEnabled   ? GL_TRUE : GL_FALSE;
        case 0x0BA1: return gBridge.normalizeEnabled ? GL_TRUE : GL_FALSE;
        default:
            if (cap >= 0x4000 && cap <= 0x4007)
                return gBridge.lights[cap - 0x4000].enabled ? GL_TRUE : GL_FALSE;
            return glIsEnabled(cap);
    }
}

void bridge_GetFloatv(GLenum pname, GLfloat* params) {
    if (pname == 0x0B00) { // GL_CURRENT_COLOR
        memcpy(params, gBridge.currentColor, 4*sizeof(GLfloat));
        return;
    }
    glGetFloatv(pname, params);
}

void bridge_GetIntegerv(GLenum pname, GLint* params) {
    if (pname == 0x0BE1) { *params = (GLint)gBridge.blendSrc; return; } // GL_BLEND_SRC
    if (pname == 0x0BE0) { *params = (GLint)gBridge.blendDst; return; } // GL_BLEND_DST
    glGetIntegerv(pname, params);
}

void bridge_BlendFunc(GLenum sfactor, GLenum dfactor) {
    gBridge.blendSrc = sfactor;
    gBridge.blendDst = dfactor;
    glBlendFunc(sfactor, dfactor);
}

void bridge_GetBooleanv(GLenum pname, GLboolean* params) {
    glGetBooleanv(pname, params);
}

// ===========================
// LIGHTING
// ===========================

void bridge_Lightfv(GLenum light, GLenum pname, const GLfloat* params) {
    int idx = (int)(light - 0x4000); // GL_LIGHT0 = 0x4000
    if (idx < 0 || idx >= MAX_LIGHTS) return;
    switch (pname) {
        case 0x1203: // GL_POSITION
            memcpy(gBridge.lights[idx].pos, params, 4*sizeof(float));
            break;
        case 0x1201: // GL_DIFFUSE
            memcpy(gBridge.lights[idx].diffuse, params, 4*sizeof(float));
            break;
        case 0x1200: // GL_AMBIENT
            memcpy(gBridge.lights[idx].ambient, params, 4*sizeof(float));
            break;
    }
}

void bridge_LightModelfv(GLenum pname, const GLfloat* params) {
    if (pname == 0x0B53) { // GL_LIGHT_MODEL_AMBIENT
        memcpy(gBridge.ambientLight, params, 4*sizeof(float));
    }
}

void bridge_Materialfv(GLenum face, GLenum pname, const GLfloat* params) {
    (void)face;
    switch (pname) {
        case 0x1201: // GL_DIFFUSE
        case 0x1602: // GL_AMBIENT_AND_DIFFUSE
            memcpy(gBridge.materialDiffuse, params, 4*sizeof(float));
            if (pname == 0x1602)
                memcpy(gBridge.materialAmbient, params, 4*sizeof(float));
            break;
        case 0x1200: // GL_AMBIENT
            memcpy(gBridge.materialAmbient, params, 4*sizeof(float));
            break;
    }
}

// ===========================
// FOG
// ===========================

void bridge_Fogfv(GLenum pname, const GLfloat* params) {
    switch (pname) {
        case 0x0B66: memcpy(gBridge.fogColor, params, 4*sizeof(float)); break; // GL_FOG_COLOR
        case 0x0B63: gBridge.fogStart   = params[0]; break;                    // GL_FOG_START
        case 0x0B64: gBridge.fogEnd     = params[0]; break;                    // GL_FOG_END
        case 0x0B62: gBridge.fogDensity = params[0]; break;                    // GL_FOG_DENSITY
    }
}

void bridge_Fogf(GLenum pname, GLfloat param) {
    switch (pname) {
        case 0x0B63: gBridge.fogStart   = param; break;
        case 0x0B64: gBridge.fogEnd     = param; break;
        case 0x0B62: gBridge.fogDensity = param; break;
    }
}

void bridge_Fogi(GLenum pname, GLint param) {
    if (pname == 0x0B65) { // GL_FOG_MODE
        if      ((GLuint)param == 0x2601) gBridge.fogMode = 0; // GL_LINEAR
        else if ((GLuint)param == 0x0800) gBridge.fogMode = 1; // GL_EXP
        else if ((GLuint)param == 0x0801) gBridge.fogMode = 2; // GL_EXP2
    }
}

// ===========================
// ALPHA TEST
// ===========================

void bridge_AlphaFunc(GLenum func, GLfloat ref) {
    gBridge.alphaFunc = (int)func;
    gBridge.alphaRef  = ref;
}

// ===========================
// TEXTURE ENVIRONMENT
// ===========================

void bridge_TexEnvi(GLenum target, GLenum pname, GLint param) {
    (void)target;
    if (pname == 0x2200) { // GL_TEXTURE_ENV_MODE
        if      ((GLuint)param == 0x2100) gBridge.texEnvMode = 0; // GL_MODULATE
        else if ((GLuint)param == 0x2101) gBridge.texEnvMode = 1; // GL_DECAL
        else if ((GLuint)param == 0x1E01) gBridge.texEnvMode = 2; // GL_REPLACE
    }
}

// ===========================
// CURRENT VERTEX ATTRIBUTES
// ===========================

void bridge_Normal3f(GLfloat x, GLfloat y, GLfloat z) {
    gBridge.currentNormal[0] = x;
    gBridge.currentNormal[1] = y;
    gBridge.currentNormal[2] = z;
}

void bridge_Normal3fv(const GLfloat* v) {
    memcpy(gBridge.currentNormal, v, 3*sizeof(float));
}

void bridge_TexCoord2f(GLfloat s, GLfloat t) {
    gBridge.currentTexCoord[0] = s;
    gBridge.currentTexCoord[1] = t;
}

void bridge_TexCoord2fv(const GLfloat* v) {
    memcpy(gBridge.currentTexCoord, v, 2*sizeof(float));
}

void bridge_Color3f(GLfloat r, GLfloat g, GLfloat b) {
    gBridge.currentColor[0] = r;
    gBridge.currentColor[1] = g;
    gBridge.currentColor[2] = b;
    gBridge.currentColor[3] = 1.0f;
}

void bridge_Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    gBridge.currentColor[0] = r;
    gBridge.currentColor[1] = g;
    gBridge.currentColor[2] = b;
    gBridge.currentColor[3] = a;
}

void bridge_Color4fv(const GLfloat* v) {
    memcpy(gBridge.currentColor, v, 4*sizeof(float));
}

void bridge_Color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
    gBridge.currentColor[0] = r / 255.0f;
    gBridge.currentColor[1] = g / 255.0f;
    gBridge.currentColor[2] = b / 255.0f;
    gBridge.currentColor[3] = a / 255.0f;
}

// ===========================
// IMMEDIATE MODE
// ===========================

void bridge_Begin(GLenum mode) {
    gBridge.immMode      = mode;
    gBridge.inImmMode    = true;
    gBridge.immVertCount = 0;
}

void bridge_Vertex2f(GLfloat x, GLfloat y) {
    bridge_Vertex3f(x, y, 0.0f);
}

void bridge_Vertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (gBridge.immVertCount >= MAX_IMM_VERTS) {
        BRIDGE_ERR("Immediate mode vertex buffer overflow");
        return;
    }
    ImmVertex* v = &gBridge.immVerts[gBridge.immVertCount++];
    v->position[0] = x; v->position[1] = y; v->position[2] = z;
    memcpy(v->normal,   gBridge.currentNormal,   3*sizeof(float));
    memcpy(v->texcoord, gBridge.currentTexCoord, 2*sizeof(float));
    memcpy(v->color,    gBridge.currentColor,    4*sizeof(float));
}

void bridge_Vertex3fv(const GLfloat* v) {
    bridge_Vertex3f(v[0], v[1], v[2]);
}

// Convert quads/polygons to triangles and flush
static void FlushImmMode(void) {
    if (gBridge.immVertCount == 0) return;

    // Build index buffer based on primitive type (reuse global convertedIndices buffer)
    uint16_t* indices = gBridge.convertedIndices;
    int indexCount = 0;
    int n = gBridge.immVertCount;
    GLenum renderMode = gBridge.immMode;

    switch (gBridge.immMode) {
        case 0x0007: // GL_QUADS: convert to triangles
        {
            renderMode = GL_TRIANGLES;
            int quads = n / 4;
            for (int i = 0; i < quads && indexCount + 6 <= MAX_IMM_INDICES; i++) {
                int b = i * 4;
                indices[indexCount++] = b+0;
                indices[indexCount++] = b+1;
                indices[indexCount++] = b+2;
                indices[indexCount++] = b+0;
                indices[indexCount++] = b+2;
                indices[indexCount++] = b+3;
            }
            break;
        }
        case 0x0008: // GL_QUAD_STRIP: convert to triangle strip
        {
            renderMode = GL_TRIANGLES;
            for (int i = 0; i + 3 < n && indexCount + 6 <= MAX_IMM_INDICES; i += 2) {
                indices[indexCount++] = i+0;
                indices[indexCount++] = i+1;
                indices[indexCount++] = i+2;
                indices[indexCount++] = i+1;
                indices[indexCount++] = i+3;
                indices[indexCount++] = i+2;
            }
            break;
        }
        case 0x0009: // GL_POLYGON: convert to triangle fan
        {
            renderMode = GL_TRIANGLES;
            for (int i = 1; i + 1 < n && indexCount + 3 <= MAX_IMM_INDICES; i++) {
                indices[indexCount++] = 0;
                indices[indexCount++] = i;
                indices[indexCount++] = i+1;
            }
            break;
        }
        default:
        {
            // GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP
            for (int i = 0; i < n && indexCount < MAX_IMM_INDICES; i++) {
                indices[indexCount++] = i;
            }
            break;
        }
    }

    if (indexCount == 0) return;

    // Activate shader and upload uniforms
    ActivateProgram();

    // Upload vertex data to VBO
    glBindVertexArray(gBridge.immVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gBridge.immVBO);
    glBufferData(GL_ARRAY_BUFFER, n * sizeof(ImmVertex), gBridge.immVerts, GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gBridge.immIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(uint16_t), indices, GL_STREAM_DRAW);

    // Set up vertex attrib pointers
    int stride = sizeof(ImmVertex);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, position));
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, normal));
    glEnableVertexAttribArray(2); // texcoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, texcoord));
    glEnableVertexAttribArray(3); // color
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, color));

    glDrawElements(renderMode, indexCount, GL_UNSIGNED_SHORT, 0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    gBridge.immVertCount = 0;
}

void bridge_End(void) {
    FlushImmMode();
    gBridge.inImmMode = false;
}

// ===========================
// CLIENT-SIDE VERTEX ARRAYS
// ===========================

void bridge_EnableClientState(GLenum array) {
    switch (array) {
        case 0x8074: gBridge.vaEnabled[0] = true; break; // GL_VERTEX_ARRAY
        case 0x8075: gBridge.vaEnabled[1] = true; break; // GL_NORMAL_ARRAY
        case 0x8076: gBridge.vaEnabled[2] = true; break; // GL_COLOR_ARRAY
        case 0x8078: gBridge.vaEnabled[3] = true; break; // GL_TEXTURE_COORD_ARRAY
    }
}

void bridge_DisableClientState(GLenum array) {
    switch (array) {
        case 0x8074: gBridge.vaEnabled[0] = false; break;
        case 0x8075: gBridge.vaEnabled[1] = false; break;
        case 0x8076: gBridge.vaEnabled[2] = false; break;
        case 0x8078: gBridge.vaEnabled[3] = false; break;
    }
}

void bridge_VertexPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) {
    gBridge.vaSizes[0] = size; gBridge.vaTypes[0] = type;
    gBridge.vaStrides[0] = stride; gBridge.vaPtrs[0] = ptr;
}

void bridge_NormalPointer(GLenum type, GLsizei stride, const void* ptr) {
    gBridge.vaSizes[1] = 3; gBridge.vaTypes[1] = type;
    gBridge.vaStrides[1] = stride; gBridge.vaPtrs[1] = ptr;
}

void bridge_ColorPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) {
    gBridge.vaSizes[2] = size; gBridge.vaTypes[2] = type;
    gBridge.vaStrides[2] = stride; gBridge.vaPtrs[2] = ptr;
}

void bridge_TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) {
    gBridge.vaSizes[3] = size; gBridge.vaTypes[3] = type;
    gBridge.vaStrides[3] = stride; gBridge.vaPtrs[3] = ptr;
}

// Helper: compute byte size of a GL type
static int GLTypeSize(GLenum type) {
    switch (type) {
        case GL_FLOAT:          return 4;
        case GL_UNSIGNED_BYTE:  return 1;
        case GL_UNSIGNED_SHORT: return 2;
        case GL_UNSIGNED_INT:   return 4;
        case GL_SHORT:          return 2;
        case GL_INT:            return 4;
        default:                return 4;
    }
}

// Helper: compute stride if 0 (tightly packed)
static int ComputeStride(int slot) {
    if (gBridge.vaStrides[slot] != 0)
        return gBridge.vaStrides[slot];
    return gBridge.vaSizes[slot] * GLTypeSize(gBridge.vaTypes[slot]);
}

static void DrawVertexArrays(GLenum mode, int count, GLenum indexType, const void* indices, int indexCount) {
    // We need to build a packed ImmVertex buffer from the client-side arrays
    // Detect if we have enough room
    if (count > MAX_IMM_VERTS) {
        BRIDGE_ERR("Vertex array count %d exceeds bridge limit", count);
        return;
    }

    // Build packed interleaved vertices
    ImmVertex* verts = gBridge.immVerts;
    const uint8_t* posPtr  = (const uint8_t*)gBridge.vaPtrs[0];
    const uint8_t* nrmPtr  = (const uint8_t*)gBridge.vaPtrs[1];
    const uint8_t* colPtr  = (const uint8_t*)gBridge.vaPtrs[2];
    const uint8_t* tcPtr   = (const uint8_t*)gBridge.vaPtrs[3];

    int posStride = gBridge.vaEnabled[0] ? ComputeStride(0) : 0;
    int nrmStride = gBridge.vaEnabled[1] ? ComputeStride(1) : 0;
    int colStride = gBridge.vaEnabled[2] ? ComputeStride(2) : 0;
    int tcStride  = gBridge.vaEnabled[3] ? ComputeStride(3) : 0;

    for (int i = 0; i < count; i++) {
        ImmVertex* v = &verts[i];

        if (gBridge.vaEnabled[0] && posPtr) {
            const float* p = (const float*)(posPtr + i * posStride);
            v->position[0] = p[0]; v->position[1] = p[1];
            v->position[2] = (gBridge.vaSizes[0] >= 3) ? p[2] : 0.0f;
        } else {
            v->position[0] = v->position[1] = v->position[2] = 0.0f;
        }

        if (gBridge.vaEnabled[1] && nrmPtr) {
            const float* p = (const float*)(nrmPtr + i * nrmStride);
            v->normal[0] = p[0]; v->normal[1] = p[1]; v->normal[2] = p[2];
        } else {
            memcpy(v->normal, gBridge.currentNormal, 3*sizeof(float));
        }

        if (gBridge.vaEnabled[2] && colPtr) {
            if (gBridge.vaTypes[2] == GL_FLOAT) {
                const float* p = (const float*)(colPtr + i * colStride);
                v->color[0] = p[0]; v->color[1] = p[1];
                v->color[2] = p[2]; v->color[3] = (gBridge.vaSizes[2] >= 4) ? p[3] : 1.0f;
            } else if (gBridge.vaTypes[2] == GL_UNSIGNED_BYTE) {
                const uint8_t* p = colPtr + i * colStride;
                v->color[0] = p[0]/255.0f; v->color[1] = p[1]/255.0f;
                v->color[2] = p[2]/255.0f; v->color[3] = (gBridge.vaSizes[2]>=4) ? p[3]/255.0f : 1.0f;
            }
        } else {
            memcpy(v->color, gBridge.currentColor, 4*sizeof(float));
        }

        if (gBridge.vaEnabled[3] && tcPtr) {
            const float* p = (const float*)(tcPtr + i * tcStride);
            v->texcoord[0] = p[0]; v->texcoord[1] = p[1];
        } else {
            memcpy(v->texcoord, gBridge.currentTexCoord, 2*sizeof(float));
        }
    }

    // Handle primitive conversion for unsupported types
    GLenum renderMode = mode;
    const void* finalIndices = indices;
    int finalIndexCount = indexCount;

    if (mode == 0x0007 || mode == 0x0008 || mode == 0x0009) { // QUADS, QUAD_STRIP, POLYGON
        renderMode = GL_TRIANGLES;
        finalIndices = gBridge.convertedIndices;
        finalIndexCount = 0;

        if (mode == 0x0007) { // GL_QUADS
            int totalVerts = (indices ? indexCount : count);
            int quadCount = totalVerts / 4;
            for (int qi = 0; qi < quadCount && finalIndexCount+6 <= MAX_IMM_INDICES; qi++) {
                int b = qi * 4;
                int i0, i1, i2, i3;
                if (indices == NULL) {
                    i0 = b+0; i1 = b+1; i2 = b+2; i3 = b+3;
                } else if (indexType == GL_UNSIGNED_SHORT) {
                    const uint16_t* idx16 = (const uint16_t*)indices;
                    i0 = idx16[b+0]; i1 = idx16[b+1]; i2 = idx16[b+2]; i3 = idx16[b+3];
                } else if (indexType == GL_UNSIGNED_INT) {
                    const uint32_t* idx32 = (const uint32_t*)indices;
                    i0 = (int)idx32[b+0]; i1 = (int)idx32[b+1]; i2 = (int)idx32[b+2]; i3 = (int)idx32[b+3];
                } else {
                    const uint8_t* idx8 = (const uint8_t*)indices;
                    i0 = idx8[b+0]; i1 = idx8[b+1]; i2 = idx8[b+2]; i3 = idx8[b+3];
                }
                gBridge.convertedIndices[finalIndexCount++] = (uint16_t)i0;
                gBridge.convertedIndices[finalIndexCount++] = (uint16_t)i1;
                gBridge.convertedIndices[finalIndexCount++] = (uint16_t)i2;
                gBridge.convertedIndices[finalIndexCount++] = (uint16_t)i0;
                gBridge.convertedIndices[finalIndexCount++] = (uint16_t)i2;
                gBridge.convertedIndices[finalIndexCount++] = (uint16_t)i3;
            }
        }
    }

    ActivateProgram();

    glBindVertexArray(gBridge.arrVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gBridge.arrVBO);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(ImmVertex), verts, GL_STREAM_DRAW);

    int stride = sizeof(ImmVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, texcoord));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ImmVertex, color));

    if (finalIndices) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gBridge.arrIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, finalIndexCount * sizeof(uint16_t), finalIndices, GL_STREAM_DRAW);
        glDrawElements(renderMode, finalIndexCount, GL_UNSIGNED_SHORT, 0);
    } else {
        glDrawArrays(renderMode, 0, count);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void bridge_DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    // Determine max vertex index to know how many vertices to pack
    int maxIndex = 0;
    if (type == GL_UNSIGNED_SHORT) {
        const uint16_t* idx = (const uint16_t*)indices;
        for (int i = 0; i < count; i++) if (idx[i] > maxIndex) maxIndex = idx[i];
    } else if (type == GL_UNSIGNED_INT) {
        const uint32_t* idx = (const uint32_t*)indices;
        for (int i = 0; i < count; i++) if ((int)idx[i] > maxIndex) maxIndex = (int)idx[i];
    } else if (type == GL_UNSIGNED_BYTE) {
        const uint8_t* idx = (const uint8_t*)indices;
        for (int i = 0; i < count; i++) if (idx[i] > maxIndex) maxIndex = idx[i];
    }

    // Convert non-uint16 index types to uint16 before drawing
    // (Our bridge packs vertices to a uint16 IBO so we must convert here.)
    if (type == GL_UNSIGNED_INT && count > 0) {
        // Use heap allocation for safety with large index counts
        uint16_t* idx16 = (uint16_t*) malloc(count * sizeof(uint16_t));
        if (!idx16) { BRIDGE_ERR("OOM converting uint32 indices"); return; }
        const uint32_t* idx32 = (const uint32_t*)indices;
        for (int i = 0; i < count; i++) idx16[i] = (uint16_t)idx32[i];
        DrawVertexArrays(mode, maxIndex + 1, GL_UNSIGNED_SHORT, idx16, count);
        free(idx16);
    } else if (type == GL_UNSIGNED_BYTE && count > 0) {
        uint16_t* idx16 = (uint16_t*) malloc(count * sizeof(uint16_t));
        if (!idx16) { BRIDGE_ERR("OOM converting uint8 indices"); return; }
        const uint8_t* idx8 = (const uint8_t*)indices;
        for (int i = 0; i < count; i++) idx16[i] = (uint16_t)idx8[i];
        DrawVertexArrays(mode, maxIndex + 1, GL_UNSIGNED_SHORT, idx16, count);
        free(idx16);
    } else {
        DrawVertexArrays(mode, maxIndex + 1, type, indices, count);
    }
}

void bridge_DrawArrays(GLenum mode, GLint first, GLsizei count) {
    // Adjust pointers for 'first'
    const uint8_t* origPtrs[4];
    for (int i = 0; i < 4; i++) {
        origPtrs[i] = (const uint8_t*)gBridge.vaPtrs[i];
        if (gBridge.vaEnabled[i] && gBridge.vaPtrs[i]) {
            gBridge.vaPtrs[i] = (const uint8_t*)gBridge.vaPtrs[i] + first * ComputeStride(i);
        }
    }
    DrawVertexArrays(mode, count, GL_UNSIGNED_SHORT, NULL, count);
    for (int i = 0; i < 4; i++) gBridge.vaPtrs[i] = origPtrs[i];
}

#endif // __ANDROID__
