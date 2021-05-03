/*
 * Mesa 3-D graphics library
 * 
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/* 
 * This is the fake GLX API, for compatibility only.
 */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GL/gl.h>
#include "main/glheader.h"
#include "glapi/glapi.h"

typedef void (*__GLXextFuncPtr)(void);
__GLXextFuncPtr glXGetProcAddressARB (const GLubyte *);
void (*glXGetProcAddress(const GLubyte *procname))( void );

__GLXextFuncPtr
_glxapi_get_proc_address(const char *funcName);

/*
** glXGetProcAddress support
*/

struct name_address_pair
{
   const char *Name;
   GLvoid *Address;
};

static struct name_address_pair GLX_functions[] = {

   /*** GLX_VERSION_1_4 ***/
   { "glXGetProcAddress", (__GLXextFuncPtr) glXGetProcAddress },

   /*** GLX_ARB_get_proc_address ***/
   { "glXGetProcAddressARB", (__GLXextFuncPtr) glXGetProcAddressARB },

   { NULL, NULL }   /* end of list */
};

/*
 * Return address of named glX function, or NULL if not found.
 */
__GLXextFuncPtr
_glxapi_get_proc_address(const char *funcName)
{
   GLuint i;
   for (i = 0; GLX_functions[i].Name; i++) {
      if (strcmp(GLX_functions[i].Name, funcName) == 0) {
         return GLX_functions[i].Address;
      }
   }
   return NULL;
}

/*
 * This function does not get dispatched through the dispatch table
 * since it's really a "meta" function.
 */
__GLXextFuncPtr PUBLIC
glXGetProcAddressARB(const GLubyte *procName)
{
   __GLXextFuncPtr f;

   f = _glxapi_get_proc_address((const char *) procName);
   if (f) {
      return f;
   }

   f = (__GLXextFuncPtr) _glapi_get_proc_address((const char *) procName);

   return f;
}


/* GLX 1.4 */
void PUBLIC
(*glXGetProcAddress(const GLubyte *procName))()
{
   return glXGetProcAddressARB(procName);
}
