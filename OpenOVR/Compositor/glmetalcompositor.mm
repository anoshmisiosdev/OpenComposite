#include "stdafx.h"

#if defined(SUPPORT_GL) && defined(__APPLE__)

#include "glmetalcompositor.h"

#include "../DrvOpenXR/XrBackend.h"
#include "../DrvOpenXR/tmp_gfx/TemporaryMetal.h"
#include "../Misc/xr_metal_compat.h"

#import <IOSurface/IOSurface.h>
#import <Metal/Metal.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

#include <algorithm>
#include <cinttypes>

// MTLPixelFormat values (as int64 swapchain formats)
static constexpr int64_t MTL_RGBA8_UNORM = 70;
static constexpr int64_t MTL_RGBA8_UNORM_SRGB = 71;
static constexpr int64_t MTL_BGRA8_UNORM = 80;
static constexpr int64_t MTL_BGRA8_UNORM_SRGB = 81;

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

static id<MTLCommandQueue> GetSessionQueue(id<MTLDevice>* outDevice)
{
	auto* backend = (XrBackend*)BackendManager::Instance().GetBackendInstance();
	OOVR_FALSE_ABORT(backend);
	TemporaryMetal* metal = backend->GetTemporaryMetal();
	OOVR_FALSE_ABORT(metal && metal->commandQueue && metal->device);
	if (outDevice)
		*outDevice = (__bridge id<MTLDevice>)metal->device;
	return (__bridge id<MTLCommandQueue>)metal->commandQueue;
}

GLMetalCompositor::GLMetalCompositor(GLuint initialTexture)
{
	OOVR_FALSE_ABORT(CGLGetCurrentContext() != nullptr);
	glGenFramebuffers(2, fboId);
	OOVR_LOG("Created GL->Metal (IOSurface) compositor");
}

GLMetalCompositor::~GLMetalCompositor()
{
	DestroySharedSurfaces();
	ReleaseSwapchainImages();
	// GL names can only be deleted with a current context; leak them otherwise
	if (CGLGetCurrentContext() != nullptr && (fboId[0] || fboId[1])) {
		glDeleteFramebuffers(2, fboId);
	}
}

void GLMetalCompositor::InvokeCubemap(const vr::Texture_t* textures)
{
	OOVR_ABORT("GLMetalCompositor::InvokeCubemap: Not yet supported!");
}

void GLMetalCompositor::ReleaseSwapchainImages()
{
	for (void* img : images)
		[(__bridge id<MTLTexture>)img release];
	images.clear();
}

void GLMetalCompositor::ReadSwapchainImages()
{
	ReleaseSwapchainImages();

	uint32_t imageCount;
	OOVR_FAILED_XR_ABORT(xrEnumerateSwapchainImages(chain, 0, &imageCount, nullptr));
	std::vector<XrSwapchainImageMetalKHR> handles(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR });
	OOVR_FAILED_XR_ABORT(xrEnumerateSwapchainImages(chain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)handles.data()));

	for (const XrSwapchainImageMetalKHR& img : handles) {
		id<MTLTexture> tex = (__bridge id<MTLTexture>)img.texture;
		OOVR_FALSE_ABORT(tex != nil);
		images.push_back((__bridge void*)[tex retain]);
	}
}

void GLMetalCompositor::DestroySharedSurfaces()
{
	bool haveGl = CGLGetCurrentContext() != nullptr;
	for (Slot& slot : slots) {
		if (slot.commandBuffer) {
			id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)slot.commandBuffer;
			if (cb.status != MTLCommandBufferStatusCompleted && cb.status != MTLCommandBufferStatusError)
				[cb waitUntilCompleted];
			[cb release];
			slot.commandBuffer = nullptr;
		}
		if (slot.mtlTexture) {
			[(__bridge id<MTLTexture>)slot.mtlTexture release];
			slot.mtlTexture = nullptr;
		}
		if (slot.glTexture) {
			if (haveGl)
				glDeleteTextures(1, &slot.glTexture);
			slot.glTexture = 0;
		}
		if (slot.ioSurface) {
			CFRelease((IOSurfaceRef)slot.ioSurface);
			slot.ioSurface = nullptr;
		}
	}
}

