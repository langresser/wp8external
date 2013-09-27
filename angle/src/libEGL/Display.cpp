//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Display.cpp: Implements the egl::Display class, representing the abstract
// display on which graphics are drawn. Implements EGLDisplay.
// [EGL 1.4] section 2.1.2 page 3.

#include "precompiled.h"

#include "libEGL/Display.h"

#include <algorithm>
#include <map>
#include <vector>

#include "common/debug.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/main.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/SwapChain.h"

#include "libEGL/main.h"
#include "libEGL/Surface.h"

namespace egl
{
namespace
{
	Display* g_cur_display = nullptr;
}

egl::Display *Display::getDisplay()
{
    if (g_cur_display)
    {
		return g_cur_display;
    }
    
    g_cur_display = new egl::Display();
    return g_cur_display;
}

Display::Display()
{
    mRenderer = NULL;
}

Display::~Display()
{
    terminate();
    g_cur_display = nullptr;
}

bool Display::initialize()
{
    if (isInitialized())
    {
        return true;
    }

    mRenderer = glCreateRenderer(this, 0);
    
    if (!mRenderer)
    {
        terminate();
        return error(EGL_NOT_INITIALIZED, false);
    }

    EGLint minSwapInterval = mRenderer->getMinSwapInterval();
    EGLint maxSwapInterval = mRenderer->getMaxSwapInterval();
    EGLint maxTextureWidth = mRenderer->getMaxTextureWidth();
    EGLint maxTextureHeight = mRenderer->getMaxTextureHeight();

    if (!isInitialized())
    {
        terminate();
        return false;
    }

    initVendorString();

    return true;
}

void Display::terminate()
{
    while (!mSurfaceSet.empty())
    {
        destroySurface(*mSurfaceSet.begin());
    }

    while (!mContextSet.empty())
    {
        destroyContext(*mContextSet.begin());
    }

    glDestroyRenderer(mRenderer);
    mRenderer = NULL;
}

EGLSurface Display::createWindowSurface(EGLNativeWindowType window, const EGLint *attribList)
{
    if (attribList)
    {
        while (*attribList != EGL_NONE)
        {
            switch (attribList[0])
            {
              case EGL_RENDER_BUFFER:
                switch (attribList[1])
                {
                  case EGL_BACK_BUFFER:
                    break;
                  case EGL_SINGLE_BUFFER:
                    return error(EGL_BAD_MATCH, EGL_NO_SURFACE);   // Rendering directly to front buffer not supported
                  default:
                    return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                }
                break;
              case EGL_VG_COLORSPACE:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              case EGL_VG_ALPHA_FORMAT:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              default:
                return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
            }

            attribList += 2;
        }
    }

    if (hasExistingWindowSurface(window))
    {
        return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    if (mRenderer->testDeviceLost(false))
    {
        if (!restoreLostDevice())
            return EGL_NO_SURFACE;
    }

    Surface *surface = new Surface(this, window);

    if (!surface->initialize())
    {
        delete surface;
        return EGL_NO_SURFACE;
    }

    mSurfaceSet.insert(surface);

    return success(surface);
}

EGLSurface Display::createOffscreenSurface(HANDLE shareHandle, const EGLint *attribList)
{
    EGLint width = 0, height = 0;
    EGLenum textureFormat = EGL_NO_TEXTURE;
    EGLenum textureTarget = EGL_NO_TEXTURE;

    if (attribList)
    {
        while (*attribList != EGL_NONE)
        {
            switch (attribList[0])
            {
              case EGL_WIDTH:
                width = attribList[1];
                break;
              case EGL_HEIGHT:
                height = attribList[1];
                break;
              case EGL_LARGEST_PBUFFER:
                if (attribList[1] != EGL_FALSE)
                  UNIMPLEMENTED(); // FIXME
                break;
              case EGL_TEXTURE_FORMAT:
                switch (attribList[1])
                {
                  case EGL_NO_TEXTURE:
                  case EGL_TEXTURE_RGB:
                  case EGL_TEXTURE_RGBA:
                    textureFormat = attribList[1];
                    break;
                  default:
                    return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                }
                break;
              case EGL_TEXTURE_TARGET:
                switch (attribList[1])
                {
                  case EGL_NO_TEXTURE:
                  case EGL_TEXTURE_2D:
                    textureTarget = attribList[1];
                    break;
                  default:
                    return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                }
                break;
              case EGL_MIPMAP_TEXTURE:
                if (attribList[1] != EGL_FALSE)
                  return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
                break;
              case EGL_VG_COLORSPACE:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              case EGL_VG_ALPHA_FORMAT:
                return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
              default:
                return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
            }

            attribList += 2;
        }
    }

    if (width < 0 || height < 0)
    {
        return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
    }

    if (width == 0 || height == 0)
    {
        return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
    }

    if (textureFormat != EGL_NO_TEXTURE && !mRenderer->getNonPower2TextureSupport() && (!gl::isPow2(width) || !gl::isPow2(height)))
    {
        return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
    }

    if ((textureFormat != EGL_NO_TEXTURE && textureTarget == EGL_NO_TEXTURE) ||
        (textureFormat == EGL_NO_TEXTURE && textureTarget != EGL_NO_TEXTURE))
    {
        return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
    }

    if (mRenderer->testDeviceLost(false))
    {
        if (!restoreLostDevice())
            return EGL_NO_SURFACE;
    }

    Surface *surface = new Surface(this, shareHandle, width, height, textureFormat, textureTarget);

    if (!surface->initialize())
    {
        delete surface;
        return EGL_NO_SURFACE;
    }

    mSurfaceSet.insert(surface);

    return success(surface);
}

EGLContext Display::createContext(const gl::Context *shareContext, bool notifyResets, bool robustAccess)
{
    if (!mRenderer)
    {
        return NULL;
    }
    else if (mRenderer->testDeviceLost(false))   // Lost device
    {
        if (!restoreLostDevice())
            return NULL;
    }

    gl::Context *context = glCreateContext(shareContext, mRenderer, notifyResets, robustAccess);
    mContextSet.insert(context);

    return context;
}

bool Display::restoreLostDevice()
{
    for (ContextSet::iterator ctx = mContextSet.begin(); ctx != mContextSet.end(); ctx++)
    {
        if ((*ctx)->isResetNotificationEnabled())
            return false;   // If reset notifications have been requested, application must delete all contexts first
    }
 
    // Release surface resources to make the Reset() succeed
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        (*surface)->release();
    }

    if (!mRenderer->resetDevice())
    {
        return error(EGL_BAD_ALLOC, false);
    }

    // Restore any surfaces that may have been lost
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        (*surface)->resetSwapChain();
    }

