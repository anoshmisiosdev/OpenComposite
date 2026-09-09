#include "stdafx.h"
#include <numbers>
#define BASE_IMPL

#include "Misc/Config.h"

#include "convert.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>

using glm::mat4;
using glm::quat;
using glm::vec3;
using glm::vec4;

#include "BaseCompositor.h"
#include "BaseOverlay.h"

// For the left and right hand constants - TODO move them to their own file
#include "BaseSystem.h"
#include "generated/static_bases.gen.h"

// FIXME find a nice way to clean this up
#ifdef SUPPORT_VK
#include "../../DrvOpenXR/pub/DrvOpenXR.h"
#endif

#include "BaseClientCore.h"
#include "Drivers/Backend.h"
#include "Misc/ScopeGuard.h"

using namespace vr;
using namespace IVRCompositor_022;

typedef int ovr_enum_t;

#define SESS (*ovr::session)

BaseCompositor::BaseCompositor()
{
}

BaseCompositor::~BaseCompositor()
{
}

void BaseCompositor::SetTrackingSpace(ETrackingUniverseOrigin eOrigin)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::SetTrackingSpace", "TRACE-ENTRY"); }
	OOVR_LOGF("HEIGHTDIAG SetTrackingSpace eOrigin=%d (%s)", (int)eOrigin,
	    eOrigin == TrackingUniverseSeated ? "Seated/LOCAL" : (eOrigin == TrackingUniverseStanding ? "Standing/STAGE" : "Other"));
	XrReferenceSpaceType origin = XR_REFERENCE_SPACE_TYPE_STAGE;
	if (eOrigin == TrackingUniverseSeated) {
		origin = XR_REFERENCE_SPACE_TYPE_LOCAL;
	}

	GetUnsafeBaseSystem()->currentSpace = origin;
}

ETrackingUniverseOrigin BaseCompositor::GetTrackingSpace()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetTrackingSpace", "TRACE-ENTRY"); }
	if (GetUnsafeBaseSystem()->currentSpace == XR_REFERENCE_SPACE_TYPE_LOCAL) {
		return TrackingUniverseSeated;
	} else {
		return TrackingUniverseStanding;
	}
}

ovr_enum_t BaseCompositor::WaitGetPoses(TrackedDevicePose_t* renderPoseArray, uint32_t renderPoseArrayCount,
    TrackedDevicePose_t* gamePoseArray, uint32_t gamePoseArrayCount)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::WaitGetPoses", "TRACE-ENTRY"); }

	// Assume this method isn't being called between frames, b/c it really shouldn't be.
	leftEyeSubmitted = false;
	rightEyeSubmitted = false;

	BackendManager::Instance().WaitForTrackingData();

	return GetLastPoses(renderPoseArray, renderPoseArrayCount, gamePoseArray, gamePoseArrayCount);
}

void BaseCompositor::GetSinglePoseRendering(ETrackingUniverseOrigin origin, TrackedDeviceIndex_t unDeviceIndex, TrackedDevicePose_t* pOutputPose)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetSinglePoseRendering", "TRACE-ENTRY"); }
	BackendManager::Instance().GetSinglePose(origin, unDeviceIndex, pOutputPose, ETrackingStateType::TrackingStateType_Rendering);
}

mat4 BaseCompositor::GetHandTransform()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetHandTransform", "TRACE-ENTRY"); }
	float deg_to_rad = std::numbers::pi / 180;

	// The angle offset between the Touch and Vive controllers.
	// If this is incorrect, virtual hands will feel off.
	float controller_offset_angle = 39.5;

	// When testing to try and find the correct value above, uncomment
	//  this to lock the controller perfectly flat.
	// ovrPose.ThePose.Orientation = { 0,0,0,1 };

	vec3 rotateAxis = vec3(1, 0, 0);
	quat rotation = glm::rotate(controller_offset_angle * deg_to_rad, rotateAxis); // count++ * 0.01f);

	mat4 transform(rotation);

	// Controller offset
	// Note this is about right, found by playing around in Unity until everything
	//  roughly lines up. If you want to contribute better numbers, please go ahead!
	transform[3] = vec4(0.0f, 0.0353f, -0.0451f, 1.0f);

	return transform;
}

