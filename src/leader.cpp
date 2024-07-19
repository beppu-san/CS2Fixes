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

#include "leader.h"
#include "ctimer.h"
#include "common.h"
#include "commands.h"

#define MARKER_MODEL "models/leader_mark/leader_mark.vmdl"

CLeader* g_pLeader = nullptr;

MarkerVisuals_t MarkerVisualMaps[] =
{
	{Color(255, 0, 0, 255), "particles/leader/mark_tag_a.vpcf"},
	{Color(0, 255, 0, 255), "particles/leader/mark_tag_b.vpcf"},
	{Color(0, 0, 255, 255), "particles/leader/mark_tag_c.vpcf"},
	{Color(255, 255, 0, 255), "particles/leader/mark_tag_d.vpcf"},
	{Color(255, 0, 255, 255), "particles/leader/mark_tag_e.vpcf"},
};

void Leader_Precache(IEntityResourceManifest* pResourceManifest)
{
	pResourceManifest->AddResource(MARKER_MODEL);

	for (int i = 0; i < MAXMARKERS; i++)
		pResourceManifest->AddResource(MarkerVisualMaps[i].pszSpriteName);
}

bool CLeader::SetLeader(ZEPlayer* pPlayer)
{
	if (!pPlayer)
		return false;

	m_pPlayer = pPlayer;
	m_iMarkerIndex = 0;

	new CTimer(.01f, false, true, [this, pPlayer]()
		{
			if (GetLeader() != pPlayer)
			{
				// Remove leader here?
				m_pPlayer = nullptr;
				m_iMarkerIndex = -1;
				m_nButtonsPrevious = IN_NONE;
				m_nButtons = IN_NONE;
				m_flRightClickDuration = -1.f;

				return -1.f;
			}

			CCSPlayerPawn* pPawn = (CCSPlayerPawn*)CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot())->GetPawn();
			CPlayer_MovementServices* pMovement = pPawn->m_pMovementServices();

			uint64 nButtons = pMovement->m_nButtons().m_pButtonStates()[0];
			m_nButtonsPrevious = m_nButtons;
			m_nButtons = nButtons;

			if (m_nButtons & IN_SPEED)
			{
				if (!(m_nButtonsPrevious & IN_ATTACK2) && (m_nButtons & IN_ATTACK2))
					RemoveAllMarkers();
			}
			else
			{
				if (m_flRightClickDuration <= .2f)
				{
					if ((m_nButtonsPrevious & IN_ATTACK2) && !(m_nButtons & IN_ATTACK2))
						PutMarker();
				}
				else
				{
					// if ((m_nButtonsPrevious & IN_ATTACK2) && (m_nButtons & IN_ATTACK2))
						// DrawPaint();
				}
			}

			m_flRightClickDuration = (m_nButtonsPrevious & IN_ATTACK2) && (m_nButtons & IN_ATTACK2) ? m_flRightClickDuration + .01f : 0.f;

			return .01f;
		});

	return true;
}

void CLeader::PutMarker()
{
	ZEPlayer* pPlayer = GetLeader();
	if (!pPlayer)
		return;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();

	if (!pPawn || !pPawn->IsAlive() || pPlayer->IsInfected())
		return;

	m_markers[m_iMarkerIndex].Init();

	Vector vecMarkerPos;
	QAngle angMarkerAngles;
	CBaseEntity* pParentEnt = UTIL_FindPickerEntity(pController);
	GetAimingPos(pPawn, vecMarkerPos, angMarkerAngles);

	m_markers[m_iMarkerIndex].Create(vecMarkerPos, angMarkerAngles, pParentEnt, MarkerVisualMaps[m_iMarkerIndex]);

	m_iMarkerIndex = (m_iMarkerIndex + 1) % MAXMARKERS;
}

void CLeader::RemoveAllMarkers()
{
	for (int i = 0; i < MAXMARKERS; i++)
		m_markers[i].Init();
}

