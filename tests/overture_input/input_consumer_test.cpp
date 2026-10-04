#include "openvr.h"
#include "math/Math.h"
#include <cstdio>
#include <string>
#include <vector>
#include "input/SteamVRInput.h"
#include "game/Game.h"
using namespace vr;
bool focused=true;
bool actions_active=true;
bool trigger_held=true;
bool aim_active=false;
bool aim_error=false;
unsigned long tick=100;
int focus_checks=0;
namespace hpl { cGame* gGame=nullptr; unsigned long GetApplicationTime() { return tick; } }
// The broad OpenVR interfaces require inert overrides for unused methods.
// Only external sampling is substituted; the input/controller consumer below
// is the same source compiled into the Overture product.
class FakeInput : public vr::IVRInput { public:
EVRInputError SetActionManifestPath( const char *pchActionManifestPath ) override { return {}; }
EVRInputError GetActionSetHandle( const char *, VRActionSetHandle_t *pHandle ) override { *pHandle=1; return VRInputError_None; }
EVRInputError GetActionHandle( const char *name, VRActionHandle_t *pHandle ) override {
    const std::string path(name);
    *pHandle=path=="/actions/gameplay/in/move"?101:
        path=="/actions/gameplay/in/interact"?102:
        path=="/actions/ui/in/drag"?103:
        path=="/actions/global/in/right_aim"?104:200;
    return VRInputError_None;
}
EVRInputError GetInputSourceHandle( const char *, VRInputValueHandle_t *pHandle ) override { *pHandle=1; return VRInputError_None; }
EVRInputError UpdateActionState( VR_ARRAY_COUNT( unSetCount ) VRActiveActionSet_t *pSets, uint32_t unSizeOfVRSelectedActionSet_t, uint32_t unSetCount ) override { return {}; }
EVRInputError GetDigitalActionData( VRActionHandle_t action, InputDigitalActionData_t *pActionData, uint32_t unActionDataSize, VRInputValueHandle_t ulRestrictToDevice ) override { *pActionData = {}; pActionData->bActive=actions_active; pActionData->bState=trigger_held && (action==102 || action==103); pActionData->bChanged=true; return VRInputError_None; }
EVRInputError GetAnalogActionData( VRActionHandle_t action, InputAnalogActionData_t *pActionData, uint32_t unActionDataSize, VRInputValueHandle_t ulRestrictToDevice ) override { *pActionData = {}; if (actions_active && action == 101) { pActionData->bActive=true; pActionData->y=0.9f; } return VRInputError_None; }
EVRInputError GetPoseActionDataRelativeToNow( VRActionHandle_t action, ETrackingUniverseOrigin eOrigin, float fPredictedSecondsFromNow, InputPoseActionData_t *pActionData, uint32_t unActionDataSize, VRInputValueHandle_t ulRestrictToDevice ) override { return {}; }
EVRInputError GetPoseActionDataForNextFrame( VRActionHandle_t action, ETrackingUniverseOrigin, InputPoseActionData_t *data, uint32_t, VRInputValueHandle_t ) override {
    *data={}; if (action==104) {
      if (aim_error) return VRInputError_NoData;
      data->bActive=aim_active; data->pose.bPoseIsValid=true; data->pose.bDeviceIsConnected=true;
      data->pose.mDeviceToAbsoluteTracking.m[0][0]=1;
      data->pose.mDeviceToAbsoluteTracking.m[1][1]=1;
      data->pose.mDeviceToAbsoluteTracking.m[2][2]=1;
      data->pose.mDeviceToAbsoluteTracking.m[0][3]=7;
    } return VRInputError_None;
}
EVRInputError GetSkeletalActionData( VRActionHandle_t action, InputSkeletalActionData_t *pActionData, uint32_t unActionDataSize ) override { return {}; }
EVRInputError GetDominantHand( ETrackedControllerRole *peDominantHand ) override { return {}; }
EVRInputError SetDominantHand( ETrackedControllerRole eDominantHand ) override { return {}; }
EVRInputError GetEyeTrackingDataRelativeToNow( VRActionHandle_t action, vr::ETrackingUniverseOrigin eOrigin, float fPredictedSecondsFromNow, vr::VREyeTrackingData_t *pEyeTrackingData, uint32_t ulEyeTrackingDataSize ) override { return {}; }
EVRInputError GetEyeTrackingDataForNextFrame( VRActionHandle_t action, vr::ETrackingUniverseOrigin eOrigin, vr::VREyeTrackingData_t *pEyeTrackingData, uint32_t ulEyeTrackingDataSize ) override { return {}; }
EVRInputError GetBoneCount( VRActionHandle_t action, uint32_t* pBoneCount ) override { return {}; }
EVRInputError GetBoneHierarchy( VRActionHandle_t action, VR_ARRAY_COUNT( unIndexArayCount ) BoneIndex_t* pParentIndices, uint32_t unIndexArayCount ) override { return {}; }
EVRInputError GetBoneName( VRActionHandle_t action, BoneIndex_t nBoneIndex, VR_OUT_STRING() char* pchBoneName, uint32_t unNameBufferSize ) override { return {}; }
EVRInputError GetSkeletalReferenceTransforms( VRActionHandle_t action, EVRSkeletalTransformSpace eTransformSpace, EVRSkeletalReferencePose eReferencePose, VR_ARRAY_COUNT( unTransformArrayCount ) VRBoneTransform_t *pTransformArray, uint32_t unTransformArrayCount ) override { return {}; }
EVRInputError GetSkeletalTrackingLevel( VRActionHandle_t action, EVRSkeletalTrackingLevel* pSkeletalTrackingLevel ) override { return {}; }
EVRInputError GetSkeletalBoneData( VRActionHandle_t action, EVRSkeletalTransformSpace eTransformSpace, EVRSkeletalMotionRange eMotionRange, VR_ARRAY_COUNT( unTransformArrayCount ) VRBoneTransform_t *pTransformArray, uint32_t unTransformArrayCount ) override { return {}; }
EVRInputError GetSkeletalSummaryData( VRActionHandle_t action, EVRSummaryType eSummaryType, VRSkeletalSummaryData_t * pSkeletalSummaryData ) override { return VRInputError_NoData; }
EVRInputError GetSkeletalBoneDataCompressed( VRActionHandle_t action, EVRSkeletalMotionRange eMotionRange, VR_OUT_BUFFER_COUNT( unCompressedSize ) void *pvCompressedData, uint32_t unCompressedSize, uint32_t *punRequiredCompressedSize ) override { return {}; }
EVRInputError DecompressSkeletalBoneData( const void *pvCompressedBuffer, uint32_t unCompressedBufferSize, EVRSkeletalTransformSpace eTransformSpace, VR_ARRAY_COUNT( unTransformArrayCount ) VRBoneTransform_t *pTransformArray, uint32_t unTransformArrayCount ) override { return {}; }
EVRInputError TriggerHapticVibrationAction( VRActionHandle_t action, float fStartSecondsFromNow, float fDurationSeconds, float fFrequency, float fAmplitude, VRInputValueHandle_t ulRestrictToDevice ) override { return {}; }
EVRInputError GetActionOrigins( VRActionSetHandle_t actionSetHandle, VRActionHandle_t digitalActionHandle, VR_ARRAY_COUNT( originOutCount ) VRInputValueHandle_t *originsOut, uint32_t originOutCount ) override { return {}; }
EVRInputError GetOriginLocalizedName( VRInputValueHandle_t origin, VR_OUT_STRING() char *pchNameArray, uint32_t unNameArraySize, int32_t unStringSectionsToInclude ) override { return {}; }
EVRInputError GetOriginTrackedDeviceInfo( VRInputValueHandle_t origin, InputOriginInfo_t *pOriginInfo, uint32_t unOriginInfoSize ) override { return {}; }
EVRInputError GetActionBindingInfo( VRActionHandle_t action, VR_ARRAY_COUNT( unBindingInfoCount ) InputBindingInfo_t *pOriginInfo, uint32_t unBindingInfoSize, uint32_t unBindingInfoCount, uint32_t *punReturnedBindingInfoCount ) override { return {}; }
EVRInputError ShowActionOrigins( VRActionSetHandle_t actionSetHandle, VRActionHandle_t ulActionHandle ) override { return {}; }
EVRInputError ShowBindingsForActionSet( VR_ARRAY_COUNT( unSetCount ) VRActiveActionSet_t *pSets, uint32_t unSizeOfVRSelectedActionSet_t, uint32_t unSetCount, VRInputValueHandle_t originToHighlight ) override { return {}; }
EVRInputError GetComponentStateForBinding( const char *pchRenderModelName, const char *pchComponentName, const InputBindingInfo_t *pOriginInfo, uint32_t unBindingInfoSize, uint32_t unBindingInfoCount, vr::RenderModel_ComponentState_t *pComponentState ) override { return {}; }
bool IsUsingLegacyInput() override { return {}; }
EVRInputError OpenBindingUI( const char* pchAppKey, VRActionSetHandle_t ulActionSetHandle, VRInputValueHandle_t ulDeviceHandle, bool bShowOnDesktop ) override { return {}; }
EVRInputError GetBindingVariant( vr::VRInputValueHandle_t ulDevicePath, VR_OUT_STRING() char *pchVariantArray, uint32_t unVariantArraySize ) override { return {}; }
};
class FakeSystem : public vr::IVRSystem { public:
void GetRecommendedRenderTargetSize( uint32_t *pnWidth, uint32_t *pnHeight ) override {}
HmdMatrix44_t GetProjectionMatrix( EVREye eEye, float fNearZ, float fFarZ ) override { return {}; }
void GetProjectionRaw( EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom ) override {}
bool ComputeDistortion( EVREye eEye, float fU, float fV, DistortionCoordinates_t *pDistortionCoordinates ) override { return {}; }
bool ComputeDistortionSet( EVREye eEye, EVRDistortionChannel eChannel, bool bAsNormalizedDeviceCoordinates, uint32_t nNumCoordinates, const DistortionCoordinate_t *pInput, DistortionCoordinate_t *pOutput ) override { return {}; }
HmdMatrix34_t GetEyeToHeadTransform( EVREye eEye ) override { return {}; }
bool GetTimeSinceLastVsync( float *pfSecondsSinceLastVsync, uint64_t *pulFrameCounter ) override { return {}; }
int32_t GetD3D9AdapterIndex() override { return {}; }
void GetDXGIOutputInfo( int32_t *pnAdapterIndex ) override {}
void GetOutputDevice( uint64_t *pnDevice, ETextureType textureType, VkInstance_T *pInstance = nullptr ) override {}
bool IsDisplayOnDesktop() override { return {}; }
bool SetDisplayVisibility( bool bIsVisibleOnDesktop ) override { return {}; }
void GetDeviceToAbsoluteTrackingPose( ETrackingUniverseOrigin eOrigin, float fPredictedSecondsToPhotonsFromNow, VR_ARRAY_COUNT(unTrackedDevicePoseArrayCount) TrackedDevicePose_t *pTrackedDevicePoseArray, uint32_t unTrackedDevicePoseArrayCount ) override {}
HmdMatrix34_t GetSeatedZeroPoseToStandingAbsoluteTrackingPose() override { return {}; }
HmdMatrix34_t GetRawZeroPoseToStandingAbsoluteTrackingPose() override { return {}; }
uint32_t GetSortedTrackedDeviceIndicesOfClass( ETrackedDeviceClass eTrackedDeviceClass, VR_ARRAY_COUNT(unTrackedDeviceIndexArrayCount) vr::TrackedDeviceIndex_t *punTrackedDeviceIndexArray, uint32_t unTrackedDeviceIndexArrayCount, vr::TrackedDeviceIndex_t unRelativeToTrackedDeviceIndex = k_unTrackedDeviceIndex_Hmd ) override { return {}; }
EDeviceActivityLevel GetTrackedDeviceActivityLevel( vr::TrackedDeviceIndex_t unDeviceId ) override { return {}; }
void ApplyTransform( TrackedDevicePose_t *pOutputPose, const TrackedDevicePose_t *pTrackedDevicePose, const HmdMatrix34_t *pTransform ) override {}
vr::TrackedDeviceIndex_t GetTrackedDeviceIndexForControllerRole( vr::ETrackedControllerRole unDeviceType ) override { return {}; }
vr::ETrackedControllerRole GetControllerRoleForTrackedDeviceIndex( vr::TrackedDeviceIndex_t unDeviceIndex ) override { return {}; }
ETrackedDeviceClass GetTrackedDeviceClass( vr::TrackedDeviceIndex_t unDeviceIndex ) override { return {}; }
bool IsTrackedDeviceConnected( vr::TrackedDeviceIndex_t unDeviceIndex ) override { return {}; }
bool GetBoolTrackedDeviceProperty( vr::TrackedDeviceIndex_t unDeviceIndex, ETrackedDeviceProperty prop, ETrackedPropertyError *pError = 0L ) override { return {}; }
float GetFloatTrackedDeviceProperty( vr::TrackedDeviceIndex_t unDeviceIndex, ETrackedDeviceProperty prop, ETrackedPropertyError *pError = 0L ) override { return {}; }
int32_t GetInt32TrackedDeviceProperty( vr::TrackedDeviceIndex_t unDeviceIndex, ETrackedDeviceProperty prop, ETrackedPropertyError *pError = 0L ) override { return {}; }
uint64_t GetUint64TrackedDeviceProperty( vr::TrackedDeviceIndex_t unDeviceIndex, ETrackedDeviceProperty prop, ETrackedPropertyError *pError = 0L ) override { return {}; }
HmdMatrix34_t GetMatrix34TrackedDeviceProperty( vr::TrackedDeviceIndex_t unDeviceIndex, ETrackedDeviceProperty prop, ETrackedPropertyError *pError = 0L ) override { return {}; }
uint32_t GetArrayTrackedDeviceProperty( vr::TrackedDeviceIndex_t unDeviceIndex, ETrackedDeviceProperty prop, PropertyTypeTag_t propType, void *pBuffer, uint32_t unBufferSize, ETrackedPropertyError *pError = 0L ) override { return {}; }
uint32_t GetStringTrackedDeviceProperty( vr::TrackedDeviceIndex_t unDeviceIndex, ETrackedDeviceProperty prop, VR_OUT_STRING() char *pchValue, uint32_t unBufferSize, ETrackedPropertyError *pError = 0L ) override { return {}; }
const char *GetPropErrorNameFromEnum( ETrackedPropertyError error ) override { return {}; }
bool PollNextEvent( VREvent_t *pEvent, uint32_t uncbVREvent ) override { return {}; }
bool PollNextEventWithPose( ETrackingUniverseOrigin eOrigin, VREvent_t *pEvent, uint32_t uncbVREvent, vr::TrackedDevicePose_t *pTrackedDevicePose ) override { return {}; }
bool PollNextEventWithPoseAndOverlays( vr::ETrackingUniverseOrigin eOrigin, VREvent_t *pEvent, uint32_t uncbVREvent, TrackedDevicePose_t *pTrackedDevicePose, VROverlayHandle_t *pulOverlayHandle ) override { return {}; }
const char *GetEventTypeNameFromEnum( EVREventType eType ) override { return {}; }
HiddenAreaMesh_t GetHiddenAreaMesh( EVREye eEye, EHiddenAreaMeshType type = k_eHiddenAreaMesh_Standard ) override { return {}; }
bool GetEyeTrackedFoveationCenter( HmdVector2_t *pNdcLeft, HmdVector2_t *pNdcRight ) override { return {}; }
bool GetEyeTrackedFoveationCenterForProjection( const HmdMatrix44_t *pProjMat, HmdVector2_t *pNdc ) override { return {}; }
bool GetControllerState( vr::TrackedDeviceIndex_t unControllerDeviceIndex, vr::VRControllerState_t *pControllerState, uint32_t unControllerStateSize ) override { *pControllerState = {}; pControllerState->rAxis[0].y=0.9f; pControllerState->ulButtonPressed=trigger_held?(1ULL<<k_EButton_SteamVR_Trigger):0; return true; }
bool GetControllerStateWithPose( ETrackingUniverseOrigin eOrigin, vr::TrackedDeviceIndex_t unControllerDeviceIndex, vr::VRControllerState_t *pControllerState, uint32_t unControllerStateSize, TrackedDevicePose_t *pTrackedDevicePose ) override { return {}; }
void TriggerHapticPulse( vr::TrackedDeviceIndex_t unControllerDeviceIndex, uint32_t unAxisId, unsigned short usDurationMicroSec ) override {}
const char *GetButtonIdNameFromEnum( EVRButtonId eButtonId ) override { return {}; }
const char *GetControllerAxisTypeNameFromEnum( EVRControllerAxisType eAxisType ) override { return {}; }
bool IsInputAvailable() override { ++focus_checks; return focused; }
bool IsSteamVRDrawingControllers() override { return {}; }
bool ShouldApplicationPause() override { return {}; }
bool ShouldApplicationReduceRenderingWork() override { return {}; }
vr::EVRFirmwareError PerformFirmwareUpdate( vr::TrackedDeviceIndex_t unDeviceIndex ) override { return {}; }
void AcknowledgeQuit_Exiting() override {}
uint32_t GetAppContainerFilePaths( VR_OUT_STRING() char *pchBuffer, uint32_t unBufferSize ) override { return {}; }
const char *GetRuntimeVersion() override { return {}; }
vr::EVRInitError SetSDKVersion( uint32_t nVersionMajor, uint32_t nVersionMinor, uint32_t nVersionBuild ) override { return {}; }
};
class FakeCompositor : public vr::IVRCompositor { public:
void SetTrackingSpace( ETrackingUniverseOrigin eOrigin ) override {}
ETrackingUniverseOrigin GetTrackingSpace() override { return TrackingUniverseStanding; }
EVRCompositorError WaitGetPoses( VR_ARRAY_COUNT( unRenderPoseArrayCount ) TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount, VR_ARRAY_COUNT( unGamePoseArrayCount ) TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount ) override { return {}; }
EVRCompositorError GetLastPoses( VR_ARRAY_COUNT( unRenderPoseArrayCount ) TrackedDevicePose_t* pRenderPoseArray, uint32_t unRenderPoseArrayCount, VR_ARRAY_COUNT( unGamePoseArrayCount ) TrackedDevicePose_t* pGamePoseArray, uint32_t unGamePoseArrayCount ) override { return {}; }
EVRCompositorError GetLastPoseForTrackedDeviceIndex( TrackedDeviceIndex_t unDeviceIndex, TrackedDevicePose_t *pOutputPose, TrackedDevicePose_t *pOutputGamePose ) override { return {}; }
EVRCompositorError GetSubmitTexture( Texture_t *pOutTexture, bool *pNeedsFlush, EVRCompositorTextureUsage eUsage, const Texture_t *pTexture, const VRTextureBounds_t *pBounds = 0, EVRSubmitFlags nSubmitFlags = Submit_Default ) override { return {}; }
EVRCompositorError Submit( EVREye eEye, const Texture_t *pTexture, const VRTextureBounds_t* pBounds = 0, EVRSubmitFlags nSubmitFlags = Submit_Default ) override { return {}; }
EVRCompositorError SubmitWithArrayIndex( EVREye eEye, const Texture_t *pTexture, uint32_t unTextureArrayIndex, const VRTextureBounds_t *pBounds = 0, EVRSubmitFlags nSubmitFlags = Submit_Default ) override { return {}; }
void ClearLastSubmittedFrame() override {}
void PostPresentHandoff() override {}
bool GetFrameTiming( Compositor_FrameTiming *pTiming, uint32_t unFramesAgo = 0 ) override { return {}; }
uint32_t GetFrameTimings( VR_ARRAY_COUNT( nFrames ) Compositor_FrameTiming *pTiming, uint32_t nFrames ) override { return {}; }
float GetFrameTimeRemaining() override { return {}; }
void GetCumulativeStats( Compositor_CumulativeStats *pStats, uint32_t nStatsSizeInBytes ) override {}
void FadeToColor( float fSeconds, float fRed, float fGreen, float fBlue, float fAlpha, bool bBackground = false ) override {}
HmdColor_t GetCurrentFadeColor( bool bBackground = false ) override { return {}; }
void FadeGrid( float fSeconds, bool bFadeGridIn ) override {}
float GetCurrentGridAlpha() override { return {}; }
EVRCompositorError SetSkyboxOverride( VR_ARRAY_COUNT( unTextureCount ) const Texture_t *pTextures, uint32_t unTextureCount ) override { return {}; }
void ClearSkyboxOverride() override {}
void CompositorBringToFront() override {}
void CompositorGoToBack() override {}
void CompositorQuit() override {}
bool IsFullscreen() override { return {}; }
uint32_t GetCurrentSceneFocusProcess() override { return {}; }
uint32_t GetLastFrameRenderer() override { return {}; }
bool CanRenderScene() override { return {}; }
void ShowMirrorWindow() override {}
void HideMirrorWindow() override {}
bool IsMirrorWindowVisible() override { return {}; }
void CompositorDumpImages() override {}
bool ShouldAppRenderWithLowResources() override { return {}; }
void ForceInterleavedReprojectionOn( bool bOverride ) override {}
void ForceReconnectProcess() override {}
void SuspendRendering( bool bSuspend ) override {}
vr::EVRCompositorError GetMirrorTextureD3D11( vr::EVREye eEye, void *pD3D11DeviceOrResource, void **ppD3D11ShaderResourceView ) override { return {}; }
void ReleaseMirrorTextureD3D11( void *pD3D11ShaderResourceView ) override {}
vr::EVRCompositorError GetMirrorTextureGL( vr::EVREye eEye, vr::glUInt_t *pglTextureId, vr::glSharedTextureHandle_t *pglSharedTextureHandle ) override { return {}; }
bool ReleaseSharedGLTexture( vr::glUInt_t glTextureId, vr::glSharedTextureHandle_t glSharedTextureHandle ) override { return {}; }
void LockGLSharedTextureForAccess( vr::glSharedTextureHandle_t glSharedTextureHandle ) override {}
void UnlockGLSharedTextureForAccess( vr::glSharedTextureHandle_t glSharedTextureHandle ) override {}
uint32_t GetVulkanInstanceExtensionsRequired( VR_OUT_STRING() char *pchValue, uint32_t unBufferSize ) override { return {}; }
uint32_t GetVulkanDeviceExtensionsRequired( VkPhysicalDevice_T *pPhysicalDevice, VR_OUT_STRING() char *pchValue, uint32_t unBufferSize ) override { return {}; }
void SetExplicitTimingMode( EVRCompositorTimingMode eTimingMode ) override {}
EVRCompositorError SubmitExplicitTimingData() override { return {}; }
bool IsMotionSmoothingEnabled() override { return {}; }
bool IsMotionSmoothingSupported() override { return {}; }
bool IsCurrentSceneFocusAppLoading() override { return {}; }
EVRCompositorError SetStageOverride_Async( const char *pchRenderModelPath, const HmdMatrix34_t *pTransform = 0, const Compositor_StageRenderSettings *pRenderSettings = 0, uint32_t nSizeOfRenderSettings = 0 ) override { return {}; }
void ClearStageOverride() override {}
bool GetCompositorBenchmarkResults( Compositor_BenchmarkResults *pBenchmarkResults, uint32_t nSizeOfBenchmarkResults ) override { return {}; }
EVRCompositorError GetLastPosePredictionIDs( uint32_t *pRenderPosePredictionID, uint32_t *pGamePosePredictionID ) override { return {}; }
EVRCompositorError GetPosesForFrame( uint32_t unPosePredictionID, VR_ARRAY_COUNT( unPoseArrayCount ) TrackedDevicePose_t* pPoseArray, uint32_t unPoseArrayCount ) override { return {}; }
};