    return true;
}


void Display::destroySurface(egl::Surface *surface)
{
    delete surface;
    mSurfaceSet.erase(surface);
}

void Display::destroyContext(gl::Context *context)
{
    glDestroyContext(context);
    mContextSet.erase(context);
}

void Display::notifyDeviceLost()
{
    for (ContextSet::iterator context = mContextSet.begin(); context != mContextSet.end(); context++)
    {
        (*context)->markContextLost();
    }
    egl::error(EGL_CONTEXT_LOST);
}

void Display::recreateSwapChains()
{
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        (*surface)->getSwapChain()->recreate();
    }
}

bool Display::isInitialized() const
{
	return mRenderer != nullptr;
}

bool Display::isValidContext(gl::Context *context)
{
    return mContextSet.find(context) != mContextSet.end();
}

bool Display::isValidSurface(egl::Surface *surface)
{
    return mSurfaceSet.find(surface) != mSurfaceSet.end();
}

bool Display::hasExistingWindowSurface(EGLNativeWindowType window)
{
    for (SurfaceSet::iterator surface = mSurfaceSet.begin(); surface != mSurfaceSet.end(); surface++)
    {
        if ((*surface)->getWindowHandle() == window)
        {
            return true;
        }
    }

    return false;
}

void Display::initVendorString()
{
    mVendorString = "Google Inc.";

    LUID adapterLuid = {0};

    if (mRenderer && mRenderer->getLUID(&adapterLuid))
    {
        char adapterLuidString[64];
        sprintf_s(adapterLuidString, sizeof(adapterLuidString), " (adapter LUID: %08x%08x)", adapterLuid.HighPart, adapterLuid.LowPart);

        mVendorString += adapterLuidString;
    }
}

const char *Display::getVendorString() const
{
    return mVendorString.c_str();
}

}
