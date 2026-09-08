#include "stdafx.h"

#define BASE_IMPL
#include "BaseChaperoneSetup.h"

#include "convert.h"

#include <string>

using namespace vr;

bool BaseChaperoneSetup::CommitWorkingCopy(EChaperoneConfigFile configFile)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::CommitWorkingCopy", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::RevertWorkingCopy()
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::RevertWorkingCopy", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetWorkingPlayAreaSize(float* pSizeX, float* pSizeZ)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetWorkingPlayAreaSize", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetWorkingPlayAreaRect(HmdQuad_t* rect)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetWorkingPlayAreaRect", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetWorkingCollisionBoundsInfo(VR_OUT_ARRAY_COUNT(punQuadsCount) HmdQuad_t* pQuadsBuffer, uint32_t* punQuadsCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetWorkingCollisionBoundsInfo", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetLiveCollisionBoundsInfo(VR_OUT_ARRAY_COUNT(punQuadsCount) HmdQuad_t* pQuadsBuffer, uint32_t* punQuadsCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetLiveCollisionBoundsInfo", "TRACE-ENTRY"); }
	using glm::vec3;

	// TODO better find out what this method does

	XrExtent2Df bounds;
	XrResult result = xrGetReferenceSpaceBoundsRect(xr_session.get(), XR_REFERENCE_SPACE_TYPE_STAGE, &bounds);

	if (result == XR_SPACE_BOUNDS_UNAVAILABLE) {
		if (punQuadsCount)
			*punQuadsCount = 0;
		return false; // TODO verify SteamVR returns this if Guardian isn't set up
	}

	OOVR_FAILED_XR_ABORT(result);

	if (pQuadsBuffer) {
		HmdVector3_t* corners = pQuadsBuffer->vCorners;
		// TODO is this correct? Surely it's offset a bit? What happens when you recentre?
		corners[0] = G2S_v3f(vec3(-bounds.width, 0, -bounds.height));
		corners[1] = G2S_v3f(vec3(-bounds.width, 0, bounds.height));
		corners[2] = G2S_v3f(vec3(bounds.width, 0, bounds.height));
		corners[3] = G2S_v3f(vec3(bounds.width, 0, -bounds.height));
	}

	// string msg = to_string(status) + "," + to_string(pointsCount);
	// OOVR_LOG(msg.c_str());

	if (punQuadsCount)
		*punQuadsCount = 1;

	return true;
}
bool BaseChaperoneSetup::GetWorkingSeatedZeroPoseToRawTrackingPose(HmdMatrix34_t* pmatSeatedZeroPoseToRawTrackingPose)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetWorkingSeatedZeroPoseToRawTrackingPose", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetWorkingStandingZeroPoseToRawTrackingPose(HmdMatrix34_t* pmatStandingZeroPoseToRawTrackingPose)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetWorkingStandingZeroPoseToRawTrackingPose", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::SetWorkingPlayAreaSize(float sizeX, float sizeZ)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::SetWorkingPlayAreaSize", "TRACE-ENTRY"); }
	// Called by VRGIN (a VR mod framework), to hide Chaperone during seated play. Noop here.
}
void BaseChaperoneSetup::SetWorkingCollisionBoundsInfo(VR_ARRAY_COUNT(unQuadsCount) HmdQuad_t* pQuadsBuffer, uint32_t unQuadsCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::SetWorkingCollisionBoundsInfo", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::SetWorkingSeatedZeroPoseToRawTrackingPose(const HmdMatrix34_t* pMatSeatedZeroPoseToRawTrackingPose)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::SetWorkingSeatedZeroPoseToRawTrackingPose", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::SetWorkingStandingZeroPoseToRawTrackingPose(const HmdMatrix34_t* pMatStandingZeroPoseToRawTrackingPose)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::SetWorkingStandingZeroPoseToRawTrackingPose", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::ReloadFromDisk(EChaperoneConfigFile configFile)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::ReloadFromDisk", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetLiveSeatedZeroPoseToRawTrackingPose(HmdMatrix34_t* pmatSeatedZeroPoseToRawTrackingPose)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetLiveSeatedZeroPoseToRawTrackingPose", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::SetWorkingCollisionBoundsTagsInfo(VR_ARRAY_COUNT(unTagCount) uint8_t* pTagsBuffer, uint32_t unTagCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::SetWorkingCollisionBoundsTagsInfo", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetLiveCollisionBoundsTagsInfo(VR_OUT_ARRAY_COUNT(punTagCount) uint8_t* pTagsBuffer, uint32_t* punTagCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetLiveCollisionBoundsTagsInfo", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::SetWorkingPhysicalBoundsInfo(VR_ARRAY_COUNT(unQuadsCount) HmdQuad_t* pQuadsBuffer, uint32_t unQuadsCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::SetWorkingPhysicalBoundsInfo", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::GetLivePhysicalBoundsInfo(VR_OUT_ARRAY_COUNT(punQuadsCount) HmdQuad_t* pQuadsBuffer, uint32_t* punQuadsCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::GetLivePhysicalBoundsInfo", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::ExportLiveToBuffer(VR_OUT_STRING() char* pBuffer, uint32_t* pnBufferLength)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::ExportLiveToBuffer", "TRACE-ENTRY"); }
	STUBBED();
}
bool BaseChaperoneSetup::ImportFromBufferToWorking(const char* pBuffer, uint32_t nImportFlags)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::ImportFromBufferToWorking", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::SetWorkingPerimeter(VR_ARRAY_COUNT(unPointCount) HmdVector2_t* pPointBuffer, uint32_t unPointCount)
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::SetWorkingPerimeter", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::ShowWorkingSetPreview()
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::ShowWorkingSetPreview", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::HideWorkingSetPreview()
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::HideWorkingSetPreview", "TRACE-ENTRY"); }
	STUBBED();
}
void BaseChaperoneSetup::RoomSetupStarting()
{
	{ static thread_local int _n=0; if(_n++<20) oovr_log_raw(__FILE__, __LINE__, "BaseChaperoneSetup::RoomSetupStarting", "TRACE-ENTRY"); }
	STUBBED();
}
