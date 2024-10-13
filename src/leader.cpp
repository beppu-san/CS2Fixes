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
#include "gameevents.pb.h"
#include "zombiereborn.h"
#include "networksystem/inetworkmessages.h"

#include "tier0/memdbgon.h"

extern IVEngineServer2* g_pEngineServer2;
extern CGameEntitySystem* g_pEntitySystem;
extern CGlobalVars* gpGlobals;
extern IGameEventManager2* g_gameEventManager;

bool g_bEnableLeader = false;
static float g_flLeaderVoteRatio = 0.15f;
static bool g_bMutePingsIfNoLeader = true;

FAKE_BOOL_CVAR(cs2f_leader_enable, "Whether to enable Leader features", g_bEnableLeader, false, false)
FAKE_FLOAT_CVAR(cs2f_leader_vote_ratio, "Vote ratio needed for player to become a leader", g_flLeaderVoteRatio, 0.25f, false)
FAKE_BOOL_CVAR(cs2f_leader_mute_ping_no_leader, "Whether to mute player pings whenever there's no leader", g_bMutePingsIfNoLeader, true, false)

CUtlVector<CLeaderMarker> g_vecMarkers;
CUtlVector<CZRLeader> g_vecLeaders;

int Leader_GetRequiredVoteCounts()
{
	int iPlayers = 0;

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer(i);

		if (pPlayer && !pPlayer->IsFakeClient())
			iPlayers++;
	}

	return (int)(iPlayers * g_flLeaderVoteRatio) + 1;
}

void Leader_Precache(IEntityResourceManifest* pResourceManifest)
{
	pResourceManifest->AddResource("models/leader_model/marker.vmdl");
}

void Leader_PostEventAbstract_Source1LegacyGameEvent(const uint64* clients, const CNetMessage* pData)
{
	if (!g_bEnableLeader)
		return;

	auto pPBData = pData->ToPB<CMsgSource1LegacyGameEvent>();

	static int player_ping_id = g_gameEventManager->LookupEventId("player_ping");

	if (pPBData->eventid() != player_ping_id)
		return;

	// Don't kill ping visual when there's no leader, only mute the ping depending on cvar
	if (g_vecLeaders.Count() == 0)
	{
		if (g_bMutePingsIfNoLeader)
			*(uint64*)clients = 0;

		return;
	}

	IGameEvent* pEvent = g_gameEventManager->UnserializeEvent(*pPBData);

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(pEvent->GetPlayerSlot("userid"));
	CCSPlayerController* pController = CCSPlayerController::FromSlot(pEvent->GetPlayerSlot("userid"));
	CBaseEntity* pEntity = (CBaseEntity*)g_pEntitySystem->GetEntityInstance(pEvent->GetEntityIndex("entityid"));

	g_gameEventManager->FreeEvent(pEvent);

	// no reason to block zombie pings. sound affected by sound block cvar
	if (pController->m_iTeamNum == CS_TEAM_T)
	{
		if (g_bMutePingsIfNoLeader)
			*(uint64*)clients = 0;

		return;
	}

	// allow leader human pings
	if (pPlayer->IsLeader())
		return;

	// Remove entity responsible for visual part of the ping
	pEntity->Remove();

	// Block clients from playing the ping sound
	*(uint64*)clients = 0;
}

void Leader_OnRoundStart(IGameEvent* pEvent)
{
	FOR_EACH_VEC(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].GetPlayer()->GetLeaderType() == ELeaderType::TEMPORARY)
		{
			g_vecLeaders.Remove(i);
			break;
		}

		g_vecLeaders[i].RemoveAllMarkers();
	}
}