mat4 BaseCompositor::GetTrackerTransform()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetTrackerTransform", "TRACE-ENTRY"); }
	// The amount the tracker should be offset
	float tracker_offset = 0.025f;

	mat4 transform(1.0f);

	transform[3] = vec4(0.0f, 0.0f, tracker_offset, 1.0f);

	return transform;
}

ovr_enum_t BaseCompositor::GetLastPoses(TrackedDevicePose_t* renderPoseArray, uint32_t renderPoseArrayCount,
    TrackedDevicePose_t* gamePoseArray, uint32_t gamePoseArrayCount)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetLastPoses", "TRACE-ENTRY"); }

	ETrackingUniverseOrigin origin = GetTrackingSpace();

	for (uint32_t i = 0; i < std::max(gamePoseArrayCount, renderPoseArrayCount); i++) {
		TrackedDevicePose_t* renderPose = NULL;
		TrackedDevicePose_t* gamePose = NULL;

		if (renderPoseArray) {
			renderPose = i < renderPoseArrayCount ? renderPoseArray + i : NULL;
		}

		if (gamePoseArray) {
			gamePose = i < gamePoseArrayCount ? gamePoseArray + i : NULL;
		}

		if (renderPose) {
			GetSinglePoseRendering(origin, i, renderPose);
		}

		if (gamePose) {
			if (renderPose) {
				*gamePose = *renderPose;
			} else {
				GetSinglePoseRendering(origin, i, gamePose);
			}
		}
	}

	return VRCompositorError_None;
}

ovr_enum_t BaseCompositor::GetLastPoseForTrackedDeviceIndex(TrackedDeviceIndex_t unDeviceIndex, TrackedDevicePose_t* pOutputPose,
    TrackedDevicePose_t* pOutputGamePose)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetLastPoseForTrackedDeviceIndex", "TRACE-ENTRY"); }

	if (unDeviceIndex >= k_unMaxTrackedDeviceCount) {
		return VRCompositorError_IndexOutOfRange;
	}

	TrackedDevicePose_t pose{};
	GetSinglePoseRendering(GetTrackingSpace(), unDeviceIndex, &pose);

	if (pOutputPose) {
		*pOutputPose = pose;
	}

	if (pOutputGamePose) {
		*pOutputGamePose = pose;
	}

	return VRCompositorError_None;
}

#if !defined(OC_XR_PORT) && defined(SUPPORT_DX) && defined(SUPPORT_DX11)
DX11Compositor* BaseCompositor::dxcomp;
#endif

std::unique_ptr<Compositor> BaseCompositor::CreateCompositorAPI(const vr::Texture_t* texture)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::CreateCompositorAPI", "TRACE-ENTRY"); }
	std::unique_ptr<Compositor> comp;

	switch (texture->eType) {
#if defined(SUPPORT_GL)
	case TextureType_OpenGL: {
		// Double-cast to avoid a CLion warning
		comp = std::make_unique<GLCompositor>((GLuint)(intptr_t)texture->handle);
		break;
	}
#elif defined(SUPPORT_GLES)
	case TextureType_OpenGL: {
		// Double-cast to avoid a CLion warning
		comp = std::make_unique<GLESCompositor>();
		break;
	}
#endif
#if defined(SUPPORT_DX) && defined(SUPPORT_DX11)
	case TextureType_DirectX: {
		if (!oovr_global_configuration.DX10Mode())
			comp = std::make_unique<DX11Compositor>((ID3D11Texture2D*)texture->handle);

#if defined(SUPPORT_DX10) && !defined(OC_XR_PORT)
		else
			comp = std::make_unique<DX10Compositor>((ID3D10Texture2D*)texture->handle);

		dxcomp = (DX11Compositor*)comp;
#else
		else
			STUBBED();
#endif

		break;
	}
#endif
#ifdef SUPPORT_VK
	case TextureType_Vulkan: {
		comp = std::make_unique<VkCompositor>(texture);
		break;
	}
#endif
#if defined(SUPPORT_DX) && defined(SUPPORT_DX12)
	case TextureType_DirectX12: {
		comp = std::make_unique<DX12Compositor>((D3D12TextureData_t*)texture->handle);
		break;
	}
#endif
	default:
		string err = "[BaseCompositor::Submit] Unsupported texture type: " + to_string(texture->eType);
		OOVR_ABORT(err.c_str());
	}

	return comp;
}