void GLMetalCompositor::CreateSharedSurfaces()
{
	DestroySharedSurfaces();

	id<MTLDevice> device = nil;
	GetSessionQueue(&device);

	const int width = (int)createInfo.width;
	const int height = (int)createInfo.height;
	const uint32_t pixelFormat = surfaceIsBGRA ? 'BGRA' : 'RGBA';

	CGLContextObj ctx = CGLGetCurrentContext();
	OOVR_FALSE_ABORT(ctx != nullptr);

	for (Slot& slot : slots) {
		// IOSurface
		NSDictionary* props = @{
			(id)kIOSurfaceWidth : @(width),
			(id)kIOSurfaceHeight : @(height),
			(id)kIOSurfaceBytesPerElement : @(4),
			(id)kIOSurfacePixelFormat : @(pixelFormat),
		};
		IOSurfaceRef surface = IOSurfaceCreate((__bridge CFDictionaryRef)props);
		OOVR_FALSE_ABORT(surface != nullptr);
		slot.ioSurface = surface;

		// GL side: a rectangle texture aliasing the surface. Keep the GL view linear (GL_RGBA8) so the
		// blit is a raw byte copy; sRGB-ness is expressed on the Metal side via the pixel format.
		glGenTextures(1, &slot.glTexture);
		glBindTexture(GL_TEXTURE_RECTANGLE, slot.glTexture);
		CGLError err = CGLTexImageIOSurface2D(ctx, GL_TEXTURE_RECTANGLE, GL_RGBA8, width, height,
		    surfaceIsBGRA ? GL_BGRA : GL_RGBA,
		    surfaceIsBGRA ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_BYTE,
		    surface, 0);
		glBindTexture(GL_TEXTURE_RECTANGLE, 0);
		if (err != kCGLNoError)
			OOVR_ABORTF("CGLTexImageIOSurface2D failed: %d (%s)", (int)err, CGLErrorString(err));

		// Metal side: a texture view of the same surface, in the swapchain's pixel format
		MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(MTLPixelFormat)metalFormat
		                                                                               width:width
		                                                                              height:height
		                                                                           mipmapped:NO];
		desc.usage = MTLTextureUsageShaderRead;
		id<MTLTexture> tex = [device newTextureWithDescriptor:desc iosurface:surface plane:0];
		OOVR_FALSE_ABORT(tex != nil);
		slot.mtlTexture = (__bridge void*)tex;
	}
	nextSlot = 0;
}

void GLMetalCompositor::CheckCreateSwapChain(int width, int height, vr::EColorSpace c_space, int rawGlFormat)
{
	// Gamma-space submissions (and explicitly sRGB GL textures) need an sRGB swapchain so the
	// runtime decodes the bytes correctly; everything else is treated as linear 8-bit.
	bool srgb = (c_space == vr::ColorSpace_Gamma) || rawGlFormat == GL_SRGB8_ALPHA8;

	XrSwapchainCreateInfo desc = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
	desc.faceCount = 1;
	desc.width = width;
	desc.height = height;
	desc.format = srgb ? MTL_BGRA8_UNORM_SRGB : MTL_BGRA8_UNORM;
	desc.mipCount = 1;
	desc.sampleCount = 1;
	desc.arraySize = 1;
	desc.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

	// If we previously had to fall back to another format, keep using it so we don't recreate every frame
	if (createInfo.format != createInfoFormat)
		desc.format = createInfo.format;

	if (memcmp(&desc, &createInfo, sizeof(desc)) == 0)
		return;

	desc.format = srgb ? MTL_BGRA8_UNORM_SRGB : MTL_BGRA8_UNORM;
	createInfoFormat = desc.format;

	// Pick a format the runtime actually supports
	uint32_t formatCount;
	OOVR_FAILED_XR_ABORT(xrEnumerateSwapchainFormats(xr_session.get(), 0, &formatCount, nullptr));
	std::vector<int64_t> formats(formatCount);
	OOVR_FAILED_XR_ABORT(xrEnumerateSwapchainFormats(xr_session.get(), formatCount, &formatCount, formats.data()));

	auto has = [&](int64_t f) { return std::count(formats.begin(), formats.end(), f) != 0; };
	surfaceIsBGRA = true;
	if (!has(desc.format)) {
		int64_t rgba = srgb ? MTL_RGBA8_UNORM_SRGB : MTL_RGBA8_UNORM;
		OOVR_LOGF("Metal swapchain format %" PRIi64 " unsupported, trying RGBA", desc.format);
		for (int64_t f : formats)
			OOVR_LOGF("Valid format: %" PRIi64, f);
		if (has(rgba)) {
			desc.format = rgba;
			surfaceIsBGRA = false;
		} else if (has(MTL_BGRA8_UNORM)) {
			desc.format = MTL_BGRA8_UNORM;
		} else if (has(MTL_RGBA8_UNORM)) {
			desc.format = MTL_RGBA8_UNORM;
			surfaceIsBGRA = false;
		} else {
			OOVR_ABORT("No usable 8-bit Metal swapchain format");
		}
	}

	OOVR_LOGF("Creating new Metal swapchain for GL app: %dx%d with format %" PRIi64 " (gl internal format 0x%x, %s)",
	    width, height, desc.format, rawGlFormat, srgb ? "sRGB" : "linear");

	if (chain) {
		OOVR_FAILED_XR_ABORT(xrDestroySwapchain(chain));
		chain = XR_NULL_HANDLE;
	}

	createInfo = desc;
	metalFormat = desc.format;

	OOVR_FAILED_XR_ABORT(xrCreateSwapchain(xr_session.get(), &desc, &chain));

	ReadSwapchainImages();
	CreateSharedSurfaces();
}

