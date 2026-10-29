/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of Penumbra Overture.
 *
 * Penumbra Overture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "VRSettings.h"

#include "StdAfx.h"
#include "resources/ConfigFile.h"

#include <cerrno>
#include <climits>
#include <cstdlib>

using namespace hpl;

namespace
{
	// HPL GetInt uses atoi, accepting partial values and silently turning
	// malformed input into zero. Keep Neural defaults for malformed integers.
	int ReadNeuralInt(cConfigFile *apConfig, const char *asKey, int alDefault)
	{
		const tString sValue = apConfig->GetString("VR", asKey, "");
		if(sValue.empty()) return alDefault;
		char *pEnd = NULL;
		errno = 0;
		const long lValue = std::strtol(sValue.c_str(), &pEnd, 10);
		if(errno == ERANGE || pEnd == sValue.c_str() || *pEnd != '\0' ||
			lValue < INT_MIN || lValue > INT_MAX) return alDefault;
		return static_cast<int>(lValue);
	}

	const float kDefaultMoveSpeed = 1.0f;
	const float kDefaultMoveDeadZone = 0.15f;
	const float kDefaultHeightOffset = 0.0f;
	const float kDefaultSnapTurnAngle = 45.0f;
	const float kDefaultSmoothTurnSpeed = 90.0f;
	const float kDefaultTurnDeadZone = 0.20f;
	const float kDefaultUIDistance = 1.75f;
	const float kDefaultUIScale = 1.0f;
	const float kDefaultRenderScale = 1.0f;
	const float kDefaultPhysicalCrouchDepth = 0.25f;
	const float kDefaultSubtitleScale = 1.35f;
	const float kDefaultPlayerHeight = 1.70f;

	const char *TurnModeName(eVRTurnMode aMode)
	{
		switch(aMode)
		{
		case eVRTurnMode_Disabled: return "Disabled";
		case eVRTurnMode_Smooth: return "Smooth";
		default: return "Snap";
		}
	}

	const char *CrouchModeName(eVRCrouchMode aMode)
	{
		switch(aMode)
		{
		case eVRCrouchMode_Physical: return "Physical";
		case eVRCrouchMode_Button: return "Button";
		default: return "Hybrid";
		}
	}

	const char *HandednessName(eVRHandedness aHandedness)
	{
		switch(aHandedness)
		{
		case eVRHandedness_Left: return "Left";
		default: return "Right";
		}
	}

	const char *PlayModeName(eVRPlayMode aPlayMode)
	{
		switch(aPlayMode)
		{
		case eVRPlayMode_Seated: return "Seated";
		default: return "Standing";
		}
	}

	const char *HRTFModeName(eVRHRTFMode aMode)
	{
		switch(aMode)
		{
		case eVRHRTFMode_On: return "On";
		case eVRHRTFMode_Off: return "Off";
		default: return "Auto";
		}
	}
}

cVRSettings::cVRSettings()
	: mfMoveSpeed(kDefaultMoveSpeed),
	  mfMoveDeadZone(kDefaultMoveDeadZone),
	  mfHeightOffset(kDefaultHeightOffset),
	  mTurnMode(eVRTurnMode_Snap),
	  mfSnapTurnAngle(kDefaultSnapTurnAngle),
	  mfSmoothTurnSpeed(kDefaultSmoothTurnSpeed),
	  mfTurnDeadZone(kDefaultTurnDeadZone),
	  mfUIDistance(kDefaultUIDistance),
	  mfUIScale(kDefaultUIScale),
	  mfRenderScale(kDefaultRenderScale),
	  mbEnhancedVisualsEnabled(false),
	  mCrouchMode(eVRCrouchMode_Hybrid),
	  mfPhysicalCrouchDepth(kDefaultPhysicalCrouchDepth),
	  mfSubtitleScale(kDefaultSubtitleScale),
	  mHandedness(eVRHandedness_Right),
	  mPlayMode(eVRPlayMode_Standing),
	  mfPlayerHeight(kDefaultPlayerHeight),
	  mHRTFMode(eVRHRTFMode_Auto)
{
}

