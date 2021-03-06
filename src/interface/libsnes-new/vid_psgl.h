// OpenGL interface
#ifndef _VID_PSGL_H_
#define _VID_PSGL_H_

#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <cell/dbgfont.h>

extern void psglSetVSync(uint32_t enable);
extern void psglInitGL(void);
extern void psglInitGL_with_resolution(uint32_t resolutionId);
extern void dbgFontInit(void);
extern void psglResolutionPrevious(void);
extern void psglResolutionNext(void);
extern int32_t psglInitShader(const char* filename);
extern uint32_t psglGetCurrentResolutionId(void);
extern void psglResolutionSwitch(void);
extern void psglRenderStretch(void);
extern void psglRenderPaused(void);			 
extern void psglRenderAlpha(void);
extern void CalculateViewports(void);
extern void _psglRender(void);

#define psglClearUI() glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

#define psglRenderUI() \
	cellSysutilCheckCallback(); \
	psglSwap();

#define resize(width, height) \
   if (!(iwidth >= width && iheight >= height)) \
   { \
      iwidth  = max(width,  iwidth); \
      iheight = max(height, iheight); \
      if (buffer) { delete[] buffer; buffer = NULL; } \
      buffer = new unsigned int[iwidth * iheight]; \
      memset(buffer, 0, iwidth * iheight * sizeof(unsigned int)); \
      glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE, iwidth * iheight * 4, buffer, GL_SYSTEM_DRAW_SCE); \
   }

#define lock(data, pitch) \
   pitch = iwidth * sizeof(unsigned int); \
   data = buffer;

#define set_cg_params() \
   cgGLSetParameter2f(cg_video_size, iwidth, iheight); \
   cgGLSetParameter2f(cg_texture_size, iwidth, iheight); \
   cgGLSetParameter2f(cg_output_size, cg_viewport_width, cg_viewport_height); \
   cgGLSetParameter2f(cg_v_video_size, iwidth, iheight); \
   cgGLSetParameter2f(cg_v_texture_size, iwidth, iheight); \
   cgGLSetParameter2f(cg_v_output_size, cg_viewport_width, cg_viewport_height); \
   cgGLSetParameter1f(cgp_timer, frame_count); \
   cgGLSetParameter1f(cgp_vertex_timer, frame_count);

#define refresh(inwidth, inheight) \
   frame_count += 1; \
   glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0, (iwidth * iheight) << 2, buffer); \
   glTextureReferenceSCE(GL_TEXTURE_2D, 1, iwidth, iheight, 0, GL_ARGB_SCE, iwidth << 2, 0); \
   set_cg_params(); \
   glDrawArrays(GL_QUADS, 0, 4); \
   glFlush();

#define refreshwithalpha(inwidth, inheight, alpha) \
   frame_count += 1; \
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE, (iwidth * iheight) << 2, buffer, GL_SYSTEM_DRAW_SCE); \
   uint32_t* texture = (uint32_t*)glMapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_WRITE_ONLY); \
   for(int i = 0; i != iheight; i ++) \
   { \
      for(int j = 0; j != iwidth; j ++) \
      { \
         unsigned char r = (buffer[(i) * iwidth + (j)] >> 16); \
         unsigned char g = (buffer[(i) * iwidth + (j)] >> 8 ); \
         unsigned char b = (buffer[(i) * iwidth + (j)] & 0xFF); \
         r/=2; \
         g/=2; \
         b/=2; \
         uint32_t pix = (r << 16) | (g << 8 )|  b | (0xA0 << 24); \
         texture[i * iwidth + j] = pix; \
      } \
   } \
   glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE); \
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iwidth, iheight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0); \
   set_cg_params(); \
   glDrawArrays(GL_QUADS, 0, 4);	\
   glFlush();

#define setview(x, y, w, h, outwidth, outheight) \
   float device_aspect = psglGetDeviceAspectRatio(psgl_device); \
   glMatrixMode(GL_PROJECTION); \
   glLoadIdentity(); \
   GLfloat left = 0; \
   GLfloat right = outwidth; \
   GLfloat bottom = 0; \
   GLfloat top = outheight; \
   GLfloat zNear = -1.0; \
   GLfloat zFar = 1.0; \
   float desired_aspect = vidScrnAspect; \
   GLuint lowerleft_x, lowerleft_y, viewport_width, viewport_height;  \
   GLuint real_width = w, real_height = h; \
   if(custom_aspect_ratio_mode) \
   { \
      lowerleft_x = x + nXOffset; \
      lowerleft_y = y + nYOffset; \
      real_width = w + nXScale; \
      real_height = h + nYScale; \
      left = x + nXOffset; \
      right = y + nYOffset; \
      bottom = nVidScrnHeight + nYOffset; \
   } \
   else if ( (int)(device_aspect*1000) > (int)(desired_aspect*1000) ) \
   { \
      float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5; \
      lowerleft_x = w * (0.5 - delta); \
      lowerleft_y = 0; \
      real_width = (2.0 * w * delta); \
      real_height = h; \
   } \
   else if ( (int)(device_aspect*1000) < (int)(desired_aspect*1000) ) \
   { \
      float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5; \
      lowerleft_x = 0; \
      lowerleft_y = h * (0.5 - delta); \
      real_width = w; \
      real_height = (2.0 * h * delta); \
   } \
   else  \
   { \
      lowerleft_x = 0; \
      lowerleft_y = 0; \
      real_width = w; \
      real_height = h; \
   } \
   glViewport(lowerleft_x, lowerleft_y, real_width, real_height); \
   cg_viewport_width = real_width; \
   cg_viewport_height = real_height; \
   cgGLSetStateMatrixParameter(ModelViewProj_cgParam, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY); \
   glOrthof(left, right, bottom, top, zNear, zFar); \
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f); \
   glClear(GL_COLOR_BUFFER_BIT); \
   glMatrixMode(GL_MODELVIEW); \
   glLoadIdentity();

#define init() \
   /*enable useful and required features */ \
   glDisable(GL_LIGHTING); \
   glEnable(GL_TEXTURE_2D); \
   glGenBuffers(1, &bufferID); \
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, bufferID);

#define term() \
   if (buffer) \
   { \
      delete[] buffer; \
      buffer = 0; \
      iwidth = 0; \
      iheight = 0; \		
   } \
   glDeleteBuffers(1, &bufferID); 

#endif
