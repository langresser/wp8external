//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// main.cpp: DLL entry point and management of thread-local data.

#include "libEGL/main.h"

#include "common/debug.h"

namespace egl
{
static Current g_current;
void setCurrentError(EGLint error)
{
    Current *current = &g_current;

    current->error = error;
}

EGLint getCurrentError()
{
    Current *current = &g_current;

    return current->error;
}

void setCurrentAPI(EGLenum API)
{
    Current *current = &g_current;

    current->API = API;
}

EGLenum getCurrentAPI()
{
    Current *current = &g_current;

    return current->API;
}

void setCurrentDisplay(EGLDisplay dpy)
{
    Current *current = &g_current;

    current->display = dpy;
}

EGLDisplay getCurrentDisplay()
{
    Current *current = &g_current;

    return current->display;
}

void setCurrentDrawSurface(EGLSurface surface)
{
    Current *current = &g_current;

    current->drawSurface = surface;
}

EGLSurface getCurrentDrawSurface()
{
    Current *current = &g_current;

    return current->drawSurface;
}

void setCurrentReadSurface(EGLSurface surface)
{
    Current *current = &g_current;

    current->readSurface = surface;
}

EGLSurface getCurrentReadSurface()
{
    Current *current = &g_current;

    return current->readSurface;
}

void error(EGLint errorCode)
{
    egl::setCurrentError(errorCode);
}

}