namespace vr { IVRInput* audit_input=nullptr; IVRCompositor* audit_compositor=nullptr; IVRSystem* audit_system=nullptr; }

int failures=0;
void Check(bool ok,const char* name) { if (!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",name); } }
int main() {
    FakeInput input; FakeSystem system; FakeCompositor compositor;
    audit_input=&input; audit_system=&system; audit_compositor=&compositor;
    hpl::cGame game{&system}; hpl::gGame=&game;
    hpl::TrackedController left,right;
    left.SetPose(hpl::cMatrixf::Identity,{}, {},1);
    right.SetPose(hpl::cMatrixf::Identity,{}, {},2);
    hpl::cSteamVRInput reader;
    Check(reader.Initialize(),"public manifest initialization");
    reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(reader.GetState().moveY>0.8f && reader.GetState().interact.pressed,"focused action controls work");
    actions_active=false; ++tick;
    reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(reader.GetState().moveY>0.8f,"focused binding refresh retains grace");
    focused=false; ++tick;
    Check(reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right),"focus loss must prevent legacy fallback");
    Check(reader.GetState().moveY==0 && !reader.GetState().interact.pressed && reader.GetState().interact.justReleased,"focus loss releases movement and interaction immediately");
    ++tick; reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(!reader.GetState().interact.justReleased,"focus release edge emitted once");
    left.UpdateButtonState(); right.UpdateButtonState();
    reader.UpdateLegacyState(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(reader.GetState().moveY==0 && !reader.GetState().interact.pressed,"raw polling cannot bypass focus");
    focused=true; actions_active=true; ++tick;
    reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(reader.GetState().moveY>0.8f && !reader.GetState().interact.pressed,"focus return uses fresh axes and latches held interaction");
    ++tick; reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(!reader.GetState().interact.pressed,"interaction remains latched until release");
    trigger_held=false; ++tick; reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right);
    trigger_held=true; ++tick; reader.Update(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(reader.GetState().interact.justPressed,"fresh press after focus recovery works");
    reader.Update(hpl::eSteamVRInputContext_UI,left,right);
    Check(reader.GetState().uiDrag.pressed,"UI drag fixture held");
    focused=false; ++tick; reader.Update(hpl::eSteamVRInputContext_UI,left,right);
    Check(reader.GetState().uiDrag.justReleased && !reader.GetState().uiDrag.pressed,"focus loss releases UI drag");
    focused=true; aim_active=true; ++tick; reader.Update(hpl::eSteamVRInputContext_UI,left,right);
    Check(right.IsAimValid(),"active aim acquired");
    right.BeginPoseFrame();
    Check(!right.IsAimValid(),"new tracking frame cannot reuse previous aim");
    auto grip=hpl::cMatrixf::Identity; grip.m[0][3]=2;
    right.SetPose(grip,{}, {},2); aim_active=false;
    reader.Update(hpl::eSteamVRInputContext_UI,left,right);
    Check(!right.IsAimValid() && right.IsPoseValid() && right.GetMatrix().m[0][3]==2,"inactive aim uses current grip");
    aim_active=true; reader.Update(hpl::eSteamVRInputContext_UI,left,right);
    aim_error=true; reader.Update(hpl::eSteamVRInputContext_UI,left,right);
    Check(!right.IsAimValid() && right.IsPoseValid(),"aim read error clears auxiliary pose within same frame");
    aim_error=false; reader.Update(hpl::eSteamVRInputContext_UI,left,right);
    right.SetPoseValid(false);
    Check(!right.IsAimValid(),"lost grip clears auxiliary aim");
    hpl::cSteamVRInput legacy;
    focused=false; legacy.UpdateLegacyState(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(!legacy.GetState().interact.pressed,"manifest unavailable still respects focus");
    focused=true; right.SetPose(grip,{}, {},2); left.UpdateButtonState(); right.UpdateButtonState();
    legacy.UpdateLegacyState(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(!legacy.GetState().interact.pressed,"raw held button latched on focus regain");
    trigger_held=false; left.UpdateButtonState(); right.UpdateButtonState(); legacy.UpdateLegacyState(hpl::eSteamVRInputContext_Gameplay,left,right);
    trigger_held=true; left.UpdateButtonState(); right.UpdateButtonState(); legacy.UpdateLegacyState(hpl::eSteamVRInputContext_Gameplay,left,right);
    Check(legacy.GetState().interact.justPressed,"raw press resumes after release");
    std::printf("Overture input consumer: %d failures\n",failures);
    return failures?1:0;
}