ovr_enum_t BaseCompositor::Submit(EVREye eye, const Texture_t* texture, const VRTextureBounds_t* bounds, EVRSubmitFlags submitFlags)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::Submit", "TRACE-ENTRY"); }
	{
		static thread_local int _n = 0;
		if (_n++ < 40) {
			if (bounds)
				OOVR_LOGF("[STEREO-DBG] Submit eye=%d texType=%d flags=%d bounds uMin=%f uMax=%f vMin=%f vMax=%f",
				    (int)eye, (int)texture->eType, (int)submitFlags,
				    bounds->uMin, bounds->uMax, bounds->vMin, bounds->vMax);
			else
				OOVR_LOGF("[STEREO-DBG] Submit eye=%d texType=%d flags=%d bounds=NULL",
				    (int)eye, (int)texture->eType, (int)submitFlags);
		}
	}
	if (BaseClientCore::appType == vr::VRApplication_Background) {
		OOVR_ABORT("Error - application with type VRApplication_Background should not be submitting!");
	}

	bool isFirstEye = !leftEyeSubmitted && !rightEyeSubmitted;

	bool eyeState = false;
	if (eye == Eye_Left)
		eyeState = leftEyeSubmitted;
	else
		eyeState = rightEyeSubmitted;

	if (eyeState) {
		OOVR_ABORT("Eye already submitted!");
	}

	if (eye == Eye_Left)
		leftEyeSubmitted = true;
	else
		rightEyeSubmitted = true;

	// Handle null textures
	// Rather surprisingly, it's perfectly valid to pass null textures to SteamVR. So far, I've
	// only seen this used in Sparc (see #19). If a game tries to do this, ensure that either neither or
	// both eyes do so, and don't actually send a frame to LibOVR.
	bool textureNull = texture->handle == nullptr;
	if (isFirstEye) {
		isNullRender = textureNull;
	} else if (textureNull != isNullRender) {
		OOVR_ABORT("Cannot mismatch first and second eye renders");
	}

	{
		// Turns out games (Beat Saber in particular) really do not like the session being restarted
		// while they're in the middle of submitting a frame.
		auto lock = xr_session.lock_shared();
		if (!textureNull)
			BackendManager::Instance().StoreEyeTexture(eye, texture, bounds, submitFlags, isFirstEye);

		if (leftEyeSubmitted && rightEyeSubmitted) {
			if (!isNullRender)
				BackendManager::Instance().SubmitFrames(isInSkybox, false);

			leftEyeSubmitted = false;
			rightEyeSubmitted = false;
		}
	}

	return VRCompositorError_None;
}

ovr_enum_t BaseCompositor::SubmitWithArrayIndex(vr::EVREye eye, const vr::Texture_t* texture, uint32_t idx, const vr::VRTextureBounds_t* bounds, vr::EVRSubmitFlags submitFlags)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::SubmitWithArrayIndex", "TRACE-ENTRY"); }
	return Submit(eye, &texture[idx], bounds, submitFlags);
}

void BaseCompositor::ClearLastSubmittedFrame()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ClearLastSubmittedFrame", "TRACE-ENTRY"); }
	// At this point we should show the loading screen and show Guardian, and undo this when the
	// next frame comes along. TODO implement since it would improve loading screens, but it's certainly not critical
}

void BaseCompositor::PostPresentHandoff()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::PostPresentHandoff", "TRACE-ENTRY"); }
	// It appears (from the documentation) that SteamVR will, even after all frames are submitted, not begin
	//  compositing the submitted textures until WaitGetPoses is called. Thus is you want to do some rendering
	//  or game logic or whatever, it will delay the compositor. Calling this tells SteamVR that no further changes
	//  are to be made to the frame, and it can begin the compositor - in the aforementioned cases, this would be
	//  called directly after the last Submit call.
	//
	// Some apps provide a GUI through layers that are submitted after any eye textures are submitted. The app then
	// calls PostPresentHandOff to signal that all data is submitted and compositor can start work. Mimick this by
	// calling SubmitFrames with a flag to say it is called from this function.

	BackendManager::Instance().SubmitFrames(isInSkybox, true);
}

