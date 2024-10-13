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

#include "gamesystem.h"
#include "igameevents.h"
#include "playermanager.h"
#include "utils/entity.h"

#include "entity/cbasemodelentity.h"
#include "entity/cpointworldtext.h"

#define LEADER_PREFIX " \4[ZLeader]\1 "

enum class ELeaderType
{
	NONE,
	HEAD,
	TEMPORARY,
	ASSISTANT,
};

class CLeaderMarker
{
public:
	CLeaderMarker(const Vector& origin, const QAngle& angles, int iIndex, CBaseEntity* pParent = nullptr);

	bool operator==(const CLeaderMarker& a) const { return m_iIndex == a.m_iIndex; }

	int GetIndex() { return m_iIndex; }
	void Remove();

private:
	CHandle<CBaseModelEntity> m_hModel;
	CHandle<CPointWorldText> m_hSprite;
	int m_iIndex;
};

class CZRLeader
{
public:
	CZRLeader(ZEPlayer* pPlayer, ELeaderType nType);
	~CZRLeader();

	void PutMarker();
	void RemoveAllMarkers();

	CBaseEntity* GetEntityAimed(Vector& origin, QAngle& angles, bool bIsRecursive = false);
	void TraceShape(const Vector& start, const Vector& end, CTraceFilter* pFilter, trace_t& pm) { addresses::TracePlayerBBox(start, end, { { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f } }, pFilter, pm); }

	ZEPlayer* GetPlayer() { return m_hPlayer.Get(); }

private:
	ZEPlayerHandle m_hPlayer;
	uint64 m_iButtonsPrevious;
	uint64 m_iButtons;
	bool m_bIgnoreAttack2Input;
	int m_iPaintModeQueue;
	int m_iMarkerIndex;
};

extern CUtlVector<CLeaderMarker> g_vecMarkers;
extern CUtlVector<CZRLeader> g_vecLeaders;

extern bool g_bEnableLeader;

int Leader_GetRequiredVoteCounts();
void Leader_Precache(IEntityResourceManifest* pResourceManifest);
void Leader_PostEventAbstract_Source1LegacyGameEvent(const uint64* clients, const CNetMessage* pData);
void Leader_OnRoundStart(IGameEvent* pEvent);