CZRLeader::CZRLeader(ZEPlayer* pPlayer, ELeaderType nType) :
	m_iButtonsPrevious(IN_NONE),
	m_iButtons(IN_NONE),
	m_bIgnoreAttack2Input(false),
	m_iPaintModeQueue(20),
	m_iMarkerIndex(0)
{
	if (!pPlayer || nType == ELeaderType::NONE)
		return;

	m_hPlayer = pPlayer->GetHandle();
	pPlayer->SetLeader(nType);

	new CTimer(0.01f, false, true, [this, pPlayer]() -> float
		{
			if (!pPlayer || pPlayer->GetLeaderType() == ELeaderType::NONE)
				return -1.f;

			if (!pPlayer->IsLeader())
				return 0.01f;

			CCSPlayerPawn* pPawn = (CCSPlayerPawn*)CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot())->GetPawn();

			if (pPlayer->IsInfected() || !pPawn->IsAlive())
				return 0.01f;

			uint64 iButtons = pPawn->m_pMovementServices()->m_nButtons().m_pButtonStates()[0];
			m_iButtonsPrevious = m_iButtons;
			m_iButtons = iButtons;

			if (m_iButtons & IN_ATTACK2)
			{
				if (m_iPaintModeQueue == 0)
				{
					//
				}
				else if (!(m_iButtonsPrevious & IN_ATTACK2))
				{
					if (m_iButtons & IN_SPEED)
					{
						RemoveAllMarkers();
						m_bIgnoreAttack2Input = true;
					}
					else if (m_iButtons & IN_DUCK)
					{
						m_bIgnoreAttack2Input = true;
					}

					if (!m_bIgnoreAttack2Input)
						m_iPaintModeQueue = m_iPaintModeQueue > 0 ? m_iPaintModeQueue - 1 : 0;
				}
			}
			else if (m_iButtonsPrevious & IN_ATTACK2)
			{
				if (m_iPaintModeQueue > 0 && !m_bIgnoreAttack2Input)
					PutMarker();

				m_iPaintModeQueue = 20;
				m_bIgnoreAttack2Input = false;
			}

			return 0.01f;
		});

	g_vecLeaders.AddToTail(*this);

	CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());

	const std::map<ELeaderType, const char*> mapLeaderNames =
	{
		{ELeaderType::HEAD, "HEAD Leader"},
		{ELeaderType::TEMPORARY, "TEMPORARY Leader"},
		{ELeaderType::ASSISTANT, "ASSISTANT Leader"},
	};

	ClientPrintAll(HUD_PRINTTALK, LEADER_PREFIX " \x0E" "%s \3has become the \5%s\3!", pController->GetPlayerName(), mapLeaderNames.find(nType)->second);
	ClientPrint(pController, HUD_PRINTTALK, LEADER_PREFIX " \x0EYou are currently the new %s!", mapLeaderNames.find(nType)->second);
	ClientPrint(pController, HUD_PRINTTALK, LEADER_PREFIX " \x0E[+attack2] \3to put a marker.");
	ClientPrint(pController, HUD_PRINTTALK, LEADER_PREFIX " \x0E[+attack2] + [+sprint] \3to clear ALL markers.");
}

CZRLeader::~CZRLeader()
{
	ZEPlayer* pPlayer = m_hPlayer.Get();

	if (!pPlayer)
		return;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());

	pPlayer->SetLeader(ELeaderType::NONE);

	ClientPrintAll(HUD_PRINTTALK, LEADER_PREFIX " \x0E" "%s \2no longer has the Leader access.", pController->GetPlayerName());
}

void CZRLeader::PutMarker()
{
	ZEPlayer* pPlayer = m_hPlayer.Get();

	if (!pPlayer)
		return;

	if (pPlayer->GetLeaderType() == ELeaderType::ASSISTANT)
		return;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(pPlayer->GetPlayerSlot());
	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pController->GetPawn();

	if (pPlayer->IsInfected() || !pPawn->IsAlive())
		return;

	FOR_EACH_VEC(g_vecMarkers, i)
	{
		if (g_vecMarkers[i].GetIndex() == m_iMarkerIndex)
		{
			g_vecMarkers[i].Remove();
			break;
		}
	}

	Vector vecOrigin;
	QAngle angRotate;
	CBaseEntity* pEntity = GetEntityAimed(vecOrigin, angRotate);

	new CLeaderMarker(vecOrigin, angRotate, m_iMarkerIndex, pEntity);

	const std::map<int, const char*> mapMarkerNames =
	{
		{0, " \x0F" "A\1"},
		{1, " \4B\1"},
		{2, " \x0C" "C\1"},
		{3, " \x09" "D\1"},
		{4, " \x0E" "E\1"},
		{5, " \x0B" "F\1"},
		{6, " \x10G\1"},
		{7, " \3H\1"},
	};

	ClientPrintAll(HUD_PRINTTALK, " \x0E" "\xE2\x98\x85 " "%s: \3Watch out for" "%s \3Marker!", pController->GetPlayerName(), mapMarkerNames.find(m_iMarkerIndex)->second);

	m_iMarkerIndex = (m_iMarkerIndex + 1) % 8;
}

