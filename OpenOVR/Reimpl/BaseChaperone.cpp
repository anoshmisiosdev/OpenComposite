#include "stdafx.h"
#define BASE_IMPL
#include "BaseChaperone.h"
#include "BaseSystem.h"
#include "generated/static_bases.gen.h"

#include "Drivers/Backend.h"

#include <vector>

using namespace vr;

BaseChaperone::BaseChaperoneCalibrationState BaseChaperone::GetCalibrationState()
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::GetCalibrationState", _b); } }
	return ChaperoneCalibrationState_OK;
}
bool BaseChaperone::GetPlayAreaSize(float* pSizeX, float* pSizeZ)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::GetPlayAreaSize", _b); } }
	vr::HmdVector3_t minPoint, maxPoint;
	bool success = GetMinMaxPoints(minPoint, maxPoint);

	if (!success)
		return false;

	*pSizeX = maxPoint.v[0] - minPoint.v[0];
	*pSizeZ = maxPoint.v[2] - minPoint.v[2];

	// TODO verify return value
	return true;
}
bool BaseChaperone::GetPlayAreaRect(HmdQuad_t* rect)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::GetPlayAreaRect", _b); } }
	memset(rect, 0, sizeof(vr::HmdQuad_t));

	vr::HmdVector3_t minPoint, maxPoint;
	bool success = GetMinMaxPoints(minPoint, maxPoint);

	if (!success)
		return false;

	rect->vCorners[0].v[0] = maxPoint.v[0];
	rect->vCorners[0].v[2] = maxPoint.v[2];

	rect->vCorners[1].v[0] = maxPoint.v[0];
	rect->vCorners[1].v[2] = minPoint.v[2];

	rect->vCorners[2].v[0] = minPoint.v[0];
	rect->vCorners[2].v[2] = minPoint.v[2];

	rect->vCorners[3].v[0] = minPoint.v[0];
	rect->vCorners[3].v[2] = maxPoint.v[2];

	return true;
}
void BaseChaperone::ReloadInfo(void)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::ReloadInfo", _b); } }
	STUBBED();
}
void BaseChaperone::SetSceneColor(HmdColor_t color)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::SetSceneColor", _b); } }
	OOVR_LOG_ONCE("No implementation");
}
void BaseChaperone::GetBoundsColor(HmdColor_t* pOutputColorArray, int nNumOutputColors, float flCollisionBoundsFadeDistance, HmdColor_t* pOutputCameraColor)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::GetBoundsColor", _b); } }
	STUBBED();
}
bool BaseChaperone::AreBoundsVisible()
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::AreBoundsVisible", _b); } }
	return BackendManager::Instance().AreBoundsVisible();
}
void BaseChaperone::ForceBoundsVisible(bool bForce)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::ForceBoundsVisible", _b); } }
	return BackendManager::Instance().ForceBoundsVisible(bForce);
}

bool BaseChaperone::GetMinMaxPoints(vr::HmdVector3_t& minPoint, vr::HmdVector3_t& maxPoint)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::GetMinMaxPoints", _b); } }
	int count;
	bool success = BackendManager::Instance().GetPlayAreaPoints(nullptr, &count);

	if (!success)
		return false; // the play area isn't set up, or is unsupported by the backend

	std::vector<vr::HmdVector3_t> points(count);
	success = BackendManager::Instance().GetPlayAreaPoints(points.data(), nullptr);

	if (!success)
		return false; // shouldn't happen - should be caught by first check

	if (points.size() < 2)
		return false; // not enough points to find a min/max

	minPoint = points[0];
	maxPoint = points[0];

	for (const auto& point : points) {
		for (int i = 0; i < 3; i++) {
			minPoint.v[i] = std::min(minPoint.v[i], point.v[i]);
			maxPoint.v[i] = std::max(maxPoint.v[i], point.v[i]);
		}
	}

	return true;
}

void BaseChaperone::ResetZeroPose(vr::ETrackingUniverseOrigin eTrackingUniverseOrigin)
{
	{ static thread_local int _n=0; if(_n++<20) { char _b[64]; snprintf(_b,sizeof(_b),"TRACE-ENTRY this=%p",(void*)this); oovr_log_raw(__FILE__, __LINE__, "BaseChaperone::ResetZeroPose", _b); } }
	if (eTrackingUniverseOrigin != TrackingUniverseSeated) {
		OOVR_LOG_ONCE("No implementation");
	}

	// TODO do we have to do anything about the tracking origin?
	GetUnsafeBaseSystem()->ResetSeatedZeroPose();
}
