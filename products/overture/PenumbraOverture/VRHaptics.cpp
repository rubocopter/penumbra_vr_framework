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
#include "StdAfx.h"
#include "VRHaptics.h"

#include "Init.h"
#include "system/LowLevelSystem.h"

namespace
{
	std::uint32_t glLastSubmission[penumbra_vr::runtime::kVrHapticEventCount][2] = {};
	bool gbHasSubmitted[penumbra_vr::runtime::kVrHapticEventCount][2] = {};

	const char *GetHandName(eVRHapticHand aHand)
	{
		switch(aHand)
		{
		case eVRHapticHand_Left: return "left";
		case eVRHapticHand_Right: return "right";
		case eVRHapticHand_Both: return "both";
		default: return "unknown";
		}
	}

	bool SubmitToHand(cInit *apInit, eVRHapticEvent aEvent,
		eSteamVRHand aSteamVRHand, int alHandIndex,
		const penumbra_vr::runtime::VrHapticProfile &aProfile,
		float afAmplitude, std::uint32_t alNow)
	{
		const bool poseValid = aSteamVRHand == eSteamVRHand_Left
			? apInit->mpGame->vr_left_hand.IsPoseValid()
			: apInit->mpGame->vr_right_hand.IsPoseValid();
		if(!poseValid) return false;

		const std::size_t eventIndex = penumbra_vr::runtime::HapticEventIndex(aEvent);
		if(!penumbra_vr::runtime::HapticCooldownReady(
			aEvent, gbHasSubmitted[eventIndex][alHandIndex],
			glLastSubmission[eventIndex][alHandIndex], alNow))
		{
			return false;
		}

		if(!apInit->mpGame->vr_input.TriggerHaptic(aSteamVRHand,
			aProfile.duration_seconds, aProfile.frequency_hz, afAmplitude))
		{
			return false;
		}

		gbHasSubmitted[eventIndex][alHandIndex] = true;
		glLastSubmission[eventIndex][alHandIndex] = alNow;
		return true;
	}
}

bool cVRHaptics::Play(cInit *apInit, eVRHapticEvent aEvent,
	eVRHapticHand aHand, float afStrength)
{
	if(apInit == NULL || apInit->mpGame == NULL) return false;
	if(!penumbra_vr::runtime::IsKnownHapticEvent(aEvent)) return false;

	const penumbra_vr::runtime::VrHapticProfile profile =
		penumbra_vr::runtime::HapticProfile(aEvent);
	const float fAmplitude =
		penumbra_vr::runtime::ScaleHapticAmplitude(aEvent, afStrength);
	if(fAmplitude <= 0.0f) return false;

	const unsigned long lNow = GetApplicationTime();
	const std::uint32_t lNowMs = static_cast<std::uint32_t>(lNow);
	bool bSuccess = false;
	if(aHand == eVRHapticHand_Left || aHand == eVRHapticHand_Both)
	{
		bSuccess = SubmitToHand(apInit, aEvent, eSteamVRHand_Left, 0,
			profile, fAmplitude, lNowMs) || bSuccess;
	}
	if(aHand == eVRHapticHand_Right || aHand == eVRHapticHand_Both)
	{
		bSuccess = SubmitToHand(apInit, aEvent, eSteamVRHand_Right, 1,
			profile, fAmplitude, lNowMs) || bSuccess;
	}

	if(bSuccess)
	{
		Log(" [VR haptics +%lu ms] %s -> %s (submitted).\n",
			lNow, profile.name, GetHandName(aHand));
	}
	return bSuccess;
}
