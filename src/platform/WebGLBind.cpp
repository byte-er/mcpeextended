// Web (Emscripten) only: bind every glad GLES1 entry point directly to
// Emscripten's GL implementation (legacy fixed-function emulation on WebGL).
// glfwGetProcAddress does not return the emulated fixed-function calls, so
// glad would otherwise leave them NULL. Generated from glad.h.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

bool webIsTouchDevice() {
	static int cached = -1;
	if (cached < 0)
		cached = EM_ASM_INT({ return (window.matchMedia && matchMedia('(pointer: coarse)').matches) ? 1 : 0; });
	return cached == 1;
}

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

extern "C" {
extern void* glad_glActiveTexture;
extern void* glad_glAlphaFunc;
extern void* glad_glBindBuffer;
extern void* glad_glBindTexture;
extern void* glad_glBlendFunc;
extern void* glad_glBufferData;
extern void* glad_glBufferSubData;
extern void* glad_glClear;
extern void* glad_glClearColor;
extern void* glad_glClearDepthf;
extern void* glad_glClearStencil;
extern void* glad_glClientActiveTexture;
extern void* glad_glColor4f;
extern void* glad_glColor4ub;
extern void* glad_glColorMask;
extern void* glad_glColorPointer;
extern void* glad_glCompressedTexImage2D;
extern void* glad_glCompressedTexSubImage2D;
extern void* glad_glCopyTexImage2D;
extern void* glad_glCopyTexSubImage2D;
extern void* glad_glCullFace;
extern void* glad_glDeleteBuffers;
extern void* glad_glDeleteTextures;
extern void* glad_glDepthFunc;
extern void* glad_glDepthMask;
extern void* glad_glDepthRangef;
extern void* glad_glDisable;
extern void* glad_glDisableClientState;
extern void* glad_glDrawArrays;
extern void* glad_glDrawElements;
extern void* glad_glEnable;
extern void* glad_glEnableClientState;
extern void* glad_glFinish;
extern void* glad_glFlush;
extern void* glad_glFogf;
extern void* glad_glFogfv;
extern void* glad_glFrontFace;
extern void* glad_glGenBuffers;
extern void* glad_glGenTextures;
extern void* glad_glGetBooleanv;
extern void* glad_glGetBufferParameteriv;
extern void* glad_glGetError;
extern void* glad_glGetFloatv;
extern void* glad_glGetIntegerv;
extern void* glad_glGetPointerv;
extern void* glad_glGetString;
extern void* glad_glGetTexEnvfv;
extern void* glad_glGetTexEnviv;
extern void* glad_glGetTexParameterfv;
extern void* glad_glGetTexParameteriv;
extern void* glad_glHint;
extern void* glad_glIsBuffer;
extern void* glad_glIsEnabled;
extern void* glad_glIsTexture;
extern void* glad_glLightModelf;
extern void* glad_glLightModelfv;
extern void* glad_glLightfv;
extern void* glad_glLineWidth;
extern void* glad_glLoadIdentity;
extern void* glad_glLoadMatrixf;
extern void* glad_glMaterialfv;
extern void* glad_glMatrixMode;
extern void* glad_glMultMatrixf;
extern void* glad_glNormal3f;
extern void* glad_glNormalPointer;
extern void* glad_glPixelStorei;
extern void* glad_glPointSize;
extern void* glad_glPolygonOffset;
extern void* glad_glReadPixels;
extern void* glad_glRotatef;
extern void* glad_glSampleCoverage;
extern void* glad_glScalef;
extern void* glad_glScissor;
extern void* glad_glShadeModel;
extern void* glad_glStencilFunc;
extern void* glad_glStencilMask;
extern void* glad_glStencilOp;
extern void* glad_glTexCoordPointer;
extern void* glad_glTexEnvf;
extern void* glad_glTexEnvfv;
extern void* glad_glTexEnvi;
extern void* glad_glTexImage2D;
extern void* glad_glTexParameterf;
extern void* glad_glTexParameterfv;
extern void* glad_glTexParameteri;
extern void* glad_glTexParameteriv;
extern void* glad_glTexSubImage2D;
extern void* glad_glTranslatef;
extern void* glad_glVertexPointer;
extern void* glad_glViewport;
}

// GLES1-only entry points mapped onto the desktop style calls the emulation has
static void web_glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { glOrtho(l, r, b, t, n, f); }
static void web_glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { glFrustum(l, r, b, t, n, f); }
static void web_glFogx(GLenum p, GLint v) { glFogi(p, v); }
static void web_glFogxv(GLenum p, const GLint* v) { GLfloat f[4] = { (GLfloat)v[0], (GLfloat)v[1], (GLfloat)v[2], (GLfloat)v[3] }; glFogfv(p, f); }
extern "C" { extern void* glad_glOrthof; extern void* glad_glFrustumf; extern void* glad_glFogx; extern void* glad_glFogxv; }