void CZRLeader::RemoveAllMarkers()
{
	FOR_EACH_VEC(g_vecMarkers, i)
	{
		g_vecMarkers[i].Remove();
		i--;
	}

	m_iMarkerIndex = 0;
}

CBaseEntity* CZRLeader::GetEntityAimed(Vector& origin, QAngle& angles, bool bIsRecursive)
{
	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)CCSPlayerController::FromSlot(m_hPlayer.Get()->GetPlayerSlot())->GetPawn();

	Vector vecStart;
	if (bIsRecursive)
		vecStart = origin;
	else
	{
		vecStart = pPawn->GetAbsOrigin();
		vecStart.z += 64.f;
	}

	Vector vecForward;
	AngleVectors(pPawn->m_angEyeAngles(), &vecForward);
	vecForward.Normalized();

	Vector vecEnd = vecStart + vecForward * 65536.f;

	CTraceFilter* pFilter = new CTraceFilter();
	pFilter->m_nInteractsWith = 0x39312b;
	pFilter->m_nInteractsExclude = 0x48804;
	pFilter->m_nInteractsAs = 0x40000;
	pFilter->m_nObjectSetMask = RNQUERY_OBJECTS_ALL;
	pFilter->m_nCollisionGroup = COLLISION_GROUP_ALWAYS;
	pFilter->m_bHitSolid = true;
	pFilter->m_bHitSolidRequiresGenerateContacts = true;
	pFilter->SetPassEntityOwner1(static_cast<CEntityInstance*>(pPawn));

	trace_t nTrace;
	TraceShape(vecStart, vecEnd, pFilter, nTrace);

	if (nTrace.DidHit())
	{
		CUtlVector<CHandle<CBasePlayerWeapon>>* vecWeapons = pPawn->m_pWeaponServices() ? pPawn->m_pWeaponServices()->m_hMyWeapons() : nullptr;

		CBaseEntity* pEnt = static_cast<CBaseEntity*>(nTrace.m_pEnt);
		CGameSceneNode* pNode = pEnt->m_CBodyComponent()->m_pSceneNode()->m_pParent();

		while (pNode)
		{
			CBaseEntity* pParented = static_cast<CBaseEntity*>(pNode->m_pOwner());

			FOR_EACH_VEC(*vecWeapons, i)
			{
				CBasePlayerWeapon* pWeapon = (*vecWeapons)[i].Get();

				if (pWeapon && pWeapon == pParented)
				{
					vecStart = nTrace.m_vHitPoint + vecForward * 16.f;
					return GetEntityAimed(vecStart, angles, true);
				}
			}

			pNode = pParented->m_CBodyComponent()->m_pSceneNode()->m_pParent();
		}

		origin = nTrace.m_vHitPoint;

		VectorAngles(nTrace.m_vHitNormal, angles);
		angles.x += 90.f;

		return pEnt;
	}

	return nullptr;
}

