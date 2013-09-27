//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include <tchar.h>

#include "libEGL/Surface.h"

#include "common/debug.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/SwapChain.h"
#include "libGLESv2/main.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"

#include "gles2/gl2ext.h"

#ifdef __cplusplus_winrt
extern void get_screen_size(int& width, int& height);
#endif

namespace egl
{
Surface::Surface(Display *display, EGLNativeWindowType window) 
    : mDisplay(display), mWindow(window)
{
    mRenderer = mDisplay->getRenderer();
    mSwapChain = NULL;
    mShareHandle = NULL;
    mTexture = NULL;
    mTextureFormat = EGL_NO_TEXTURE;
    mTextureTarget = EGL_NO_TEXTURE;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mSwapInterval = -1;
    mWidth = -1;
    mHeight = -1;
    setSwapInterval(1);
}

Surface::Surface(Display *display, HANDLE shareHandle, EGLint width, EGLint height, EGLenum textureFormat, EGLenum textureType)
    : mDisplay(display), mWindow(NULL), mShareHandle(shareHandle), mWidth(width), mHeight(height)
{
    mRenderer = mDisplay->getRenderer();
    mSwapChain = NULL;
    mTexture = NULL;
    mTextureFormat = textureFormat;
    mTextureTarget = textureType;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mSwapInterval = -1;
    setSwapInterval(1);
}

Surface::~Surface()
{
    release();
}

bool Surface::initialize()
{
    if (!resetSwapChain())
      return false;

    return true;
}

void Surface::release()
{
    delete mSwapChain;
    mSwapChain = NULL;

    if (mTexture)
    {
        mTexture->releaseTexImage();
        mTexture = NULL;
    }
}

bool Surface::resetSwapChain()
{
    ASSERT(!mSwapChain);

    int width;
    int height;

    if (mWindow != nullptr)
    {
#ifndef __cplusplus_winrt
        RECT windowRect;
        if (!GetClientRect(getWindowHandle(), &windowRect))
        {
            ASSERT(false);

            ERR("Could not retrieve the window dimensions");
            return error(EGL_BAD_SURFACE, false);
        }

        width = windowRect.right - windowRect.left;
        height = windowRect.bottom - windowRect.top;
#else
		get_screen_size(width, height);
#endif
    }
    else
    {
        // non-window surface - size is determined at creation
        width = mWidth;
        height = mHeight;
    }

    mSwapChain = mRenderer->createSwapChain(mWindow, mShareHandle,
                                            GL_RGBA8_OES,
                                            GL_DEPTH24_STENCIL8_OES);
    if (!mSwapChain)
    {
        return error(EGL_BAD_ALLOC, false);
    }

    if (!resetSwapChain(width, height))
    {
        delete mSwapChain;
        mSwapChain = NULL;
        return false;
    }

    return true;
}

bool Surface::resizeSwapChain(int backbufferWidth, int backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->resize(backbufferWidth, backbufferHeight);

    if (status == EGL_CONTEXT_LOST)
    {
        mDisplay->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;

    return true;
}

bool Surface::resetSwapChain(int backbufferWidth, int backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->reset(backbufferWidth, backbufferHeight, mSwapInterval);

    if (status == EGL_CONTEXT_LOST)
    {
        mRenderer->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;
    mSwapIntervalDirty = false;

    return true;
}

bool Surface::swapRect(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mSwapChain)
    {
        return true;
    }

    if (x + width > mWidth)
    {
        width = mWidth - x;
    }

    if (y + height > mHeight)
    {
        height = mHeight - y;
    }

    if (width == 0 || height == 0)
    {
        return true;
    }

    EGLint status = mSwapChain->swapRect(x, y, width, height);

    if (status == EGL_CONTEXT_LOST)
    {
        mRenderer->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    checkForOutOfDateSwapChain();

    return true;
}

EGLNativeWindowType Surface::getWindowHandle()
{
    return mWindow;
}

bool Surface::checkForOutOfDateSwapChain()
{
	int clientWidth, clientHeight;
#ifndef __cplusplus_winrt
    RECT client;
    if (!GetClientRect(getWindowHandle(), &client))
    {
        ASSERT(false);
        return false;
    }
	clientWidth = client.right - client.left;
	clientHeight = client.bottom - client.top;
#else
	get_screen_size(clientWidth, clientHeight);
#endif

    // Grow the buffer now, if the window has grown. We need to grow now to avoid losing information.
    bool sizeDirty = clientWidth != getWidth() || clientHeight != getHeight();

    if (mSwapIntervalDirty)
    {
        resetSwapChain(clientWidth, clientHeight);
    }
    else if (sizeDirty)
    {
        resizeSwapChain(clientWidth, clientHeight);
    }

    if (mSwapIntervalDirty || sizeDirty)
    {
        if (static_cast<egl::Surface*>(getCurrentDrawSurface()) == this)
        {
            glMakeCurrent(glGetCurrentContext(), static_cast<egl::Display*>(getCurrentDisplay()), this);
        }

        return true;
    }

    return false;
}

bool Surface::swap()
{
    return swapRect(0, 0, mWidth, mHeight);
}

EGLint Surface::getWidth() const
{
    return mWidth;
}

EGLint Surface::getHeight() const
{
    return mHeight;
}

rx::SwapChain *Surface::getSwapChain() const
{
    return mSwapChain;
}

void Surface::setSwapInterval(EGLint interval)
{
    if (mSwapInterval == interval)
    {
        return;
    }
    
    mSwapInterval = interval;
    mSwapInterval = std::max(mSwapInterval, mRenderer->getMinSwapInterval());
    mSwapInterval = std::min(mSwapInterval, mRenderer->getMaxSwapInterval());

    mSwapIntervalDirty = true;
}

EGLenum Surface::getTextureFormat() const
{
    return mTextureFormat;
}

EGLenum Surface::getTextureTarget() const
{
    return mTextureTarget;
}

void Surface::setBoundTexture(gl::Texture2D *texture)
{
    mTexture = texture;
}

gl::Texture2D *Surface::getBoundTexture() const
{
    return mTexture;
}

}
