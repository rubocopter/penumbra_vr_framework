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
#ifndef GAME_VR_HAND_COLLISION_POLICY_H
#define GAME_VR_HAND_COLLISION_POLICY_H

#include "vr_interaction_policy.hpp"

// Keep the proven Overture source API while making the Framework runtime the
// single owner of game-neutral palm collision policy.
namespace VRHandCollisionPolicy =
	penumbra_vr::runtime::vr_interaction_policy;

#endif // GAME_VR_HAND_COLLISION_POLICY_H
