// macOS: a Metal device + command queue used to create the OpenXR session.
// Unlike the other temporary graphics objects this one is kept for the lifetime of the
// session when the app renders with OpenGL, since there is no OpenGL binding on macOS
// and GL frames are copied into Metal swapchains (see GLMetalCompositor).
#pragma once

#include "TemporaryGraphics.h"

#include "../XrDriverPrivate.h"

#include "../../OpenOVR/Misc/xr_metal_compat.h"

class TemporaryMetal : public TemporaryGraphics {
public:
	TemporaryMetal();
	~TemporaryMetal() override;

	const void* GetGraphicsBinding() const override { return &binding; }

	TemporaryMetal* GetAsMetal() override { return this; }

	// id<MTLDevice> / id<MTLCommandQueue>, retained
	void* device = nullptr;
	void* commandQueue = nullptr;

private:
	XrGraphicsBindingMetalKHR binding = {};
};
