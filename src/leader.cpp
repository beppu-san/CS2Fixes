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
	pPlayer.SetLeader(true);

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

void Leader_PostEventAbstract_Source1LegacyGameEvent(const uint64* clients, const CNetMessage* pData)
{
	if (!g_bEnableLeader)
		return;

	auto pPBData = pData->ToPB<CMsgSource1LegacyGameEvent>();

	static int player_ping_id = g_gameEventManager->LookupEventId("player_ping");

	if (pPBData->eventid() != player_ping_id)
		return;

	// Don't kill ping visual when there's no leader, only mute the ping depending on cvar
	if (Leader_NoLeaders())
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
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		ZEPlayer* pPlayer = g_playerManager->GetPlayer((CPlayerSlot)i);

		if (pPlayer && !pPlayer->IsLeader())
			pPlayer->SetLeaderTracer(0);
	}

	g_iMarkerCount = 0;
}

// revisit this later with a TempEnt implementation
void Leader_BulletImpact(IGameEvent* pEvent)
{
	ZEPlayer* pPlayer = g_playerManager->GetPlayer(pEvent->GetPlayerSlot("userid"));

	if (!pPlayer)
		return;

	int iTracerIndex = pPlayer->GetLeaderTracer();

	if (!iTracerIndex)
		return;

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pEvent->GetPlayerPawn("userid");
	CBasePlayerWeapon* pWeapon = pPawn->m_pWeaponServices->m_hActiveWeapon.Get();

	CParticleSystem* particle = (CParticleSystem*)CreateEntityByName("info_particle_system");

	// Teleport particle to muzzle_flash attachment of player's weapon
	particle->AcceptInput("SetParent", "!activator", pWeapon, nullptr);
	particle->AcceptInput("SetParentAttachment", "muzzle_flash");

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	// Event contains other end of the particle
	Vector vecData = Vector(pEvent->GetFloat("x"), pEvent->GetFloat("y"), pEvent->GetFloat("z"));
	Color clTint = LeaderColorMap[iTracerIndex].clColor;

	pKeyValues->SetString("effect_name", "particles/cs2fixes/leader_tracer.vpcf");
	pKeyValues->SetInt("data_cp", 1);
	pKeyValues->SetVector("data_cp_value", vecData);
	pKeyValues->SetInt("tint_cp", 2);
	pKeyValues->SetColor("tint_cp_color", clTint);
	pKeyValues->SetBool("start_active", true);

	particle->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(particle, "DestroyImmediately", nullptr, nullptr, "", 0.1f);
	UTIL_AddEntityIOEvent(particle, "Kill", nullptr, nullptr, "", 0.12f);
}

CON_COMMAND_CHAT(glow, "<name> [duration] - toggle glow highlight on a player")
{
	int iPlayerSlot = player ? player->GetPlayerSlot() : -1;
	ZEPlayer* pPlayer = g_playerManager->GetPlayer((CPlayerSlot)iPlayerSlot);

	bool bIsAdmin;
	if (pPlayer)
		bIsAdmin = pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC);
	else // console
		bIsAdmin = true;

	Color color;
	int iDuration = 0;
	if (args.ArgC() == 3)
		iDuration = V_StringToInt32(args[2], 0);

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nTargetType = g_playerManager->TargetPlayerString(iPlayerSlot, args[1], iNumClients, pSlots);

	if (bIsAdmin) // Admin command logic
	{
		if (args.ArgC() < 2)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !glow <name> [duration]");
			return;
		}

		if (!iNumClients)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
			return;
		}

		const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

		for (int i = 0; i < iNumClients; i++)
		{
			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

			if (!pTarget)
				continue;

			if (pTarget->m_iTeamNum < CS_TEAM_T)
				continue;

			// Exception - Use LeaderIndex color if Admin is also a Leader
			if (pPlayer && pPlayer->IsLeader())
				color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;
			else
				color = pTarget->m_iTeamNum == CS_TEAM_T ? LeaderColorMap[2].clColor/*orange*/ : LeaderColorMap[1].clColor/*blue*/;

			ZEPlayer* pPlayerTarget = g_playerManager->GetPlayer(pSlots[i]);

			if (!pPlayerTarget->GetGlowModel())
				pPlayerTarget->StartGlow(color, iDuration);
			else
				pPlayerTarget->EndGlow();

			if (nTargetType < ETargetType::ALL)
				PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "toggled glow on", "", CHAT_PREFIX);
		}

		PrintMultiAdminAction(nTargetType, pszCommandPlayerName, "toggled glow on", "", CHAT_PREFIX);

		return;
	}

	// Leader command logic

	if (!g_bEnableLeader)
		return;

	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a Leader or an Admin to use this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !glow <name> [duration]");
		return;
	}

	if (player->m_iTeamNum != CS_TEAM_CT && g_bLeaderActionsHumanOnly)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a human to use this command.");
		return;
	}

	if (nTargetType > ETargetType::SELF)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must target a specific player.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one player fit the target name.");
		return;
	}

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);

	if (!pTarget)
		return;

	if (pTarget->m_iTeamNum != CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only place Leader glow on a human.");
		return;
	}

	color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;

	ZEPlayer* pPlayerTarget = g_playerManager->GetPlayer(pSlots[0]);

	if (!pPlayerTarget->GetGlowModel())
	{
		pPlayerTarget->StartGlow(color, iDuration);
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s enabled glow on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
	else
	{
		pPlayerTarget->EndGlow();
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s disabled glow on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
}

CON_COMMAND_CHAT(tracer, "<name> [color] - toggle projectile tracers on a player")
{
	if (!g_bEnableLeader)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	int iPlayerSlot = player->GetPlayerSlot();

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iPlayerSlot);

	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a leader to use this command.");
		return;
	}

	if (player->m_iTeamNum != CS_TEAM_CT && g_bLeaderActionsHumanOnly)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a human to use this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !tracer <name> [color]");
		return;
	}

	int iNumClients = 0;
	int pSlot[MAXPLAYERS];
	ETargetType nTargetType = g_playerManager->TargetPlayerString(iPlayerSlot, args[1], iNumClients, pSlot);

	if (nTargetType > ETargetType::SELF)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must target a specific player.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one player fit the target name.");
		return;
	}

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlot[0]);

	if (!pTarget)
		return;

	if (pTarget->m_iTeamNum != CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only toggle tracers on a human.");
		return;
	}

	ZEPlayer* pPlayerTarget = g_playerManager->GetPlayer(pSlot[0]);

	if (pPlayerTarget->GetLeaderTracer())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Disabled tracers for player %s.", pTarget->GetPlayerName());
		pPlayerTarget->SetLeaderTracer(0);
		return;
	}

	int iTracerIndex = 0;
	if (args.ArgC() < 3)
		iTracerIndex = pPlayer->GetLeaderIndex();
	else
	{
		int iIndex = V_StringToInt32(args[2], -1);

		if (iIndex > -1)
			iTracerIndex = MIN(iIndex, g_nLeaderColorMapSize - 1);
		else
		{
			for (int i = 0; i < g_nLeaderColorMapSize; i++)
			{
				if (!V_stricmp(args[2], LeaderColorMap[i].pszColorName))
				{
					iTracerIndex = i;
					break;
				}
			}
		}
	}

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Enabled tracers for player %s.", pTarget->GetPlayerName());
	pPlayerTarget->SetLeaderTracer(iTracerIndex);
}