void cVRSettings::Load(cConfigFile *apConfig)
{
	if(apConfig == NULL) return;

	mfMoveSpeed = Clamp(apConfig->GetFloat("VR", "MoveSpeed", kDefaultMoveSpeed), 0.25f, 3.0f);
	mfMoveDeadZone = Clamp(apConfig->GetFloat("VR", "MoveDeadZone", kDefaultMoveDeadZone), 0.0f, 0.9f);
	mfHeightOffset = Clamp(apConfig->GetFloat("VR", "HeightOffset", kDefaultHeightOffset), -0.5f, 0.5f);

	const tString sTurnMode = cString::ToLowerCase(apConfig->GetString("VR", "TurnMode", "Snap"));
	if(sTurnMode == "disabled") mTurnMode = eVRTurnMode_Disabled;
	else if(sTurnMode == "smooth") mTurnMode = eVRTurnMode_Smooth;
	else mTurnMode = eVRTurnMode_Snap;

	mfSnapTurnAngle = Clamp(apConfig->GetFloat("VR", "SnapTurnAngle", kDefaultSnapTurnAngle), 15.0f, 90.0f);
	mfSmoothTurnSpeed = Clamp(apConfig->GetFloat("VR", "SmoothTurnSpeed", kDefaultSmoothTurnSpeed), 30.0f, 360.0f);
	// Builds before the comfort pass persisted 120 deg/s as their untouched
	// default. Migrate that one legacy value once while preserving deliberate
	// custom speeds on existing profiles.
	const int lSettingsVersion = apConfig->GetInt("VR", "SettingsVersion", 0);
	if(lSettingsVersion < 1 && mfSmoothTurnSpeed > 119.9f && mfSmoothTurnSpeed < 120.1f)
		mfSmoothTurnSpeed = kDefaultSmoothTurnSpeed;
	mfTurnDeadZone = Clamp(apConfig->GetFloat("VR", "TurnDeadZone", kDefaultTurnDeadZone), 0.0f, 0.9f);
	mfUIDistance = Clamp(apConfig->GetFloat("VR", "UIDistance", kDefaultUIDistance), 0.75f, 3.0f);
	mfUIScale = Clamp(apConfig->GetFloat("VR", "UIScale", kDefaultUIScale), 0.5f, 2.0f);
	// Applied when the VR eye buffers are created, so a new value needs an
	// application restart to take effect.
	mfRenderScale = Clamp(apConfig->GetFloat("VR", "RenderScale", kDefaultRenderScale), 0.5f, 2.0f);
	// Migrate the old ambient-prototype switch into the complete visuals
	// bundle. The new key takes precedence once the settings are saved.
	mbEnhancedVisualsEnabled = apConfig->GetBool("VR", "EnhancedVisuals",
		apConfig->GetBool("VR", "HemisphericalAmbient", false));

	const tString sCrouchMode = cString::ToLowerCase(apConfig->GetString("VR", "CrouchMode", "Hybrid"));
	if(sCrouchMode == "physical") mCrouchMode = eVRCrouchMode_Physical;
	else if(sCrouchMode == "button") mCrouchMode = eVRCrouchMode_Button;
	else mCrouchMode = eVRCrouchMode_Hybrid;
	mfPhysicalCrouchDepth = Clamp(apConfig->GetFloat("VR", "PhysicalCrouchDepth", kDefaultPhysicalCrouchDepth), 0.10f, 0.60f);
	mfSubtitleScale = Clamp(apConfig->GetFloat("VR", "SubtitleScale", kDefaultSubtitleScale), 0.75f, 2.0f);

	const tString sHandedness = cString::ToLowerCase(apConfig->GetString("VR", "Handedness", "Right"));
	if(sHandedness == "left") mHandedness = eVRHandedness_Left;
	else mHandedness = eVRHandedness_Right;

	const tString sPlayMode = cString::ToLowerCase(apConfig->GetString("VR", "PlayMode", "Standing"));
	if(sPlayMode == "seated") mPlayMode = eVRPlayMode_Seated;
	else mPlayMode = eVRPlayMode_Standing;

	mfPlayerHeight = Clamp(apConfig->GetFloat("VR", "PlayerHeight", kDefaultPlayerHeight), 1.40f, 2.10f);

	// Binaural rendering through OpenAL Soft. Auto lets the runtime enable
	// HRTF only when the output device reports itself as headphones.
	const tString sHRTF = cString::ToLowerCase(apConfig->GetString("VR", "HRTF", "Auto"));
	if(sHRTF == "on") mHRTFMode = eVRHRTFMode_On;
	else if(sHRTF == "off") mHRTFMode = eVRHRTFMode_Off;
	else mHRTFMode = eVRHRTFMode_Auto;

	penumbra_vr::runtime::NeuralPostSettings neural;
	neural.enabled = apConfig->GetBool("VR", "NeuralEnabled", neural.enabled);
	neural.internal_height = ReadNeuralInt(apConfig, "NeuralInternalHeight", neural.internal_height);
	neural.scale_percent = ReadNeuralInt(apConfig, "NeuralScalePercent", neural.scale_percent);
	neural.intensity_percent = ReadNeuralInt(apConfig, "NeuralIntensityPercent", neural.intensity_percent);
	neural.passes = ReadNeuralInt(apConfig, "NeuralPasses", neural.passes);
	neural.style = ReadNeuralInt(apConfig, "NeuralStyle", neural.style);
	neural.fovea_percent = ReadNeuralInt(apConfig, "NeuralFoveaPercent", neural.fovea_percent);
	neural.outer_enabled = apConfig->GetBool("VR", "NeuralOuterEnabled", neural.outer_enabled);
	neural.outer_scale_percent = ReadNeuralInt(apConfig, "NeuralOuterScalePercent", neural.outer_scale_percent);
	SetNeuralSettings(neural);

	Log(" VR settings: move speed %.2f, dead zone %.2f, height offset %.2f m; "
		"turn %s, snap %.0f deg, smooth %.0f deg/s, turn dead zone %.2f; "
		"UI distance %.2f m, scale %.2f; render scale %.2f; enhanced visuals %s; crouch %s, depth %.2f m; subtitle scale %.2f; "
		"handedness %s, play mode %s, player height %.2f m; hrtf %s.\n",
		mfMoveSpeed, mfMoveDeadZone, mfHeightOffset, TurnModeName(mTurnMode),
		mfSnapTurnAngle, mfSmoothTurnSpeed, mfTurnDeadZone, mfUIDistance, mfUIScale,
		mfRenderScale, mbEnhancedVisualsEnabled ? "On" : "Off",
		CrouchModeName(mCrouchMode), mfPhysicalCrouchDepth, mfSubtitleScale,
		HandednessName(mHandedness), PlayModeName(mPlayMode), mfPlayerHeight, HRTFModeName(mHRTFMode));
}