CLeaderMarker::CLeaderMarker(const Vector& origin, const QAngle& angles, int iIndex, CBaseEntity* pParent) :
	m_iIndex(iIndex)
{
	struct Marker_t
	{
		Color clColor;
		const char* pszText;
	};

	const std::map<int, Marker_t> mapMarkers =
	{
		{0, {Color(255, 0, 0), "A"}},
		{1, {Color(0, 255, 0), "B"}},
		{2, {Color(0, 0, 255), "C"}},
		{3, {Color(255, 255, 0), "D"}},
		{4, {Color(255, 0, 255), "E"}},
		{5, {Color(0, 255, 255), "F"}},
		{6, {Color(255, 128, 0), "G"}},
		{7, {Color(128, 0, 255), "H"}},
	};

	Color color = mapMarkers.find(iIndex)->second.clColor;

	CBaseModelEntity* pModel = CreateEntityByName<CBaseModelEntity>("prop_dynamic_override");
	CEntityKeyValues* pKvModel = new CEntityKeyValues();
	pKvModel->SetString("model", "models/leader_model/marker.vmdl");
	pKvModel->SetString("DefaultAnim", "idle");
	pKvModel->SetInt("spawnflags", 256U);
	pKvModel->SetInt("disablerecieveshadows", 1);
	pKvModel->SetInt("disableshadows", 1);
	pKvModel->SetColor("rendercolor", Color(color.r(), color.g(), color.b(), 160));
	pKvModel->SetInt("rendermode", kRenderTransColor);
	pKvModel->SetInt("renderfx", kRenderFxPulseSlow);
	pKvModel->SetInt("solid", SOLID_NONE);
	pKvModel->SetInt("skin", 0);
	pKvModel->SetFloat("scale", 1.f);

	pModel->DispatchSpawn(pKvModel);
	pModel->Teleport(&origin, &angles, nullptr);

	CPointWorldText* pSprite = CreateEntityByName<CPointWorldText>("point_worldtext");
	CEntityKeyValues* pKvSprite = new CEntityKeyValues();
	pKvSprite->SetString("message", mapMarkers.find(iIndex)->second.pszText);
	pKvSprite->SetString("font_name", "Arial Black");
	pKvSprite->SetInt("font_size", 120);
	pKvSprite->SetColor("color", Color(color.r(), color.g(), color.b(), 255));
	pKvSprite->SetInt("justify_horizontal", POINT_WORLD_TEXT_JUSTIFY_HORIZONTAL_CENTER);
	pKvSprite->SetInt("justify_vertical", POINT_WORLD_TEXT_JUSTIFY_VERTICAL_CENTER);
	pKvSprite->SetInt("reorient_mode", POINT_WORLD_TEXT_REORIENT_AROUND_UP);
	pKvSprite->SetFloat("world_units_per_pixel", 0.25f);
	pKvSprite->SetBool("enabled", true);
	pKvSprite->SetBool("fullbright", true);

	QAngle buf = angles - QAngle(90.f, 0.f, 0.f);
	Vector vecForward;
	AngleVectors(buf, &vecForward);

	Vector vecOffset = origin + vecForward * 30.f;
	buf = QAngle(0.f, 0.f, 90.f);

	pSprite->DispatchSpawn(pKvSprite);
	pSprite->Teleport(&vecOffset, &buf, nullptr);

	new CTimer(0.f, false, false, [pModel, pSprite, pParent]() -> float
		{
			pSprite->SetParent(pModel);

			if (pParent)
				pModel->SetParent(pParent);

			return -1.f;
		});

	m_hModel.Set(pModel);
	m_hSprite.Set(pSprite);

	g_vecMarkers.AddToTail(*this);
}

void CLeaderMarker::Remove()
{
	CBaseModelEntity* pModel = m_hModel.Get();
	if (pModel)
		UTIL_AddEntityIOEvent(pModel, "Kill", nullptr, nullptr, 0.f);

	CPointWorldText* pSprite = m_hSprite.Get();
	if (pSprite)
		UTIL_AddEntityIOEvent(pModel, "Kill", nullptr, nullptr, 0.f);

	FOR_EACH_VEC(g_vecMarkers, i)
	{
		if (g_vecMarkers[i] == *this)
		{
			g_vecMarkers.Remove(i);
			break;
		}
	}
}

CON_COMMAND_CHAT_FLAGS(setleader, "<name> - Force a player to become a new HEAD Leader", ADMFLAG_BAN)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !setleader <name>");
		return;
	}

	FOR_EACH_VEC(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].GetPlayer()->GetLeaderType() == ELeaderType::HEAD)
		{
			ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "Head Leader has been already existed!");
			return;
		}
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_BOT | NO_IMMUNITY))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	new CZRLeader(pPlayerTarget, ELeaderType::HEAD);

	ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "You have assigned \3" "%s \1as the \5Head Leader\1.", pTarget->GetPlayerName());
}