bool BaseCompositor::GetFrameTiming(OOVR_Compositor_FrameTiming* pTiming, uint32_t unFramesAgo)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetFrameTiming", "TRACE-ENTRY"); }
	// "Sets oldest timing info if nFramesAgo is larger than the stored history." So if our history
	// of timing records is only 1 we can just return that.

	return BackendManager::Instance().GetFrameTiming(pTiming, unFramesAgo);

	// TODO fill in the m_nNumVSyncsReadyForUse and uint32_t m_nNumVSyncsToFirstView fields, but only
	// when called from the correct version of the interface.
}

uint32_t BaseCompositor::GetFrameTimings(OOVR_Compositor_FrameTiming* pTiming, uint32_t nFrames)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetFrameTimings", "TRACE-ENTRY"); }
	// This is a request to fill out an array of timing data. However only an arbitrary number
	// of records are available with number being filled returned. In the case of only one record
	// being available we can just send the most recent timing data and return 1.
	bool populated = BackendManager::Instance().GetFrameTiming(pTiming, 1);
	return populated ? 1 : 0;
}

bool BaseCompositor::GetFrameTiming(vr::Compositor_FrameTiming* pTiming, uint32_t unFramesAgo)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetFrameTiming", "TRACE-ENTRY"); }
	return GetFrameTiming((OOVR_Compositor_FrameTiming*)pTiming, unFramesAgo);
}

uint32_t BaseCompositor::GetFrameTimings(vr::Compositor_FrameTiming* pTiming, uint32_t nFrames)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetFrameTimings", "TRACE-ENTRY"); }
	return GetFrameTimings((OOVR_Compositor_FrameTiming*)pTiming, nFrames);
}

float BaseCompositor::GetFrameTimeRemaining()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetFrameTimeRemaining", "TRACE-ENTRY"); }
	STUBBED();
}

void BaseCompositor::GetCumulativeStats(OOVR_Compositor_CumulativeStats* pStats, uint32_t nStatsSizeInBytes)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetCumulativeStats", "TRACE-ENTRY"); }
	STUBBED();
}

void BaseCompositor::FadeToColor(float fSeconds, float fRed, float fGreen, float fBlue, float fAlpha, bool bBackground)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::FadeToColor", "TRACE-ENTRY"); }
	fadeTime = fSeconds;
	fadeColour.r = fRed;
	fadeColour.g = fGreen;
	fadeColour.b = fBlue;
	fadeColour.a = fAlpha;

	// TODO what does background do?
}

HmdColor_t BaseCompositor::GetCurrentFadeColor(bool bBackground)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetCurrentFadeColor", "TRACE-ENTRY"); }
	return fadeColour;
}

void BaseCompositor::FadeGrid(float fSeconds, bool bFadeIn)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::FadeGrid", "TRACE-ENTRY"); }
	// This is the app telling SteamVR to fade from the rendered scene into the skybox, eg before the
	//  app loads a new level (this is how the default SteamVR Unity plugin works).
	//
	// Let's not bother implementing the fade (that would be a LOT of work), and just skip straight over.
	isInSkybox = bFadeIn;

	// TODO suppress input while in this mode
}

float BaseCompositor::GetCurrentGridAlpha()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetCurrentGridAlpha", "TRACE-ENTRY"); }
	return isInSkybox ? 1.0f : 0.0f;
}

ovr_enum_t BaseCompositor::SetSkyboxOverride(const Texture_t* pTextures, uint32_t unTextureCount)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::SetSkyboxOverride", "TRACE-ENTRY"); }
	return BackendManager::Instance().SetSkyboxOverride(pTextures, unTextureCount);
}

void BaseCompositor::ClearSkyboxOverride()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ClearSkyboxOverride", "TRACE-ENTRY"); }
	BackendManager::Instance().ClearSkyboxOverride();
}

void BaseCompositor::CompositorBringToFront()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::CompositorBringToFront", "TRACE-ENTRY"); }
	// No actions required, Oculus runs via direct mode
}

void BaseCompositor::CompositorGoToBack()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::CompositorGoToBack", "TRACE-ENTRY"); }
	STUBBED();
}