void cVRSettings::Save(cConfigFile *apConfig) const
{
	if(apConfig == NULL) return;

	apConfig->SetFloat("VR", "MoveSpeed", mfMoveSpeed);
	apConfig->SetFloat("VR", "MoveDeadZone", mfMoveDeadZone);
	apConfig->SetFloat("VR", "HeightOffset", mfHeightOffset);
	apConfig->SetString("VR", "TurnMode", TurnModeName(mTurnMode));
	apConfig->SetFloat("VR", "SnapTurnAngle", mfSnapTurnAngle);
	apConfig->SetFloat("VR", "SmoothTurnSpeed", mfSmoothTurnSpeed);
	apConfig->SetFloat("VR", "TurnDeadZone", mfTurnDeadZone);
	apConfig->SetFloat("VR", "UIDistance", mfUIDistance);
	apConfig->SetFloat("VR", "UIScale", mfUIScale);
	apConfig->SetFloat("VR", "RenderScale", mfRenderScale);
	apConfig->SetBool("VR", "EnhancedVisuals", mbEnhancedVisualsEnabled);
	apConfig->SetString("VR", "CrouchMode", CrouchModeName(mCrouchMode));
	apConfig->SetFloat("VR", "PhysicalCrouchDepth", mfPhysicalCrouchDepth);
	apConfig->SetFloat("VR", "SubtitleScale", mfSubtitleScale);
	apConfig->SetString("VR", "Handedness", HandednessName(mHandedness));
	apConfig->SetString("VR", "PlayMode", PlayModeName(mPlayMode));
	apConfig->SetFloat("VR", "PlayerHeight", mfPlayerHeight);
	apConfig->SetString("VR", "HRTF", HRTFModeName(mHRTFMode));
	apConfig->SetInt("VR", "SettingsVersion", 1);
	apConfig->SetBool("VR", "NeuralEnabled", mNeuralSettings.enabled);
	apConfig->SetInt("VR", "NeuralInternalHeight", mNeuralSettings.internal_height);
	apConfig->SetInt("VR", "NeuralScalePercent", mNeuralSettings.scale_percent);
	apConfig->SetInt("VR", "NeuralIntensityPercent", mNeuralSettings.intensity_percent);
	apConfig->SetInt("VR", "NeuralPasses", mNeuralSettings.passes);
	apConfig->SetInt("VR", "NeuralStyle", mNeuralSettings.style);
	apConfig->SetInt("VR", "NeuralFoveaPercent", mNeuralSettings.fovea_percent);
	apConfig->SetBool("VR", "NeuralOuterEnabled", mNeuralSettings.outer_enabled);
	apConfig->SetInt("VR", "NeuralOuterScalePercent", mNeuralSettings.outer_scale_percent);
}

void cVRSettings::SetNeuralSettings(const penumbra_vr::runtime::NeuralPostSettings& aSettings)
{
	mNeuralSettings = aSettings;
	penumbra_vr::runtime::NormalizeNeuralPostSettings(mNeuralSettings);
}

float cVRSettings::Clamp(float afValue, float afMinimum, float afMaximum)
{
	if(afValue < afMinimum) return afMinimum;
	if(afValue > afMaximum) return afMaximum;
	return afValue;
}
