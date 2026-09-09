#include "stdafx.h"

#include "../Misc/Config.h"
#include "compositor.h"

Compositor::~Compositor()
{
	if (chain) {
		OOVR_FAILED_XR_SOFT_ABORT(xrDestroySwapchain(chain));
		chain = XR_NULL_HANDLE;
	}
}

void Compositor::Invoke(const vr::Texture_t* texture, const vr::VRTextureBounds_t* bounds, XrSwapchainSubImage& subImage, std::optional<XruEye> eye, vr::EVRSubmitFlags submitFlags)
{
	if (bounds && bounds->vMin == 0.0f && bounds->vMax == 1.0f && bounds->uMin == 0.0f && bounds->uMax == 1.0f)
		bounds = nullptr;

	// Always extract the eye's sub-region into a per-eye-sized swapchain, and submit the full
	// swapchain as the sub-image. This is what the shader-invert path already did, and it is the
	// only presentation that is correct on ALL OpenXR runtimes.
	//
	// The previous non-invert path created a full-size (e.g. double-wide) swapchain containing the
	// whole submitted texture and relied on the runtime honouring subImage.imageRect to crop out
	// each eye's half. Compliant runtimes do that, but our target runtime (OXRSys) consumes the
	// full swapchain image and effectively ignores imageRect, so BOTH eyes received the entire
	// side-by-side double-wide texture -- the game appeared as a flat 2D panel with the left and
	// right images sitting next to each other instead of true per-eye stereo.
	//
	// By passing `bounds` into CopyToSwapchain the swapchain is sized to the eye's region and only
	// that region is copied in (CheckCreateSwapChain shrinks width/height by the bounds fraction and
	// CopyToSwapchain selects the correct half). The sub-image is then the full per-eye swapchain,
	// so the result is correct regardless of whether the runtime honours imageRect.
	CopyToSwapchain(texture, bounds, eye, submitFlags);
	subImage.swapchain = GetSwapChain();
	subImage.imageArrayIndex = 0; // This is *not* the swapchain index
	XrExtent2Di src = GetSrcSize();

	// The horizontal (and vertical) *extent* of the eye's region is already baked into the
	// per-eye swapchain by CopyToSwapchain, so the sub-image covers the full swapchain width.
	// However, we must still preserve the source's vertical orientation. Games with a
	// bottom-left texture origin (e.g. Unity/OpenGL, such as SUPERHOT VR) submit bounds with
	// vMin > vMax to request a vertical flip; games with a top-left origin (e.g. UE4/BasaultVR)
	// submit vMin < vMax. If we always emitted an upright (positive-height) imageRect, the
	// flipped-origin games rendered upside-down. So when the game asked for a V-flip, emit an
	// imageRect that spans the swapchain top-to-bottom in reverse (offset.y = height,
	// extent.height = -height), which is how the orientation is signalled to the runtime.
	if (bounds && bounds->vMin > bounds->vMax) {
		// uMin/uMax already applied by the per-eye swapchain crop -> use full width here.
		vr::VRTextureBounds_t rectBounds = { 0.0f, bounds->vMin, 1.0f, bounds->vMax };
		CalculateViewport(&rectBounds, src.width, src.height, true, subImage.imageRect);
	} else {
		CalculateViewport(nullptr, src.width, src.height, true, subImage.imageRect);
	}

	{
		static thread_local int _n = 0;
		if (_n++ < 40) {
			OOVR_LOGF("[STEREO-DBG] Invoke eye=%d src=%dx%d vFlip=%d imageRect off=(%d,%d) ext=%dx%d swapchain=%p",
			    eye.has_value() ? (int)*eye : -1, src.width, src.height,
			    (bounds && bounds->vMin > bounds->vMax) ? 1 : 0,
			    subImage.imageRect.offset.x, subImage.imageRect.offset.y,
			    subImage.imageRect.extent.width, subImage.imageRect.extent.height,
			    (void*)subImage.swapchain);
		}
	}
}

bool Compositor::CalculateViewport(const vr::VRTextureBounds_t* ptrBounds, int32_t width, int32_t height, bool supportsInvert, XrRect2Di& viewport)
{
	bool submitVerticallyFlipped = false;

	if (ptrBounds) {
		vr::VRTextureBounds_t newBounds = *ptrBounds;
		if (!supportsInvert && newBounds.vMin > newBounds.vMax) {
			float newMax = newBounds.vMin;
			newBounds.vMin = newBounds.vMax;
			newBounds.vMax = newMax;
			submitVerticallyFlipped = true;
		} else {
			submitVerticallyFlipped = false;
		}

		viewport.offset.x = (int)(newBounds.uMin * (float)width);
		viewport.offset.y = (int)(newBounds.vMin * (float)height);
		viewport.extent.width = (int)((newBounds.uMax - newBounds.uMin) * (float)width);
		viewport.extent.height = (int)((newBounds.vMax - newBounds.vMin) * (float)height);
	} else {
		viewport.offset.x = viewport.offset.y = 0;
		viewport.extent.width = width;
		viewport.extent.height = height;
		submitVerticallyFlipped = false;
	}
	return submitVerticallyFlipped;
}