void webBindGLFunctions() {
	glad_glOrthof   = (void*)&web_glOrthof;
	glad_glFrustumf = (void*)&web_glFrustumf;
	glad_glFogx     = (void*)&web_glFogx;
	glad_glFogxv    = (void*)&web_glFogxv;
	glad_glActiveTexture = (void*)&glActiveTexture;
	glad_glAlphaFunc = (void*)&glAlphaFunc;
	glad_glBindBuffer = (void*)&glBindBuffer;
	glad_glBindTexture = (void*)&glBindTexture;
	glad_glBlendFunc = (void*)&glBlendFunc;
	glad_glBufferData = (void*)&glBufferData;
	glad_glBufferSubData = (void*)&glBufferSubData;
	glad_glClear = (void*)&glClear;
	glad_glClearColor = (void*)&glClearColor;
	glad_glClearDepthf = (void*)&glClearDepthf;
	glad_glClearStencil = (void*)&glClearStencil;
	glad_glClientActiveTexture = (void*)&glClientActiveTexture;
	glad_glColor4f = (void*)&glColor4f;
	glad_glColor4ub = (void*)&glColor4ub;
	glad_glColorMask = (void*)&glColorMask;
	glad_glColorPointer = (void*)&glColorPointer;
	glad_glCompressedTexImage2D = (void*)&glCompressedTexImage2D;
	glad_glCompressedTexSubImage2D = (void*)&glCompressedTexSubImage2D;
	glad_glCopyTexImage2D = (void*)&glCopyTexImage2D;
	glad_glCopyTexSubImage2D = (void*)&glCopyTexSubImage2D;
	glad_glCullFace = (void*)&glCullFace;
	glad_glDeleteBuffers = (void*)&glDeleteBuffers;
	glad_glDeleteTextures = (void*)&glDeleteTextures;
	glad_glDepthFunc = (void*)&glDepthFunc;
	glad_glDepthMask = (void*)&glDepthMask;
	glad_glDepthRangef = (void*)&glDepthRangef;
	glad_glDisable = (void*)&glDisable;
	glad_glDisableClientState = (void*)&glDisableClientState;
	glad_glDrawArrays = (void*)&glDrawArrays;
	glad_glDrawElements = (void*)&glDrawElements;
	glad_glEnable = (void*)&glEnable;
	glad_glEnableClientState = (void*)&glEnableClientState;
	glad_glFinish = (void*)&glFinish;
	glad_glFlush = (void*)&glFlush;
	glad_glFogf = (void*)&glFogf;
	glad_glFogfv = (void*)&glFogfv;
	glad_glFrontFace = (void*)&glFrontFace;
	glad_glGenBuffers = (void*)&glGenBuffers;
	glad_glGenTextures = (void*)&glGenTextures;
	glad_glGetBooleanv = (void*)&glGetBooleanv;
	glad_glGetBufferParameteriv = (void*)&glGetBufferParameteriv;
	glad_glGetError = (void*)&glGetError;
	glad_glGetFloatv = (void*)&glGetFloatv;
	glad_glGetIntegerv = (void*)&glGetIntegerv;
	glad_glGetPointerv = (void*)&glGetPointerv;
	glad_glGetString = (void*)&glGetString;
	glad_glGetTexEnvfv = (void*)&glGetTexEnvfv;
	glad_glGetTexEnviv = (void*)&glGetTexEnviv;
	glad_glGetTexParameterfv = (void*)&glGetTexParameterfv;
	glad_glGetTexParameteriv = (void*)&glGetTexParameteriv;
	glad_glHint = (void*)&glHint;
	glad_glIsBuffer = (void*)&glIsBuffer;
	glad_glIsEnabled = (void*)&glIsEnabled;
	glad_glIsTexture = (void*)&glIsTexture;
	glad_glLightModelf = (void*)&glLightModelf;
	glad_glLightModelfv = (void*)&glLightModelfv;
	glad_glLightfv = (void*)&glLightfv;
	glad_glLineWidth = (void*)&glLineWidth;
	glad_glLoadIdentity = (void*)&glLoadIdentity;
	glad_glLoadMatrixf = (void*)&glLoadMatrixf;
	glad_glMaterialfv = (void*)&glMaterialfv;
	glad_glMatrixMode = (void*)&glMatrixMode;
	glad_glMultMatrixf = (void*)&glMultMatrixf;
	glad_glNormal3f = (void*)&glNormal3f;
	glad_glNormalPointer = (void*)&glNormalPointer;
	glad_glPixelStorei = (void*)&glPixelStorei;
	glad_glPointSize = (void*)&glPointSize;
	glad_glPolygonOffset = (void*)&glPolygonOffset;
	glad_glReadPixels = (void*)&glReadPixels;
	glad_glRotatef = (void*)&glRotatef;
	glad_glSampleCoverage = (void*)&glSampleCoverage;
	glad_glScalef = (void*)&glScalef;
	glad_glScissor = (void*)&glScissor;
	glad_glShadeModel = (void*)&glShadeModel;
	glad_glStencilFunc = (void*)&glStencilFunc;
	glad_glStencilMask = (void*)&glStencilMask;
	glad_glStencilOp = (void*)&glStencilOp;
	glad_glTexCoordPointer = (void*)&glTexCoordPointer;
	glad_glTexEnvf = (void*)&glTexEnvf;
	glad_glTexEnvfv = (void*)&glTexEnvfv;
	glad_glTexEnvi = (void*)&glTexEnvi;
	glad_glTexImage2D = (void*)&glTexImage2D;
	glad_glTexParameterf = (void*)&glTexParameterf;
	glad_glTexParameterfv = (void*)&glTexParameterfv;
	glad_glTexParameteri = (void*)&glTexParameteri;
	glad_glTexParameteriv = (void*)&glTexParameteriv;
	glad_glTexSubImage2D = (void*)&glTexSubImage2D;
	glad_glTranslatef = (void*)&glTranslatef;
	glad_glVertexPointer = (void*)&glVertexPointer;
	glad_glViewport = (void*)&glViewport;
}
#endif