void BaseCompositor::CompositorQuit()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::CompositorQuit", "TRACE-ENTRY"); }
	STUBBED();
}

bool BaseCompositor::IsFullscreen()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::IsFullscreen", "TRACE-ENTRY"); }
	STUBBED();
}

uint32_t BaseCompositor::GetCurrentSceneFocusProcess()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetCurrentSceneFocusProcess", "TRACE-ENTRY"); }
	STUBBED();
}

uint32_t BaseCompositor::GetLastFrameRenderer()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetLastFrameRenderer", "TRACE-ENTRY"); }
	STUBBED();
}

bool BaseCompositor::CanRenderScene()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::CanRenderScene", "TRACE-ENTRY"); }
	return true; // TODO implement
}

void BaseCompositor::ShowMirrorWindow()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ShowMirrorWindow", "TRACE-ENTRY"); }
	STUBBED();
}

void BaseCompositor::HideMirrorWindow()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::HideMirrorWindow", "TRACE-ENTRY"); }
	STUBBED();
}

bool BaseCompositor::IsMirrorWindowVisible()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::IsMirrorWindowVisible", "TRACE-ENTRY"); }
	STUBBED();
}

void BaseCompositor::CompositorDumpImages()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::CompositorDumpImages", "TRACE-ENTRY"); }
	STUBBED();
}

bool BaseCompositor::ShouldAppRenderWithLowResources()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ShouldAppRenderWithLowResources", "TRACE-ENTRY"); }
	// TODO put in config file
	return false;
}

void BaseCompositor::ForceInterleavedReprojectionOn(bool bOverride)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ForceInterleavedReprojectionOn", "TRACE-ENTRY"); }
	// Force timewarp on? Yeah right.
}

void BaseCompositor::ForceReconnectProcess()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ForceReconnectProcess", "TRACE-ENTRY"); }
	// We should always be connected
}

void BaseCompositor::SuspendRendering(bool bSuspend)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::SuspendRendering", "TRACE-ENTRY"); }
	// TODO
	// I'm not sure what the purpose of this function is. If you know, please tell me.
	// - ZNix
	// STUBBED();
}

ovr_enum_t BaseCompositor::GetMirrorTextureD3D11(EVREye eEye, void* pD3D11DeviceOrResource, void** ppD3D11ShaderResourceView)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetMirrorTextureD3D11", "TRACE-ENTRY"); }
#if defined(SUPPORT_DX) && defined(SUPPORT_DX11)
	return BackendManager::Instance().GetMirrorTextureD3D11(eEye, pD3D11DeviceOrResource, ppD3D11ShaderResourceView);
#else
	OOVR_ABORT("Cannot get D3D mirror texture - D3D support disabled");
#endif
}

void BaseCompositor::ReleaseMirrorTextureD3D11(void* pD3D11ShaderResourceView)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ReleaseMirrorTextureD3D11", "TRACE-ENTRY"); }
#if defined(SUPPORT_DX) && defined(SUPPORT_DX11)
	return BackendManager::Instance().ReleaseMirrorTextureD3D11(pD3D11ShaderResourceView);
#else
	OOVR_ABORT("Cannot get D3D mirror texture - D3D support disabled");
#endif
}

ovr_enum_t BaseCompositor::GetMirrorTextureGL(EVREye eEye, glUInt_t* pglTextureId, glSharedTextureHandle_t* pglSharedTextureHandle)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetMirrorTextureGL", "TRACE-ENTRY"); }
	STUBBED();
}

bool BaseCompositor::ReleaseSharedGLTexture(glUInt_t glTextureId, glSharedTextureHandle_t glSharedTextureHandle)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ReleaseSharedGLTexture", "TRACE-ENTRY"); }
	STUBBED();
}

void BaseCompositor::LockGLSharedTextureForAccess(glSharedTextureHandle_t glSharedTextureHandle)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::LockGLSharedTextureForAccess", "TRACE-ENTRY"); }
	STUBBED();
}

void BaseCompositor::UnlockGLSharedTextureForAccess(glSharedTextureHandle_t glSharedTextureHandle)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::UnlockGLSharedTextureForAccess", "TRACE-ENTRY"); }
	STUBBED();
}

