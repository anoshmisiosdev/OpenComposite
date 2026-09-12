#ifdef SUPPORT_METAL

#include "TemporaryMetal.h"

#import <Metal/Metal.h>

TemporaryMetal::TemporaryMetal()
{
	// The runtime requires this before xrCreateSession, and hands back the MTLDevice it renders with
	XrGraphicsRequirementsMetalKHR req = { XR_TYPE_GRAPHICS_REQUIREMENTS_METAL_KHR };
	OOVR_FAILED_XR_ABORT(xr_ext->xrGetMetalGraphicsRequirementsKHR(xr_instance, xr_system, &req));

	id<MTLDevice> dev = (__bridge id<MTLDevice>)req.metalDevice;
	if (dev == nil)
		dev = MTLCreateSystemDefaultDevice();
	else
		[dev retain];
	OOVR_FALSE_ABORT(dev != nil);

	id<MTLCommandQueue> queue = [dev newCommandQueue];
	OOVR_FALSE_ABORT(queue != nil);
	queue.label = @"OpenComposite";

	device = (__bridge void*)dev;
	commandQueue = (__bridge void*)queue;

	binding = XrGraphicsBindingMetalKHR{ XR_TYPE_GRAPHICS_BINDING_METAL_KHR };
	binding.commandQueue = commandQueue;

	OOVR_LOGF("Created temporary Metal graphics on device '%s'", [dev.name UTF8String]);
}

TemporaryMetal::~TemporaryMetal()
{
	if (commandQueue)
		[(__bridge id<MTLCommandQueue>)commandQueue release];
	if (device)
		[(__bridge id<MTLDevice>)device release];
	commandQueue = nullptr;
	device = nullptr;
}

#endif
