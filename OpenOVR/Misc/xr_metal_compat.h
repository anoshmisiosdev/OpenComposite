// XR_KHR_metal_enable definitions for OpenXR SDK headers that predate the extension
// (the vendored SDK is 1.0.12). Values match the OpenXR registry (extension 30).
#pragma once

#include <openxr/openxr.h>

#ifndef XR_KHR_metal_enable
#define XR_KHR_metal_enable 1
#define XR_KHR_metal_enable_SPEC_VERSION 1
#define XR_KHR_METAL_ENABLE_EXTENSION_NAME "XR_KHR_metal_enable"

#define XR_TYPE_GRAPHICS_BINDING_METAL_KHR ((XrStructureType)1000029000)
#define XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR ((XrStructureType)1000029001)
#define XR_TYPE_GRAPHICS_REQUIREMENTS_METAL_KHR ((XrStructureType)1000029002)

// XrGraphicsBindingMetalKHR extends XrSessionCreateInfo
typedef struct XrGraphicsBindingMetalKHR {
	XrStructureType type;
	const void* XR_MAY_ALIAS next;
	void* XR_MAY_ALIAS commandQueue; // id<MTLCommandQueue>
} XrGraphicsBindingMetalKHR;

typedef struct XrSwapchainImageMetalKHR {
	XrStructureType type;
	void* XR_MAY_ALIAS next;
	void* XR_MAY_ALIAS texture; // id<MTLTexture>
} XrSwapchainImageMetalKHR;

typedef struct XrGraphicsRequirementsMetalKHR {
	XrStructureType type;
	void* XR_MAY_ALIAS next;
	void* XR_MAY_ALIAS metalDevice; // id<MTLDevice>
} XrGraphicsRequirementsMetalKHR;

typedef XrResult(XRAPI_PTR* PFN_xrGetMetalGraphicsRequirementsKHR)(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsMetalKHR* graphicsRequirements);
#endif