uint32_t BaseCompositor::GetVulkanInstanceExtensionsRequired(char* pchValue, uint32_t unBufferSize)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetVulkanInstanceExtensionsRequired", "TRACE-ENTRY"); }
#if defined(SUPPORT_VK)
	// Whaddya know, the OpenXR, Oculus and Valve methods work almost identically...
	uint32_t size;
	OOVR_FAILED_XR_ABORT(xr_ext->xrGetVulkanInstanceExtensionsKHR(xr_instance, xr_system, unBufferSize, &size, pchValue));
	return size;
#else
	OOVR_ABORT("Vulkan support disabled");
#endif
}

uint32_t BaseCompositor::GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T* pPhysicalDevice, char* pchValue, uint32_t unBufferSize)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetVulkanDeviceExtensionsRequired", "TRACE-ENTRY"); }
#if defined(SUPPORT_VK)
	uint32_t size;
	OOVR_FAILED_XR_ABORT(xr_ext->xrGetVulkanDeviceExtensionsKHR(xr_instance, xr_system, unBufferSize, &size, pchValue));
	return size;
#else
	OOVR_ABORT("Vulkan support disabled");
#endif
}

void BaseCompositor::SetExplicitTimingMode(ovr_enum_t eTimingMode)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::SetExplicitTimingMode", "TRACE-ENTRY"); }
	// Explicit timing means the application calls SubmitExplicitTimingData each
	// frame, and in return we're not allowed to use their Vulkan queue
	// during WaitGetPoses. We don't do any of that anyway, so nothing needs to
	// be done here.
}

ovr_enum_t BaseCompositor::SubmitExplicitTimingData()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::SubmitExplicitTimingData", "TRACE-ENTRY"); }
	// In SteamVR this records a more accurate timestamp for tracking via the GPU's
	// clock, Oculus doesn't support that so noop here is fine.
	return VRCompositorError_None;
}

bool BaseCompositor::IsMotionSmoothingSupported()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::IsMotionSmoothingSupported", "TRACE-ENTRY"); }
	STUBBED();
}

bool BaseCompositor::IsMotionSmoothingEnabled()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::IsMotionSmoothingEnabled", "TRACE-ENTRY"); }
	STUBBED();
}

bool BaseCompositor::IsCurrentSceneFocusAppLoading()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::IsCurrentSceneFocusAppLoading", "TRACE-ENTRY"); }
	return isInSkybox;
}

ovr_enum_t BaseCompositor::SetStageOverride_Async(const char* pchRenderModelPath, const HmdMatrix34_t* pTransform,
    const OOVR_Compositor_StageRenderSettings* pRenderSettings, uint32_t nSizeOfRenderSettings)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::SetStageOverride_Async", "TRACE-ENTRY"); }
	OOVR_SOFT_ABORT("Stage override not implemented");
	return VRCompositorError_None;
}

void BaseCompositor::ClearStageOverride()
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::ClearStageOverride", "TRACE-ENTRY"); }
	OOVR_SOFT_ABORT("Stage override not implemented");
}

bool BaseCompositor::GetCompositorBenchmarkResults(Compositor_BenchmarkResults* pBenchmarkResults, uint32_t nSizeOfBenchmarkResults)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetCompositorBenchmarkResults", "TRACE-ENTRY"); }
	OOVR_SOFT_ABORT("Compositor benchmarking not implemented");
	return false;
}

ovr_enum_t BaseCompositor::GetLastPosePredictionIDs(uint32_t* pRenderPosePredictionID, uint32_t* pGamePosePredictionID)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetLastPosePredictionIDs", "TRACE-ENTRY"); }
	OOVR_SOFT_ABORT("Pose prediction IDs hardcoded at 0");
	if (pRenderPosePredictionID)
		*pRenderPosePredictionID = 0;
	if (pGamePosePredictionID)
		*pGamePosePredictionID = 0;
	return VRCompositorError_None;
}

ovr_enum_t BaseCompositor::GetPosesForFrame(uint32_t unPosePredictionID, TrackedDevicePose_t* pPoseArray, uint32_t unPoseArrayCount)
{
	{ static thread_local int _n=0; if(_n++<50) oovr_log_raw(__FILE__, __LINE__, "BaseCompositor::GetPosesForFrame", "TRACE-ENTRY"); }
	STUBBED();
}
