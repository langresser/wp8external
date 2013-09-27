#include "precompiled.h"
//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Renderer.cpp: Implements EGL dependencies for creating and destroying Renderer instances.

#include <EGL/eglext.h>
#include "libGLESv2/main.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/Renderer11.h"
#include "libGLESv2/utilities.h"
#include "third_party/trace_event/trace_event.h"

#if !defined(ANGLE_ENABLE_D3D11)
// Enables use of the Direct3D 11 API for a default display, when available
#define ANGLE_ENABLE_D3D11 1
#endif

namespace rx
{

Renderer::Renderer(egl::Display *display) : mDisplay(display)
{
}

Renderer::~Renderer()
{
}
}

extern "C"
{

rx::Renderer *glCreateRenderer(egl::Display *display, HDC hDc)
{
    rx::Renderer *renderer = NULL;
    EGLint status = EGL_BAD_ALLOC;
    
	renderer = new rx::Renderer11(display);

	if (renderer) {
		status = renderer->initialize();
	}

	if (status == EGL_SUCCESS) {
		return renderer;
	}

	// Failed to create a D3D11 renderer
	delete renderer;
    return NULL;
}

void glDestroyRenderer(rx::Renderer *renderer)
{
    delete renderer;
}

}