CON_COMMAND_CHAT(beacon, "<name> [color] - toggle beacon on a player")
{
	int iPlayerSlot = player ? player->GetPlayerSlot() : -1;
	ZEPlayer* pPlayer = g_playerManager->GetPlayer((CPlayerSlot)iPlayerSlot);

	bool bIsAdmin;
	if (pPlayer)
		bIsAdmin = pPlayer->IsAdminFlagSet(ADMFLAG_GENERIC);
	else // console
		bIsAdmin = true;

	int iNumClients = 0;
	int pSlots[MAXPLAYERS];
	ETargetType nTargetType = g_playerManager->TargetPlayerString(iPlayerSlot, args[1], iNumClients, pSlots);

	if (bIsAdmin) // Admin beacon logic
	{
		if (args.ArgC() < 2)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !beacon <name> [color]");
			return;
		}

		if (!iNumClients)
		{
			ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
			return;
		}

		const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

		Color color;
		if (args.ArgC() == 3)
			color = Leader_ColorFromString(args[2]);

		for (int i = 0; i < iNumClients; i++)
		{
			CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);

			if (!pTarget)
				continue;

			if (pTarget->m_iTeamNum < CS_TEAM_T)
				continue;

			// Exception - Use LeaderIndex color if Admin is also a Leader
			if (args.ArgC() == 2 && pPlayer && pPlayer->IsLeader())
				color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;
			else if (args.ArgC() == 2)
				color = pTarget->m_iTeamNum == CS_TEAM_T ? LeaderColorMap[2].clColor/*orange*/ : LeaderColorMap[1].clColor/*blue*/;

			ZEPlayer* pPlayerTarget = g_playerManager->GetPlayer(pSlots[i]);

			if (!pPlayerTarget->GetBeaconParticle())
				pPlayerTarget->StartBeacon(color, pPlayer->GetHandle());
			else
				pPlayerTarget->EndBeacon();

			if (nTargetType < ETargetType::ALL)
				PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "toggled beacon on", "", CHAT_PREFIX);
		}

		PrintMultiAdminAction(nTargetType, pszCommandPlayerName, "toggled beacon on", "", CHAT_PREFIX);

		return;
	}

	// Leader beacon logic

	if (!g_bEnableLeader)
		return;

	if (!pPlayer->IsLeader())
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a Leader or an Admin to use this command.");
		return;
	}

	if (args.ArgC() < 2)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !beacon <name> [color]");
		return;
	}

	if (player->m_iTeamNum != CS_TEAM_CT && g_bLeaderActionsHumanOnly)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must be a human to use this command.");
		return;
	}

	if (nTargetType > ETargetType::SELF)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You must target a specific player.");
		return;
	}

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one player fit the target name.");
		return;
	}

	CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[0]);

	if (!pTarget)
		return;

	if (pTarget->m_iTeamNum != CS_TEAM_CT)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You can only place Leader beacon on a human.");
		return;
	}

	Color color;
	if (args.ArgC() == 3)
		color = Leader_ColorFromString(args[2]);
	else
		color = LeaderColorMap[pPlayer->GetLeaderIndex()].clColor;

	ZEPlayer* pPlayerTarget = g_playerManager->GetPlayer(pSlots[0]);

	if (!pPlayerTarget->GetBeaconParticle())
	{
		pPlayerTarget->StartBeacon(color, pPlayer->GetHandle());
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s enabled beacon on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
	else
	{
		pPlayerTarget->EndBeacon();
		ClientPrintAll(HUD_PRINTTALK, CHAT_PREFIX "Leader %s disabled beacon on %s.", player->GetPlayerName(), pTarget->GetPlayerName());
	}
}