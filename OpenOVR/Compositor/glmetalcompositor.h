#pragma once

// macOS-only compositor for OpenGL applications.
//
// There is no OpenGL graphics binding in OpenXR for macOS, and OXRSys (like every macOS runtime)
// only offers Metal (and Vulkan-over-MoltenVK) swapchains. This compositor takes the application's
// GL texture, blits it into an IOSurface-backed GL texture, and then copies that IOSurface (viewed
// as an MTLTexture) into the Metal swapchain image on the session's command queue.
//
// The header is plain C++ so it can be included from the C++ compositor factory; all Objective-C
// lives in glmetalcompositor.mm. Metal objects are stored as retained void* (id<...>).

#include "compositor.h"

#include <array>

class GLMetalCompositor : public Compositor {
public:
	explicit GLMetalCompositor(GLuint initialTexture);
	~GLMetalCompositor() override;

	void InvokeCubemap(const vr::Texture_t* textures) override;

protected:
	void CopyToSwapchain(const vr::Texture_t* texture, const vr::VRTextureBounds_t* bounds, std::optional<XruEye> eye, vr::EVRSubmitFlags submitFlags) override;

private:
	// Number of IOSurfaces rotated through so GL never writes into a surface Metal may still be reading
	static constexpr int kSlotCount = 3;

	struct Slot {
		void* ioSurface = nullptr; // IOSurfaceRef
		void* mtlTexture = nullptr; // id<MTLTexture> view of the IOSurface
		void* commandBuffer = nullptr; // id<MTLCommandBuffer> of the last copy out of this surface
		GLuint glTexture = 0; // GL_TEXTURE_RECTANGLE bound to the IOSurface
	};

	void CheckCreateSwapChain(int width, int height, vr::EColorSpace c_space, int rawGlFormat);
	void CreateSharedSurfaces();
	void DestroySharedSurfaces();
	void ReadSwapchainImages();
	void ReleaseSwapchainImages();

	std::array<Slot, kSlotCount> slots;
	int nextSlot = 0;

	std::vector<void*> images; // id<MTLTexture> swapchain images, retained

	GLuint fboId[2] = { 0, 0 };

	// The Metal pixel format the swapchain was created with, and whether the IOSurface is BGRA (vs RGBA)
	int64_t metalFormat = 0;
	bool surfaceIsBGRA = true;
};
