//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.h: Defines the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#ifndef LIBEGL_SURFACE_H_
#define LIBEGL_SURFACE_H_

#define EGLAPI
#include <EGL/egl.h>

#include "common/angleutils.h"

namespace gl
{
class Texture2D;
}
namespace rx
{
class Renderer;
class SwapChain;
}

namespace egl
{
class Display;

class Surface
{
  public:
    Surface(Display *display, EGLNativeWindowType window);
    Surface(Display *display, HANDLE shareHandle, EGLint width, EGLint height, EGLenum textureFormat, EGLenum textureTarget);
	~Surface();

    bool initialize();
    void release();
    bool resetSwapChain();

    EGLNativeWindowType getWindowHandle();
    bool swap();
    
    virtual EGLint getWidth() const;
    virtual EGLint getHeight() const;

    virtual rx::SwapChain *getSwapChain() const;

    void setSwapInterval(EGLint interval);
    bool checkForOutOfDateSwapChain();   // Returns true if swapchain changed due to resize or interval update

    virtual EGLenum getTextureFormat() const;
    virtual EGLenum getTextureTarget() const;

    virtual void setBoundTexture(gl::Texture2D *texture);
    virtual gl::Texture2D *getBoundTexture() const;

private:
    DISALLOW_COPY_AND_ASSIGN(Surface);

    Display *const mDisplay;
    rx::Renderer *mRenderer;

    HANDLE mShareHandle;
    rx::SwapChain *mSwapChain;

    bool resizeSwapChain(int backbufferWidth, int backbufferHeight);
    bool resetSwapChain(int backbufferWidth, int backbufferHeight);
    bool swapRect(EGLint x, EGLint y, EGLint width, EGLint height);

    const EGLNativeWindowType mWindow;            // Window that the surface is created for.

    EGLint mHeight;                // Height of surface
    EGLint mWidth;                 // Width of surface
    EGLint mPixelAspectRatio;      // Display aspect ratio
    EGLenum mTextureFormat;        // Format of texture: RGB, RGBA, or no texture
    EGLenum mTextureTarget;        // Type of texture: 2D or no texture
    EGLint mSwapInterval;
    
    bool mSwapIntervalDirty;
    gl::Texture2D *mTexture;
};
}

#endif   // LIBEGL_SURFACE_H_