CON_COMMAND_CHAT_FLAGS(removeleader, "<name> - Force a player to remove from the leader access", ADMFLAG_BAN)
{
	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !removeleader <name>");
		return;
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_BOT | NO_IMMUNITY))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	FOR_EACH_VEC(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].GetPlayer() == pPlayerTarget)
		{
			g_vecLeaders.Remove(i);

			ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "You have removed \3" "%s \1from the leader access.", pTarget->GetPlayerName());
			return;
		}
	}

	ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "Target does not have any leader access!");
}

CON_COMMAND_CHAT(vl, "<name> - Vote for a player to become a new leader")
{
	if (!player)
		return;

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !vl <name>");
		return;
	}

	FOR_EACH_VEC(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].GetPlayer()->GetLeaderType() == ELeaderType::HEAD)
		{
			ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "Head Leader has been already existed!");
			return;
		}
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_MULTIPLE | NO_BOT | NO_IMMUNITY))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	ZEPlayer* pPlayer = player->GetZEPlayer();
	ZEPlayer* pLastVoted = pPlayer->GetLastVotedPlayer();

	if (pLastVoted == pPlayerTarget)
	{
		ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "You have already voted for this player!");
		return;
	}

	if (pLastVoted)
		pLastVoted->RemoveLeaderVote(pPlayer);

	pPlayerTarget->AddLeaderVote(pPlayer);

	int iCurrentVotes = pPlayerTarget->GetLeaderVoteCount();
	int iRequiredVotes = Leader_GetRequiredVoteCounts();

	ClientPrintAll(HUD_PRINTTALK, LEADER_PREFIX " \3" "%s \1has voted for \3" "%s \1to become the new leader (%d/%d votes).", player->GetPlayerName(), pTarget->GetPlayerName(), iCurrentVotes, iRequiredVotes);

	if (iCurrentVotes >= iRequiredVotes)
	{
		new CZRLeader(pPlayerTarget, ELeaderType::HEAD);
		ClientPrintAll(HUD_PRINTTALK, LEADER_PREFIX " \3" "%s \1has been voted to \5HEAD Leader\1!", pTarget->GetPlayerName());

		pPlayerTarget->PurgeLeaderVotes();
	}
}

CON_COMMAND_CHAT(tl, "<name> - (only HEAD Leader available)")
{
	if (!player)
		return;

	ZEPlayer* pPlayer = player->GetZEPlayer();

	if (!pPlayer || pPlayer->GetLeaderType() != ELeaderType::HEAD)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You don't have access to this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !tl <name>");
		return;
	}

	FOR_EACH_VEC(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].GetPlayer()->GetLeaderType() == ELeaderType::TEMPORARY)
		{
			ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "TEMPORARY Leader has been already existed!");
			return;
		}
	}

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_SELF | NO_MULTIPLE | NO_BOT | NO_IMMUNITY))
		return;

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);
	CCSPlayerPawn* pPawnTarget = (CCSPlayerPawn*)pTarget->GetPawn();
	ZEPlayer* pPlayerTarget = pTarget->GetZEPlayer();

	if (pPlayerTarget->IsInfected() || !pPawnTarget->IsAlive())
	{
		ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "Target must be alive in Human!");
		return;
	}

	new CZRLeader(pPlayerTarget, ELeaderType::TEMPORARY);

	ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "You have assigned \3" "%s \1as the \5TEMPORARY Leader\1.", pTarget->GetPlayerName());
}

CON_COMMAND_CHAT(r, "- Resign from the leader access")
{
	if (!player)
		return;

	ZEPlayer* pPlayer = player->GetZEPlayer();

	if (pPlayer->GetLeaderType() == ELeaderType::NONE)
		return;

	FOR_EACH_VEC(g_vecLeaders, i)
	{
		if (g_vecLeaders[i].GetPlayer() == pPlayer)
		{
			g_vecLeaders.Remove(i);

			ClientPrint(player, HUD_PRINTTALK, LEADER_PREFIX "You resigned from the leader access!");
			return;
		}
	}
}