void CLeader::GetAimingPos(CCSPlayerPawn* pPawn, Vector& vecPos, QAngle& angAng)
{
	Vector vecOrigin = pPawn->GetAbsOrigin();
	vecOrigin.z += 64.f;

	Vector vecForward;
	AngleVectors(pPawn->m_angEyeAngles(), &vecForward);
	vecForward.Normalized();

	const Vector vecEnd = vecOrigin + vecForward * 65536.f;
	const bbox_t nBounds = { {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f} };

	CTraceFilter pFilter;
	pFilter.m_bHitSolid = true;
	pFilter.m_bHitSolidRequiresGenerateContacts = true;
	pFilter.m_bShouldIgnoreDisabledPairs = true;
	pFilter.m_nCollisionGroup = COLLISION_GROUP_INTERACTIVE_DEBRIS;
	pFilter.m_nInteractsWith = 0x2c3011;
	pFilter.m_bUnknown = true;
	pFilter.m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
	pFilter.m_nInteractsAs = 0x40000;

	trace_t nTrace;

	TraceRay(vecOrigin, vecEnd, nBounds, &pFilter, nTrace);

	if (nTrace.DidHit())
	{
		vecPos = nTrace.m_vHitPoint;
		VectorAngles(nTrace.m_vHitNormal, angAng);
		angAng.x += 90.f;
	}
}

void Marker_t::Init()
{
	CBaseModelEntity* pModel = hModel.Get();
	if (pModel)
		UTIL_AddEntityIOEvent(pModel, "Kill", nullptr, nullptr, "", .02f);

	CParticleSystem* pSprite = hSprite.Get();
	if (pSprite)
	{
		UTIL_AddEntityIOEvent(pSprite, "DestroyImmediately", nullptr, nullptr, "", .0f);
		// UTIL_AddEntityIOEvent(pSprite, "Kill", nullptr, nullptr, "", .02f);	// somehow it crashes the server.
	}

	hModel = nullptr;
	hSprite = nullptr;
}

void Marker_t::Create(Vector& origin, QAngle& angles, CBaseEntity* pHitEntity, const MarkerVisuals_t visMap)
{
	CBaseModelEntity* pModel = (CBaseModelEntity*)CreateEntityByName("prop_dynamic_override");
	CEntityKeyValues* pKeyValuesModel = new CEntityKeyValues();
	pKeyValuesModel->SetString("model", MARKER_MODEL);
	pKeyValuesModel->SetString("DefaultAnim", "idle");
	pKeyValuesModel->SetInt("spawnflags", 256U);
	pKeyValuesModel->SetColor("rendercolor", visMap.clColor);
	pKeyValuesModel->SetInt("rendermode", kRenderTransColor);
	pKeyValuesModel->SetInt("renderfx", kRenderFxPulseSlow);
	pKeyValuesModel->SetInt("disableshadows", 1);
	pKeyValuesModel->SetInt("disablereceiveshadows", 1);

	pModel->DispatchSpawn(pKeyValuesModel);
	pModel->Teleport(&origin, &angles, nullptr);
	
	if (IsTrainEntity(pHitEntity))
		pModel->SetParent(pHitEntity);

	CParticleSystem* pSprite = (CParticleSystem*)CreateEntityByName("info_particle_system");
	CEntityKeyValues* pKeyValuesSprite = new CEntityKeyValues();
	pKeyValuesSprite->SetString("effect_name", visMap.pszSpriteName);
	pKeyValuesSprite->SetBool("start_active", true);

	angles.x -= 90.f;
	Vector vecAng;
	AngleVectors(angles, &vecAng);
	origin += vecAng * 30.f;

	pSprite->DispatchSpawn(pKeyValuesSprite);
	pSprite->Teleport(&origin, nullptr, nullptr);
	pSprite->SetParent(pModel);

	hModel = pModel->GetHandle();
	hSprite = pSprite->GetHandle();
}

bool Marker_t::IsTrainEntity(CBaseEntity* pEntity)
{	
	if (!pEntity)
		return false;

	const std::string strClassname = std::string(pEntity->GetClassname());
	if (strClassname.find("func_") != std::string::npos || strClassname.find("prop_dynamic") != std::string::npos || strClassname.find("path_") != std::string::npos
		|| strClassname.find("weapon_") != std::string::npos || strClassname.find("projectile_") != std::string::npos)
		return true;
	
	return false;
}

CON_COMMAND_CHAT(leader, "- test command")
{
	ZEPlayer* pPlayer = player->GetZEPlayer();
	if (g_pLeader->SetLeader(pPlayer))
		ClientPrintAll(HUD_PRINTTALK, LEADER_PREFIX "new leader incoming");
}