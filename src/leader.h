/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "addresses.h"
#include "gamesystem.h"
#include "playermanager.h"

#include "utils/entity.h"

#include "entity/cbaseentity.h"
#include "entity/cbasemodelentity.h"
#include "entity/cparticlesystem.h"

#define LEADER_PREFIX " \4[ZLeader]\1 "
#define MAXMARKERS 5

struct MarkerVisuals_t
{
	const Color clColor;
	const char* pszSpriteName;
};

extern MarkerVisuals_t MarkerVisualMaps[MAXMARKERS];

struct Marker_t
{
	CHandle<CBaseModelEntity> hModel;
	CHandle<CParticleSystem> hSprite;

	void Init();
	void Create(Vector& origin, QAngle& angles, CBaseEntity* pHitEntity, const MarkerVisuals_t visMap);
	bool IsTrainEntity(CBaseEntity* pEntity);
};

class CLeader
{
public:
	CLeader()
	{
		m_pPlayer = nullptr;
		m_iMarkerIndex = -1;
		m_nButtonsPrevious = IN_NONE;
		m_nButtons = IN_NONE;
		m_flRightClickDuration = -1.f;
	}

	bool SetLeader(ZEPlayer* pPlayer);
	void PutMarker();
	void RemoveAllMarkers();
	void TraceRay(const Vector& vecStart, const Vector& vecEnd, const bbox_t& nBounds, CTraceFilter* pFilter, trace_t& nTrace) { addresses::TracePlayerBBox(vecStart, vecEnd, nBounds, pFilter, nTrace); }

	ZEPlayer* GetLeader() { return m_pPlayer; }
	void GetAimingPos(CCSPlayerPawn* pPlayer, Vector& vecPos, QAngle& angAng);

private:
	ZEPlayer* m_pPlayer;
	Marker_t m_markers[MAXMARKERS];
	int m_iMarkerIndex;
	uint64 m_nButtonsPrevious;
	uint64 m_nButtons;
	float m_flRightClickDuration;
};

extern CLeader* g_pLeader;

void Leader_Precache(IEntityResourceManifest* pResourceManifest);