void GLMetalCompositor::CopyToSwapchain(const vr::Texture_t* texture, const vr::VRTextureBounds_t* bounds, std::optional<XruEye>, vr::EVRSubmitFlags)
{
	if (CGLGetCurrentContext() == nullptr)
		OOVR_ABORT("GLMetalCompositor: no current OpenGL context on the submitting thread");

	// Clear any pre-existing OpenGL errors
	while (glGetError() != GL_NO_ERROR) {
	}

	auto src = (GLuint)(intptr_t)texture->handle;

	GLint inputWidth = 0, inputHeight = 0, rawFormat = 0;
	GLint prevTex2D = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex2D);
	glBindTexture(GL_TEXTURE_2D, src);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &inputWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &inputHeight);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &rawFormat);
	glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex2D);

	if (inputWidth <= 0 || inputHeight <= 0)
		OOVR_ABORTF("GLMetalCompositor: bad source texture %u (%dx%d)", src, inputWidth, inputHeight);

	XrRect2Di viewport;
	bool submitVerticallyFlipped = CalculateViewport(bounds, inputWidth, inputHeight, false, viewport);

	CheckCreateSwapChain(viewport.extent.width, viewport.extent.height, texture->eColorSpace, rawFormat);

	// Pick the next shared surface, making sure Metal has finished reading it
	Slot& slot = slots[nextSlot];
	nextSlot = (nextSlot + 1) % kSlotCount;
	if (slot.commandBuffer) {
		id<MTLCommandBuffer> prev = (__bridge id<MTLCommandBuffer>)slot.commandBuffer;
		if (prev.status != MTLCommandBufferStatusCompleted && prev.status != MTLCommandBufferStatusError)
			[prev waitUntilCompleted];
		[prev release];
		slot.commandBuffer = nullptr;
	}

	// Acquire a swapchain image
	XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
	uint32_t currentIndex = 0;
	OOVR_FAILED_XR_ABORT(xrAcquireSwapchainImage(chain, &acquireInfo, &currentIndex));

	XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	waitInfo.timeout = XR_INFINITE_DURATION;
	XrResult res;
	do {
		OOVR_FAILED_XR_ABORT(res = xrWaitSwapchainImage(chain, &waitInfo));
	} while (res == XR_TIMEOUT_EXPIRED);

	// GL: blit the eye region of the app's texture into the IOSurface-backed rectangle texture.
	// OpenGL textures have a bottom-left origin while Metal/IOSurface rows start at the top, so
	// flip vertically unless the app already submitted a flipped image (vMin > vMax bounds).
	GLint prevDrawFbo = 0, prevReadFbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
	GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	GLboolean srgbWasEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);
	if (scissorWasEnabled)
		glDisable(GL_SCISSOR_TEST);
	if (srgbWasEnabled)
		glDisable(GL_FRAMEBUFFER_SRGB);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId[1]);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, slot.glTexture, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId[0]);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src, 0);

	const int w = (int)createInfo.width;
	const int h = (int)createInfo.height;
	const bool flip = !submitVerticallyFlipped;
	glBlitFramebuffer(
	    viewport.offset.x, viewport.offset.y, viewport.offset.x + w, viewport.offset.y + h,
	    0, flip ? h : 0, w, flip ? 0 : h,
	    GL_COLOR_BUFFER_BIT, GL_NEAREST);

	// Detach so the app's texture isn't left referenced by our FBOs
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, 0, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDrawFbo);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevReadFbo);
	if (scissorWasEnabled)
		glEnable(GL_SCISSOR_TEST);
	if (srgbWasEnabled)
		glEnable(GL_FRAMEBUFFER_SRGB);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR)
		OOVR_LOG_ONCE("WARNING: OpenGL blit into IOSurface failed!");

	// Make sure the GL work has landed in the IOSurface before Metal reads it. There is no
	// cross-API fence on macOS, so wait for the GL commands to complete.
	GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	if (fence) {
		glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull /* 1s */);
		glDeleteSync(fence);
	} else {
		glFinish();
	}

	// Metal: copy the IOSurface texture into the swapchain image on the session's queue. The runtime
	// snapshots released images on this same queue, so ordering is preserved without extra sync.
	id<MTLCommandQueue> queue = GetSessionQueue(nullptr);
	id<MTLTexture> dst = (__bridge id<MTLTexture>)images.at(currentIndex);
	id<MTLTexture> srcTex = (__bridge id<MTLTexture>)slot.mtlTexture;
	id<MTLCommandBuffer> cb = [queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
	[blit copyFromTexture:srcTex
	          sourceSlice:0
	          sourceLevel:0
	         sourceOrigin:MTLOriginMake(0, 0, 0)
	           sourceSize:MTLSizeMake(w, h, 1)
	            toTexture:dst
	     destinationSlice:0
	     destinationLevel:0
	    destinationOrigin:MTLOriginMake(0, 0, 0)];
	[blit endEncoding];
	[cb commit];
	slot.commandBuffer = (__bridge void*)[cb retain];

	XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
	OOVR_FAILED_XR_ABORT(xrReleaseSwapchainImage(chain, &releaseInfo));
}

#endif
