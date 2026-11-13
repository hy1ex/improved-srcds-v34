//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_ammodef.h"
#include "weapon_csbase.h"
#include "cs_shareddefs.h"
#include "KeyValues.h"


#ifdef CLIENT_DLL

	#include "networkstringtable_clientdll.h"

#else
	
	#include "bot.h"
	#include "utldict.h"
	#include "cs_player.h"
	#include "cs_team.h"
	#include "cs_gamerules.h"
	#include "voice_gamemgr.h"
	#include "igamesystem.h"
	#include "weapon_c4.h"
	#include "mapinfo.h"
	#include "shake.h"
	#include "mapentities.h"
	#include "game.h"
	#include "cs_simple_hostage.h"
	#include "cs_gameinterface.h"
	#include "player_resource.h"
	#include "info_view_parameters.h"
	#include "cs_bot_manager.h"
	#include "cs_bot.h"
	#include "eventqueue.h"
	#include "fmtstr.h"

	#include "cs_gamestats.h"
	#include "cs_urlretrieveprices.h"
	#include "networkstringtable_gamedll.h"
	#include "nav_mesh.h"
	#include "nav_area.h"
	#include "nav_pathfind.h"
	
#endif


#include "cs_blackmarket.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifndef CLIENT_DLL


#define CS_GAME_STATS_UPDATE 79200 //22 hours
#define CS_GAME_STATS_UPDATE_PERIOD 7200 // 2 hours

extern IUploadGameStats *gamestatsuploader;
#endif


/**
 * Player hull & eye position for standing, ducking, etc.  This version has a taller
 * player height, but goldsrc-compatible collision bounds.
 */
static CViewVectors g_CSViewVectors(
	Vector( 0, 0, 64 ),		// eye position

	Vector(-16, -16, 0 ),	// hull min
	Vector( 16,  16, 62 ),	// hull max

	Vector(-16, -16, 0 ),	// duck hull min
	Vector( 16,  16, 45 ),	// duck hull max
	Vector( 0, 0, 47 ),		// duck view

	Vector(-10, -10, -10 ),	// observer hull min
	Vector( 10,  10,  10 ),	// observer hull max

	Vector( 0, 0, 14 )		// dead view height
);


#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS(info_player_terrorist, CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_counterterrorist,CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_logo,CPointEntity);
// Deathmatch spawn points (CS:GO cs_gamerules.cpp:334). CS:GO's SpawnPoint is server-only, which
// is why a stock client never has to know about it; ours is a plain point entity for the same reason.
LINK_ENTITY_TO_CLASS(info_deathmatch_spawn,CPointEntity);
#endif

REGISTER_GAMERULES_CLASS( CCSGameRules );


BEGIN_NETWORK_TABLE_NOBASE( CCSGameRules, DT_CSGameRules )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bFreezePeriod ) ),
		RecvPropInt( RECVINFO( m_iRoundTime ) ),
		RecvPropFloat( RECVINFO( m_fRoundStartTime ) ),
		RecvPropFloat( RECVINFO( m_flGameStartTime ) ),
		RecvPropInt( RECVINFO( m_iHostagesRemaining ) ),
		RecvPropBool( RECVINFO( m_bMapHasBombTarget ) ),
		RecvPropBool( RECVINFO( m_bMapHasRescueZone ) ),
		RecvPropBool( RECVINFO( m_bLogoMap ) ),
		RecvPropBool( RECVINFO( m_bBlackMarket ) )
	#else
		SendPropBool( SENDINFO( m_bFreezePeriod ) ),
		SendPropInt( SENDINFO( m_iRoundTime ), 16 ),
		SendPropFloat( SENDINFO( m_fRoundStartTime ), 32, SPROP_NOSCALE ),
		SendPropFloat( SENDINFO( m_flGameStartTime ), 32, SPROP_NOSCALE ),
		SendPropInt( SENDINFO( m_iHostagesRemaining ), 4 ),
		SendPropBool( SENDINFO( m_bMapHasBombTarget ) ),
		SendPropBool( SENDINFO( m_bMapHasRescueZone ) ),
		SendPropBool( SENDINFO( m_bLogoMap ) ),
		SendPropBool( SENDINFO( m_bBlackMarket ) )
	#endif
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( cs_gamerules, CCSGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( CSGameRulesProxy, DT_CSGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_CSGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CCSGameRules *pRules = CSGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CCSGameRulesProxy, DT_CSGameRulesProxy )
		RecvPropDataTable( "cs_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_CSGameRules ), RecvProxy_CSGameRules )
	END_RECV_TABLE()
#else
	void* SendProxy_CSGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CCSGameRules *pRules = CSGameRules();
		Assert( pRules );
		return pRules;
	}

	BEGIN_SEND_TABLE( CCSGameRulesProxy, DT_CSGameRulesProxy )
		SendPropDataTable( "cs_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_CSGameRules ), SendProxy_CSGameRules )
	END_SEND_TABLE()
#endif



ConVar ammo_50AE_max( "ammo_50AE_max", "35", FCVAR_REPLICATED );
ConVar ammo_762mm_max( "ammo_762mm_max", "90", FCVAR_REPLICATED );
ConVar ammo_556mm_max( "ammo_556mm_max", "90", FCVAR_REPLICATED );
ConVar ammo_556mm_box_max( "ammo_556mm_box_max", "200", FCVAR_REPLICATED );
ConVar ammo_338mag_max( "ammo_338mag_max", "30", FCVAR_REPLICATED );
ConVar ammo_9mm_max( "ammo_9mm_max", "120", FCVAR_REPLICATED );
ConVar ammo_buckshot_max( "ammo_buckshot_max", "32", FCVAR_REPLICATED );
ConVar ammo_45acp_max( "ammo_45acp_max", "100", FCVAR_REPLICATED );
ConVar ammo_357sig_max( "ammo_357sig_max", "52", FCVAR_REPLICATED );
ConVar ammo_57mm_max( "ammo_57mm_max", "100", FCVAR_REPLICATED );
ConVar ammo_hegrenade_max( "ammo_hegrenade_max", "1", FCVAR_REPLICATED );
ConVar ammo_flashbang_max( "ammo_flashbang_max", "2", FCVAR_REPLICATED );
ConVar ammo_smokegrenade_max( "ammo_smokegrenade_max", "1", FCVAR_REPLICATED );

extern ConVar sv_stopspeed;

ConVar mp_dynamicpricing( 
	"mp_dynamicpricing", 
	"1", 
	FCVAR_REPLICATED, 
	"Enables or Disables the dynamic weapon prices" );

ConVar mp_buytime( 
	"mp_buytime", 
	"1.5",
	FCVAR_REPLICATED,
	"How many minutes after round start players can buy items for.",
	true, 0.25,
	false, 0 );

// ---------------------------------------------------------------------------------------------------
// Death drop control. Ported from CS:GO (game/shared/cstrike15/cs_gamerules.cpp). Server side only:
// none of these are FCVAR_REPLICATED, so a stock client never has to know they exist.
// Defaults reproduce stock Counter-Strike: Source behaviour exactly.
// ---------------------------------------------------------------------------------------------------
ConVar mp_death_drop_gun(
	"mp_death_drop_gun",
	"1",
	FCVAR_NOTIFY,
	"Which gun to drop on player death: 0=none, 1=best, 2=current or best",
	true, 0,
	true, 2 );

ConVar mp_death_drop_c4(
	"mp_death_drop_c4",
	"1",
	FCVAR_NOTIFY,
	"Whether the bomb is dropped when its carrier dies" );

ConVar mp_death_drop_defuser(
	"mp_death_drop_defuser",
	"1",
	FCVAR_NOTIFY,
	"Whether a defuse kit is dropped when its owner dies" );

ConVar mp_death_drop_grenade(
	"mp_death_drop_grenade",
	"1",
	FCVAR_NOTIFY,
	"Which grenade to drop on player death: 0=none, 1=best, 2=current or best, 3=all",
	true, 0,
	true, 3 );

// ---------------------------------------------------------------------------------------------------
// Defuse kit allocation. Ported from CS:GO (game/shared/cstrike15/cs_gamerules.cpp, which also
// defines DefuserAllocation in cs_gamerules.h). Server side only, never replicated.
// Default 0 reproduces stock Counter-Strike: Source behaviour (kits must be bought).
// ---------------------------------------------------------------------------------------------------
enum
{
	DEFUSER_ALLOCATION_NONE		= 0,
	DEFUSER_ALLOCATION_RANDOM	= 1,
	DEFUSER_ALLOCATION_ALL		= 2,
};

ConVar mp_defuser_allocation(
	"mp_defuser_allocation",
	"0",
	FCVAR_NOTIFY,
	"How to allocate defusers to CTs at round start: 0=none, 1=one random CT, 2=every CT",
	true, 0,
	true, 2 );

// ---------------------------------------------------------------------------------------------------
// Respawn on death. Ported from CS:GO (mp_respawn_on_death_ct/t, mp_use_respawn_waves,
// mp_respawnwavetime_ct/t, mp_respawn_immunitytime). Server side only, never replicated. Defaults
// keep the feature off, so stock Counter-Strike: Source behaviour is unchanged until an operator
// opts in.
// ---------------------------------------------------------------------------------------------------
ConVar mp_respawn_on_death_ct(
	"mp_respawn_on_death_ct",
	"0",
	FCVAR_NOTIFY,
	"Respawn dead Counter-Terrorists automatically (0 = stock CS:S behaviour)" );

ConVar mp_respawn_on_death_t(
	"mp_respawn_on_death_t",
	"0",
	FCVAR_NOTIFY,
	"Respawn dead Terrorists automatically (0 = stock CS:S behaviour)" );

ConVar mp_use_respawn_waves(
	"mp_use_respawn_waves",
	"0",
	FCVAR_NOTIFY,
	"When a team respawns on death: 0=immediately, 1=in waves of mp_respawnwavetime_ct/t, 2=all at once when the whole team is dead" );

ConVar mp_respawnwavetime_ct(
	"mp_respawnwavetime_ct",
	"10.0",
	FCVAR_NOTIFY,
	"Seconds a dead Counter-Terrorist waits before respawning" );

ConVar mp_respawnwavetime_t(
	"mp_respawnwavetime_t",
	"10.0",
	FCVAR_NOTIFY,
	"Seconds a dead Terrorist waits before respawning" );

ConVar mp_respawn_immunitytime(
	"mp_respawn_immunitytime",
	"4.0",
	FCVAR_NOTIFY,
	"Seconds of damage immunity after a spawn while the team respawns on death (0 = none); ends early once the player moves" );

ConVar mp_respawn_weapons(
	"mp_respawn_weapons",
	"0",
	FCVAR_NOTIFY,
	"Loadout after a respawn: 0=team default pistol, 1=random weapons, 2=weapons the player had" );

ConVar mp_respawn_immunity_fade(
	"mp_respawn_immunity_fade",
	"1",
	FCVAR_NOTIFY,
	"Visual for mp_respawn_immunitytime: 0=none, 1=fade in, 2=fixed half transparency, snap back" );

ConVar mp_buy_anywhere(
	"mp_buy_anywhere",
	"0",
	FCVAR_NOTIFY,
	"Buying outside buy zones: 0=stock CS:S, 1=both teams, 2=Terrorists, 3=Counter-Terrorists" );

ConVar mp_buy_during_immunity(
	"mp_buy_during_immunity",
	"0",
	FCVAR_NOTIFY,
	"Buy window is the spawn immunity window instead of mp_buytime: 0=off, 1=both teams, 2=Terrorists, 3=Counter-Terrorists" );

// ---------------------------------------------------------------------------------------------------
// Burn spread from an ignited player. See CCSGameRules::RadiusDamage() below.
// ---------------------------------------------------------------------------------------------------
ConVar mp_burn_radius_damage(
	"mp_burn_radius_damage",
	"0",
	FCVAR_NOTIFY,
	"Whether a burning player's flame damages entities around him: 0=no (fixes the ignite lag), 1=stock" );

// ---------------------------------------------------------------------------------------------------
// Damage scaling. Ported from CS:GO (mp_damage_scale_* and mp_damage_headshot_only). Server side
// only, never replicated: the scale is applied to the damage before armour, so a stock client only
// ever sees the resulting health. Defaults of 1.0 and 0 keep stock Counter-Strike: Source behaviour.
// ---------------------------------------------------------------------------------------------------
ConVar mp_damage_scale_ct_head(
	"mp_damage_scale_ct_head",
	"1.0",
	FCVAR_NOTIFY,
	"Scale of damage applied to the head of a Counter-Terrorist (1.0 = stock)" );

ConVar mp_damage_scale_ct_body(
	"mp_damage_scale_ct_body",
	"1.0",
	FCVAR_NOTIFY,
	"Scale of damage applied to the rest of the body of a Counter-Terrorist (1.0 = stock)" );

ConVar mp_damage_scale_t_head(
	"mp_damage_scale_t_head",
	"1.0",
	FCVAR_NOTIFY,
	"Scale of damage applied to the head of a Terrorist (1.0 = stock)" );

// Ported from CS:GO cs_gamerules.cpp:3390. Fraction of the health a victim loses that is healed back
// onto the attacker, capped at twice his own maximum health; 0 is stock, no healing at all.
ConVar mp_damage_vampiric_amount(
	"mp_damage_vampiric_amount",
	"0",
	FCVAR_NOTIFY,
	"Fraction of the health damage dealt that is given back to the attacker (0 = off, stock)" );

ConVar mp_damage_scale_t_body(
	"mp_damage_scale_t_body",
	"1.0",
	FCVAR_NOTIFY,
	"Scale of damage applied to the rest of the body of a Terrorist (1.0 = stock)" );

ConVar mp_damage_headshot_only(
	"mp_damage_headshot_only",
	"0",
	FCVAR_NOTIFY,
	"Only bullets that hit the head deal damage; explosions, fire, falling and knives still hurt (0 = stock damage model)" );

// ---------------------------------------------------------------------------------------------------
// Free armour at spawn. Ported from CS:GO (mp_free_armor). Server side only; armour is already a
// networked player field, so nothing new is sent to clients. Default 0 keeps stock CS:S, where
// armour can only be bought.
// ---------------------------------------------------------------------------------------------------
ConVar mp_free_armor(
	"mp_free_armor",
	"0",
	FCVAR_NOTIFY,
	"Armour every player gets when he spawns: 0=none (stock), 1=kevlar, 2=kevlar and helmet",
	true, 0,
	true, 2 );

// ---------------------------------------------------------------------------------------------------
// Purchase restrictions. Server side only: the decision is taken when a purchase arrives, so
// nothing is replicated. Every rule is per team, and only covers weapons that team can actually
// buy: the other team's weapons are already refused by their weapon script in
// CCSPlayer::HandleCommand_Buy(), so they have no bit here. The defaults allow everything stock
// CS:S allows, so nothing changes until an operator opts in.
// ---------------------------------------------------------------------------------------------------
ConVar mp_buy_allow_guns(
	"mp_buy_allow_guns",
	"1",
	FCVAR_NOTIFY,
	"Which teams may buy guns: 0=nobody, 1=both teams (stock), 2=Terrorists only, 3=CT only",
	true, 0,
	true, 3 );

ConVar mp_max_armor_ct(
	"mp_max_armor_ct",
	"2",
	FCVAR_NOTIFY,
	"Highest armour CT may buy: 0=none, 1=kevlar, 2=kevlar and helmet (2 = stock)",
	true, 0,
	true, 2 );

ConVar mp_max_armor_t(
	"mp_max_armor_t",
	"2",
	FCVAR_NOTIFY,
	"Highest armour Terrorists may buy: 0=none, 1=kevlar, 2=kevlar and helmet (2 = stock)",
	true, 0,
	true, 2 );

ConVar mp_buy_allow_grenades_ct(
	"mp_buy_allow_grenades_ct",
	"7",
	FCVAR_NOTIFY,
	"Grenades CT may buy, as a bitfield: HE 1, flashbang 2, smoke 4 (7 = all)",
	true, 0,
	true, 7 );

ConVar mp_buy_allow_grenades_t(
	"mp_buy_allow_grenades_t",
	"7",
	FCVAR_NOTIFY,
	"Grenades Terrorists may buy, as a bitfield: HE 1, flashbang 2, smoke 4 (7 = all)",
	true, 0,
	true, 7 );

ConVar mp_buy_allow_pistols_ct(
	"mp_buy_allow_pistols_ct",
	"15",
	FCVAR_NOTIFY,
	"Pistols CT may buy, as a bitfield: USP 1, P228 2, Desert Eagle 4, Five-SeveN 8 (15 = all)",
	true, 0,
	true, 15 );

ConVar mp_buy_allow_pistols_t(
	"mp_buy_allow_pistols_t",
	"15",
	FCVAR_NOTIFY,
	"Pistols Terrorists may buy, as a bitfield: Glock 1, P228 2, Desert Eagle 4, Dual Berettas 8 (15 = all)",
	true, 0,
	true, 15 );

ConVar mp_buy_allow_smgs_ct(
	"mp_buy_allow_smgs_ct",
	"15",
	FCVAR_NOTIFY,
	"Sub machine guns CT may buy, as a bitfield: TMP 1, MP5 Navy 2, UMP-45 4, P90 8 (15 = all)",
	true, 0,
	true, 15 );

ConVar mp_buy_allow_smgs_t(
	"mp_buy_allow_smgs_t",
	"15",
	FCVAR_NOTIFY,
	"Sub machine guns Terrorists may buy, as a bitfield: MAC-10 1, MP5 Navy 2, UMP-45 4, P90 8 (15 = all)",
	true, 0,
	true, 15 );

ConVar mp_buy_allow_rifles_ct(
	"mp_buy_allow_rifles_ct",
	"7",
	FCVAR_NOTIFY,
	"Rifles CT may buy, as a bitfield: FAMAS 1, M4A1 2, AUG 4 (7 = all)",
	true, 0,
	true, 7 );

ConVar mp_buy_allow_rifles_t(
	"mp_buy_allow_rifles_t",
	"7",
	FCVAR_NOTIFY,
	"Rifles Terrorists may buy, as a bitfield: Galil 1, AK-47 2, SG552 4 (7 = all)",
	true, 0,
	true, 7 );

ConVar mp_buy_allow_shotguns_ct(
	"mp_buy_allow_shotguns_ct",
	"3",
	FCVAR_NOTIFY,
	"Shotguns CT may buy, as a bitfield: M3 1, XM1014 2 (3 = all)",
	true, 0,
	true, 3 );

ConVar mp_buy_allow_shotguns_t(
	"mp_buy_allow_shotguns_t",
	"3",
	FCVAR_NOTIFY,
	"Shotguns Terrorists may buy, as a bitfield: M3 1, XM1014 2 (3 = all)",
	true, 0,
	true, 3 );

ConVar mp_buy_allow_snipers_ct(
	"mp_buy_allow_snipers_ct",
	"7",
	FCVAR_NOTIFY,
	"Sniper rifles CT may buy, as a bitfield: Scout 1, AWP 2, SG550 4 (7 = all)",
	true, 0,
	true, 7 );

ConVar mp_buy_allow_snipers_t(
	"mp_buy_allow_snipers_t",
	"7",
	FCVAR_NOTIFY,
	"Sniper rifles Terrorists may buy, as a bitfield: Scout 1, AWP 2, G3SG1 4 (7 = all)",
	true, 0,
	true, 7 );

ConVar mp_buy_allow_machine_guns_ct(
	"mp_buy_allow_machine_guns_ct",
	"1",
	FCVAR_NOTIFY,
	"Machine guns CT may buy, as a bitfield: M249 1 (1 = all)",
	true, 0,
	true, 1 );

ConVar mp_buy_allow_machine_guns_t(
	"mp_buy_allow_machine_guns_t",
	"1",
	FCVAR_NOTIFY,
	"Machine guns Terrorists may buy, as a bitfield: M249 1 (1 = all)",
	true, 0,
	true, 1 );

// ---------------------------------------------------------------------------------------------------
// Halftime. Ported from CS:GO (mp_halftime). Server side only: team membership, the team scores and
// money are already networked, so a stock client follows the swap without any new message or netprop.
// Default 0 keeps stock CS:S behaviour, where the sides never change.
// ---------------------------------------------------------------------------------------------------
ConVar mp_halftime(
	"mp_halftime",
	"0",
	FCVAR_NOTIFY,
	"Swap the teams halfway through mp_maxrounds (0 = stock CS:S, never swap)" );

// ---------------------------------------------------------------------------------------------------
// Collision control. Ported from CS:GO's mp_solid_teammates, generalised to one switch for every
// player plus the equivalent switches for hostages and thrown grenades. Server side only, never
// replicated. Every switch defaults to -1, which never writes a collision group at all: the group
// the engine assigns stays, so the behaviour is stock, and a plugin is free to put an individual
// player, hostage or grenade in any group it likes. 0 makes the entity non-solid, 1 restores the
// engine's own group. A collision group belongs to the entity, so a player cannot be non-solid to
// teammates
// only without the stock client pushing him out of the body it still predicts: there is one
// player switch, not one per team.
// ---------------------------------------------------------------------------------------------------
ConVar mp_solid_player(
	"mp_solid_player",
	"-1",
	FCVAR_NOTIFY,
	"Whether players block each other's movement (-1 = never touch collision groups, 0 = walk through every player, 1 = players block)",
	true, -1,
	true, 1 );

ConVar mp_solid_hostages(
	"mp_solid_hostages",
	"-1",
	FCVAR_NOTIFY,
	"Whether hostages block player movement (-1 = never touch collision groups, 0 = walk through hostages, 1 = engine default)",
	true, -1,
	true, 1 );

ConVar mp_solid_grenades(
	"mp_solid_grenades",
	"-1",
	FCVAR_NOTIFY,
	"Whether thrown grenade projectiles block player movement (-1 = never touch collision groups, 0 = walk through them, 1 = engine default)",
	true, -1,
	true, 1 );

ConVar mp_teammates_are_enemies(
	"mp_teammates_are_enemies",
	"0",
	FCVAR_NOTIFY,
	"Treat teammates as enemies: friendly fire is always on and teammates count as enemies" );

ConVar mp_players_nonsolid_time(
	"mp_players_nonsolid_time",
	"0.0",
	FCVAR_NOTIFY,
	"Seconds after a spawn/respawn during which a player does not block or get blocked by players" );

// ---------------------------------------------------------------------------------------------------
// Hostage rules. Ported from CS:GO's mp_hostages_takedamage, mp_hostages_rescuetowin,
// mp_hostages_run_speed_modifier and mp_only_cts_rescue_hostages. Server side only, never replicated.
// Every default reproduces stock Counter-Strike: Source: hostages can be shot, every surviving
// hostage has to be rescued, they move at their stock speed and only Counter-Terrorists may take one.
// ---------------------------------------------------------------------------------------------------
ConVar mp_hostages_takedamage(
	"mp_hostages_takedamage",
	"1",
	FCVAR_NOTIFY,
	"Whether hostages can be hurt (1 = stock, 0 = invulnerable)" );

ConVar mp_hostages_rescuetowin(
	"mp_hostages_rescuetowin",
	"0",
	FCVAR_NOTIFY,
	"Number of hostages the CTs must rescue to win the round (0 = every surviving hostage, stock)" );

ConVar mp_hostages_run_speed_modifier(
	"mp_hostages_run_speed_modifier",
	"1.0",
	FCVAR_NOTIFY,
	"Scale on a following hostage's movement force (1.0 = stock, lower slows hostages down)",
	true, 0.1f,
	true, 1.5f );

ConVar mp_only_cts_rescue_hostages(
	"mp_only_cts_rescue_hostages",
	"1",
	FCVAR_NOTIFY,
	"Only Counter-Terrorists may take a hostage (1 = stock, 0 = Terrorists may too)" );

ConVar mp_hostages_max(
	"mp_hostages_max",
	"0",
	FCVAR_NOTIFY,
	"Maximum number of the map's hostages to keep in play (0 = all of them, stock)" );

ConVar mp_hostages_spawn_farthest(
	"mp_hostages_spawn_farthest",
	"0",
	FCVAR_NOTIFY,
	"When limiting hostages, keep the set that is spread out furthest across the map instead of a random one" );

ConVar mp_hostages_spawn_same_every_round(
	"mp_hostages_spawn_same_every_round",
	"1",
	FCVAR_NOTIFY,
	"Keep the same set of hostages for the whole match (1, CS:GO default) or pick a new set every round (0)" );

ConVar mp_hostages_spawn_force_positions(
	"mp_hostages_spawn_force_positions",
	"",
	FCVAR_NOTIFY,
	"Comma separated list of zero based hostage indices to force, e.g. '0,2' or '1,6'. Empty = no forcing." );

// ---------------------------------------------------------------------------------------------------
// C4 rules. Ported from CS:GO's mp_anyone_can_pickup_c4, mp_c4_cannot_be_defused and
// mp_plant_c4_anywhere. Server side only and every default is stock Counter-Strike: Source: only a
// Terrorist may pick the bomb up, anyone can defuse it once planted, and it can only be planted
// inside a func_bomb_target.
// ---------------------------------------------------------------------------------------------------
ConVar mp_anyone_can_pickup_c4(
	"mp_anyone_can_pickup_c4",
	"0",
	FCVAR_NOTIFY,
	"Any player may pick up the dropped C4, not only a Terrorist (0 = stock)" );

ConVar mp_c4_cannot_be_defused(
	"mp_c4_cannot_be_defused",
	"0",
	FCVAR_NOTIFY,
	"Once planted, the C4 cannot be defused by anyone (0 = stock, CTs can defuse)" );

ConVar mp_plant_c4_anywhere(
	"mp_plant_c4_anywhere",
	"0",
	FCVAR_NOTIFY,
	"The C4 may be planted outside a bomb target (0 = stock, bomb zones only)" );

// ---------------------------------------------------------------------------------------------------
// Economy control. Ported from CS:GO's mp_maxmoney / mp_afterroundmoney / mp_playercashawards /
// mp_teamcashawards. Server side only, never replicated. Defaults are the stock CS:S numbers, so a
// server that does not touch them behaves exactly like stock.
// ---------------------------------------------------------------------------------------------------
ConVar mp_maxmoney(
	"mp_maxmoney",
	"16000",
	FCVAR_NOTIFY,
	"Maximum amount of money a player can carry",
	true, 0,
	false, 0 );

ConVar mp_afterroundmoney(
	"mp_afterroundmoney",
	"0",
	FCVAR_NOTIFY,
	"Flat amount of money every player receives with the round's cash, whatever the result (0 = stock CS:S)",
	true, 0,
	false, 0 );

ConVar mp_playercashawards(
	"mp_playercashawards",
	"1",
	FCVAR_NOTIFY,
	"Whether players earn individual cash rewards such as kill awards (default: yes, stock CS:S)" );

// ---------------------------------------------------------------------------------------------------
// cash_* awards. Ported from CS:GO's cash_player_* / cash_team_* family (cs_gamerules.cpp:16089-16220),
// which in CS:GO replaces the amounts CS:S hardcodes in PlayerKilled(), the round end checks,
// RestartRound() and the hostage code. Every default is the value this tree already paid, so nothing
// changes until an operator edits a convar; the CS:GO defaults that differ are noted per convar.
// ---------------------------------------------------------------------------------------------------
ConVar cash_player_killed_enemy_default(
	"cash_player_killed_enemy_default",
	"300",
	FCVAR_NOTIFY,
	"Cash for a kill made without a weapon CS:GO tracks (CS:GO default 300, stock CS:S 300)" );

ConVar cash_player_killed_enemy_factor(
	"cash_player_killed_enemy_factor",
	"0",
	FCVAR_NOTIFY,
	"Award the killed-with weapon's kill award scaled by this instead of the flat default. 0 = stock flat award.",
	true, 0.0f,
	false, 0 );

ConVar cash_player_killed_teammate(
	"cash_player_killed_teammate",
	"-3300",
	FCVAR_NOTIFY,
	"Cash change for killing a teammate (CS:GO default -300, stock CS:S -3300)" );

ConVar cash_player_damage_hostage(
	"cash_player_damage_hostage",
	"-20",
	FCVAR_NOTIFY,
	"Cash per point of damage done to a hostage (CS:GO default -30, stock CS:S -20 per point)" );

ConVar cash_player_killed_hostage(
	"cash_player_killed_hostage",
	"-500",
	FCVAR_NOTIFY,
	"Cash for killing a hostage; the damage term is added unless mp_cash_hostage_kill_flat is 1 (CS:GO default -1000)" );

ConVar mp_cash_hostage_kill_flat(
	"mp_cash_hostage_kill_flat",
	"0",
	FCVAR_NOTIFY,
	"0 = stock CS:S hostage kill penalty of cash_player_killed_hostage minus 20 per damage point, 1 = the convar alone (CS:GO style)",
	true, 0,
	true, 1 );

ConVar cash_player_interact_with_hostage(
	"cash_player_interact_with_hostage",
	"150",
	FCVAR_NOTIFY,
	"Cash for the player who takes a hostage (CS:GO default 150, stock CS:S 150)" );

ConVar cash_player_rescued_hostage(
	"cash_player_rescued_hostage",
	"1000",
	FCVAR_NOTIFY,
	"Cash for the player who brings a hostage to the rescue zone (CS:GO default 1000, stock CS:S 1000)" );

ConVar cash_player_bomb_planted(
	"cash_player_bomb_planted",
	"0",
	FCVAR_NOTIFY,
	"Cash for the player who plants the bomb (CS:GO default 300, stock CS:S none)" );

ConVar cash_player_bomb_defused(
	"cash_player_bomb_defused",
	"0",
	FCVAR_NOTIFY,
	"Cash for the player who defuses the bomb (CS:GO default 300, stock CS:S none)" );

ConVar cash_player_get_killed(
	"cash_player_get_killed",
	"0",
	FCVAR_NOTIFY,
	"Cash a player receives when he is killed (CS:GO default 0, stock CS:S none)" );

ConVar cash_player_respawn_amount(
	"cash_player_respawn_amount",
	"0",
	FCVAR_NOTIFY,
	"Cash a player receives when he respawns mid-round (CS:GO default 0, stock CS:S none)" );

ConVar cash_team_terrorist_win_bomb(
	"cash_team_terrorist_win_bomb",
	"3500",
	FCVAR_NOTIFY,
	"Team cash for the Terrorists when the bomb explodes (CS:GO default 3500, stock CS:S 3500)" );

ConVar cash_team_win_by_defusing_bomb(
	"cash_team_win_by_defusing_bomb",
	"3250",
	FCVAR_NOTIFY,
	"Team cash for the CTs when they defuse the bomb (CS:GO default 3250, stock CS:S 3250)" );

ConVar cash_team_planted_bomb_but_defused(
	"cash_team_planted_bomb_but_defused",
	"800",
	FCVAR_NOTIFY,
	"Team cash for the Terrorists when their bomb is defused (CS:GO default 800, stock CS:S 800)" );

ConVar cash_team_win_by_hostage_rescue(
	"cash_team_win_by_hostage_rescue",
	"2500",
	FCVAR_NOTIFY,
	"Team cash for the CTs when every hostage is rescued (CS:GO default 3500, stock CS:S 2500)" );

ConVar cash_team_win_by_time_running_out_bomb(
	"cash_team_win_by_time_running_out_bomb",
	"3250",
	FCVAR_NOTIFY,
	"Team cash for the CTs when the round time runs out on a bomb map (CS:GO default 3250, stock CS:S 3250)" );

ConVar cash_team_win_by_time_running_out_hostage(
	"cash_team_win_by_time_running_out_hostage",
	"3250",
	FCVAR_NOTIFY,
	"Team cash for the Terrorists when the round time runs out on a hostage map (CS:GO default 3250, stock CS:S 3250)" );

ConVar cash_team_elimination_bomb_map(
	"cash_team_elimination_bomb_map",
	"3250",
	FCVAR_NOTIFY,
	"Team cash for winning by elimination on a bomb map (CS:GO default 3250, stock CS:S 3250)" );

ConVar cash_team_elimination_hostage_map_ct(
	"cash_team_elimination_hostage_map_ct",
	"3000",
	FCVAR_NOTIFY,
	"Team cash for the CTs winning by elimination on a hostage map (CS:GO default 2000, stock CS:S 3000)" );

ConVar cash_team_elimination_hostage_map_t(
	"cash_team_elimination_hostage_map_t",
	"3000",
	FCVAR_NOTIFY,
	"Team cash for the Terrorists winning by elimination on a hostage map (CS:GO default 1000, stock CS:S 3000)" );

ConVar cash_team_loser_bonus(
	"cash_team_loser_bonus",
	"1400",
	FCVAR_NOTIFY,
	"Base cash for the losing team (CS:GO default 1400, stock CS:S 1400)" );

ConVar cash_team_loser_bonus_consecutive_rounds(
	"cash_team_loser_bonus_consecutive_rounds",
	"500",
	FCVAR_NOTIFY,
	"Extra cash for the losing team per consecutive loss, capped at 3000 as in stock CS:S (CS:GO default 500)" );

ConVar cash_team_winner_bonus_consecutive_rounds(
	"cash_team_winner_bonus_consecutive_rounds",
	"0",
	FCVAR_NOTIFY,
	"Extra cash for the winning team while the other team is on a losing streak (CS:GO default 0)" );

ConVar cash_team_hostage_alive(
	"cash_team_hostage_alive",
	"150",
	FCVAR_NOTIFY,
	"Team cash the winner gets per hostage still alive, capped at 2000 as in stock CS:S (CS:GO default 0, stock CS:S 150)" );

ConVar cash_team_rescued_hostage(
	"cash_team_rescued_hostage",
	"750",
	FCVAR_NOTIFY,
	"Team cash the CTs get per hostage rescued during the round (CS:GO default 0, stock CS:S 750)" );

ConVar cash_team_hostage_interaction(
	"cash_team_hostage_interaction",
	"100",
	FCVAR_NOTIFY,
	"Team cash the CTs get when one of them takes a hostage (CS:GO default 500, stock CS:S 100)" );

// mp_give_player_c4 (CS:GO cs_gamerules.cpp:2442, checked at :9652). CS:S always hands the bomb out on
// a bomb map; this lets a server play those maps without the objective.
ConVar mp_give_player_c4(
	"mp_give_player_c4",
	"1",
	FCVAR_NOTIFY,
	"Whether the map should give a player the C4 at round start (1 = stock, 0 = no bomb)",
	true, 0,
	true, 1 );

ConVar mp_teamcashawards(
	"mp_teamcashawards",
	"1",
	FCVAR_NOTIFY,
	"Whether players earn team win/loss cash at round start (default: yes, stock CS:S)" );

ConVar mp_playerid(
	"mp_playerid",
	"0",
	FCVAR_REPLICATED,
	"Controls what information player see in the status bar: 0 all names; 1 team names; 2 no names",
	true, 0,
	true, 2 );

ConVar mp_playerid_delay(
	"mp_playerid_delay",
	"0.5",
	FCVAR_REPLICATED,
	"Number of seconds to delay showing information in the status bar",
	true, 0,
	true, 1 );

ConVar mp_playerid_hold(
	"mp_playerid_hold",
	"0.25",
	FCVAR_REPLICATED,
	"Number of seconds to keep showing old information in the status bar",
	true, 0,
	true, 1 );
	
ConVar mp_fadetoblack( 
	"mp_fadetoblack", 
	"0", 
	FCVAR_REPLICATED, 
	"fade a player's screen to black when he dies" );

ConVar nextlevel( 
	"nextlevel", 
	"", 
	FCVAR_REPLICATED | FCVAR_NOTIFY,
	"If set, will trigger a changelevel to the specified map at the end of the round" );
	
#ifdef CLIENT_DLL

ConVar cl_autowepswitch(
	"cl_autowepswitch",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Automatically switch to picked up weapons (if more powerful)" );

ConVar cl_autohelp(
	"cl_autohelp",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Auto-help" );

#else

	// longest the intermission can last, in seconds
	#define MAX_INTERMISSION_TIME 120

	// Falling damage stuff.
	#define CS_PLAYER_FATAL_FALL_SPEED		1100	// approx 60 feet
	#define CS_PLAYER_MAX_SAFE_FALL_SPEED	580		// approx 20 feet
	#define CS_DAMAGE_FOR_FALL_SPEED		((float)100 / ( CS_PLAYER_FATAL_FALL_SPEED - CS_PLAYER_MAX_SAFE_FALL_SPEED )) // damage per unit per second.

	// These entities are preserved each round restart. The rest are removed and recreated.
	static const char *s_PreserveEnts[] =
	{
		"ai_network",
		"ai_hint",
		"cs_gamerules",
		"cs_team_manager",
		"cs_player_manager",
		"env_soundscape",
		"env_soundscape_proxy",
		"env_soundscape_triggerable",
		"env_sun",
		"env_wind",
		"env_fog_controller",
		"func_brush",
		"func_wall",
		"func_buyzone",
		"func_illusionary",
		"func_hostage_rescue",
		"func_bomb_target",
		"infodecal",
		"info_projecteddecal",
		"info_node",
		"info_target",
		"info_node_hint",
		"info_player_counterterrorist",
		"info_player_terrorist",
		"info_deathmatch_spawn",
		"info_map_parameters",
		"keyframe_rope",
		"move_rope",
		"info_ladder",
		"player",
		"point_viewcontrol",
		"scene_manager",
		"shadow_control",
		"sky_camera",
		"soundent",
		"trigger_soundscape",
		"viewmodel",
		"predicted_viewmodel",
		"worldspawn",
		"point_devshot_camera",
		"", // END Marker
	};


	// --------------------------------------------------------------------------------------------------- //
	// Voice helper
	// --------------------------------------------------------------------------------------------------- //

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker )
		{
			// Dead players can only be heard by other dead team mates
			if ( pTalker->IsAlive() == false )
			{
				if ( pListener->IsAlive() == false )
					return ( pListener->InSameTeam( pTalker ) );

				return false;
			}

			return ( pListener->InSameTeam( pTalker ) );
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;



	// --------------------------------------------------------------------------------------------------- //
	// Globals.
	// --------------------------------------------------------------------------------------------------- //

	// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
	char *sTeamNames[] =
	{
		"Unassigned",
		"Spectator",
		"TERRORIST",
		"CT"
	};

	ConVar mp_autoteambalance( 
		"mp_autoteambalance", 
		"1", 
		FCVAR_NOTIFY );
		
	ConVar mp_maxrounds( 
		"mp_maxrounds", 
		"0", 
		FCVAR_REPLICATED | FCVAR_NOTIFY, 
		"max number of rounds to play before server changes maps", 
		true, 0, 
		false, 0 );
		
	ConVar mp_winlimit( 
		"mp_winlimit", 
		"0", 
		FCVAR_REPLICATED, 
		"max number of rounds one team can win before server changes maps", 
		true, 0, 
		false, 0 );

	ConVar mp_startmoney( 
		"mp_startmoney", 
		"800", 
		FCVAR_REPLICATED,
		"amount of money each player gets when they reset",
		true, 0,
		false, 0 );

	ConVar mp_roundtime( 
		"mp_roundtime",
		"5",
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"How many minutes each round takes.",
		true, 1,	// min value
		true, 9		// max value
		);

	// CS:GO round time per objective type plus its hostage rescue bonus (ref cs_gamerules.cpp:3148-3164,
	// 1903-1907). 0 in either mp_roundtime_* convar means "use mp_roundtime", which is the stock
	// behaviour, and mp_hostages_rescuetime only ever adds time to a hostage round.
	ConVar mp_roundtime_hostage(
		"mp_roundtime_hostage",
		"0",
		FCVAR_NOTIFY,
		"How many minutes each round of Hostage Rescue takes. If 0 then use mp_roundtime instead.",
		true, 0,	// min value
		true, 60	// max value
		);

	ConVar mp_roundtime_defuse(
		"mp_roundtime_defuse",
		"0",
		FCVAR_NOTIFY,
		"How many minutes each round of Bomb Defuse takes. If 0 then use mp_roundtime instead.",
		true, 0,	// min value
		true, 60	// max value
		);

	ConVar mp_hostages_rescuetime(
		"mp_hostages_rescuetime",
		"1",
		FCVAR_NOTIFY,
		"Additional time added to round time if a hostage is reached by a CT.",
		true, 0,	// min value
		true, 60	// max value
		);

	// CS:GO's fall damage controls (ref cs_gamerules.cpp:212-215). sv_falldamage_scale replaces the
	// dead mp_falldamage convar; the below-player pair only engages once the ratio is above 0.
	ConVar sv_falldamage_scale(
		"sv_falldamage_scale",
		"1",
		FCVAR_NOTIFY,
		"Scale applied to the damage from falling" );

	ConVar sv_falldamage_to_below_player_ratio(
		"sv_falldamage_to_below_player_ratio",
		"0",
		FCVAR_NOTIFY,
		"Landing on another player's head gives them this ratio of the damage." );

	ConVar sv_falldamage_to_below_player_multiplier(
		"sv_falldamage_to_below_player_multiplier",
		"1",
		FCVAR_NOTIFY,
		"Scale damage when distributed across two players" );

	// CS:GO's per-match gun purchase cap (ref cs_player_shared.cpp:3278-3303). -1 is unlimited,
	// which keeps the stock CS:S behaviour of buying the same gun as often as the money allows.
	ConVar mp_weapons_max_gun_purchases_per_weapon_per_match(
		"mp_weapons_max_gun_purchases_per_weapon_per_match",
		"-1",
		FCVAR_NOTIFY,
		"Max number of times a player may purchase any weapon per match",
		true, -1,	// min value
		true, 10	// max value
		);

	ConVar mp_freezetime( 
		"mp_freezetime",
		"6",
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"how many seconds to keep players frozen when the round starts",
		true, 0,	// min value
		true, 60	// max value
		);

	ConVar mp_c4timer( 
		"mp_c4timer", 
		"45", 
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"how long from when the C4 is armed until it blows",
		true, 10,	// min value
		true, 90	// max value
		);

	ConVar mp_limitteams( 
		"mp_limitteams", 
		"2", 
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"Max # of players 1 team can have over another (0 disables check)",
		true, 0,	// min value
		true, 30	// max value
		);

	ConVar mp_tkpunish( 
		"mp_tkpunish", 
		"0", 
		FCVAR_REPLICATED,
		"Will a TK'er be punished in the next round?  {0=no,  1=yes}" );

	ConVar mp_autokick(
		"mp_autokick",
		"1",
		FCVAR_REPLICATED,
		"Kick idle/team-killing players" );

	ConVar mp_spawnprotectiontime(
		"mp_spawnprotectiontime",
		"5",
		FCVAR_REPLICATED,
		"Kick players who team-kill within this many seconds of a round restart." );

	ConVar mp_restartgame( 
		"mp_restartgame", 
		"0", 
		FCVAR_GAMEDLL, 
		"If non-zero, game will restart in the specified number of seconds" );
		
	ConVar mp_humanteam( 
		"mp_humanteam", 
		"any", 
		FCVAR_REPLICATED,
		"Restricts human players to a single team {any, CT, T}" );
		
	static ConVar mp_ignore_round_win_conditions( 
		"mp_ignore_round_win_conditions", 
		"0", 
		FCVAR_NONE, 
		"Ignore conditions which would end the current round" );
		
	static ConVar mp_round_restart_delay( 
		"mp_round_restart_delay", 
		"5.0", 
		FCVAR_NONE, 
		"Number of seconds to delay before restarting a round after a win" );

	ConCommand EndRound( "endround", &CCSGameRules::EndRound, "End the current round.", FCVAR_CHEAT );

	// --------------------------------------------------------------------------------------------------- //
	// Deathmatch random spawning (ported from CS:GO cs_gamerules.cpp:18693)
	//
	// CS:GO replicates these because its client can draw deathmatch spawn overlays. We have no client
	// support for deathmatch, so they stay server-side and the stock v34 client never sees them.
	// --------------------------------------------------------------------------------------------------- //
	ConVar mp_randomspawn(
		"mp_randomspawn",
		"0",
		FCVAR_NOTIFY,
		"Determines whether players spawn at random deathmatch spawn points. 0 = default; 1 = both teams; 2 = Terrorists; 3 = CTs.",
		true, 0,
		true, 3 );

	ConVar mp_randomspawn_los(
		"mp_randomspawn_los",
		"1",
		FCVAR_NONE,
		"Only pick random spawn points that the enemy team cannot see",
		true, 0,
		true, 1 );

	ConVar mp_randomspawn_dist(
		"mp_randomspawn_dist",
		"0",
		FCVAR_NONE,
		"Minimum distance an enemy must be from a random spawn point. 0 disables the test.",
		true, 0,
		false, 0 );

	// Disconnect handling (CS:GO cs_gamerules.cpp:1957 and :1963). CS:GO kills a living player
	// before the disconnect completes so the death drops, the kill feed and the stats all happen as
	// they would for any other death. Both default to 0, which is stock CS:S: a disconnect removes
	// the player without a kill.
	ConVar mp_disconnect_kills_players(
		"mp_disconnect_kills_players",
		"0",
		FCVAR_NOTIFY,
		"When a player disconnects, kill them first (triggering item drops, stats, etc.)",
		true, 0,
		true, 1 );

	ConVar mp_disconnect_kills_bots(
		"mp_disconnect_kills_bots",
		"0",
		FCVAR_NOTIFY,
		"When a bot disconnects, kill them first. Requires mp_disconnect_kills_players.",
		true, 0,
		true, 1 );

	// Deathmatch kill scoring (CS:GO cs_gamerules.cpp:2346 and :18728). CS:GO has no switch for it
	// because its game mode decides; there is no game mode here, so mp_dm_scoring is that switch and
	// defaults to off, which keeps stock Counter-Strike: Source scoring.
	ConVar mp_dm_scoring(
		"mp_dm_scoring",
		"0",
		FCVAR_NOTIFY,
		"Award CS:GO deathmatch points for kills. 0 = stock frags, 1 = mp_dm_kill_base_score per kill with a weapon price bonus.",
		true, 0,
		true, 1 );

	ConVar mp_dm_kill_base_score(
		"mp_dm_kill_base_score",
		"10",
		FCVAR_NONE,
		"Number of base points to award for a kill in deathmatch. Cheaper weapons award 1 or 2 additional points.",
		true, 0,
		false, 0 );


	// Deathmatch spawn point generation, defined further down next to the other spawn helpers.
	static void MaintainDMSpawnPopulation( void );
	static void ResetDMSpawnState( void );

	// --------------------------------------------------------------------------------------------------- //
	// Global helper functions.
	// --------------------------------------------------------------------------------------------------- //

	void InitBodyQue(void)
	{
		// FIXME: Make this work
	}


	Vector DropToGround( 
		CBaseEntity *pMainEnt, 
		const Vector &vPos, 
		const Vector &vMins, 
		const Vector &vMaxs )
	{
		trace_t trace;
		UTIL_TraceHull( vPos, vPos + Vector( 0, 0, -500 ), vMins, vMaxs, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );
		return trace.endpos;
	}


	//-----------------------------------------------------------------------------
	// Purpose: This function can be used to find a valid placement location for an entity.
	//			Given an origin to start looking from and a minimum radius to place the entity at,
	//			it will sweep out a circle around vOrigin and try to find a valid spot (on the ground)
	//			where mins and maxs will fit.
	// Input  : *pMainEnt - Entity to place
	//			&vOrigin - Point to search around
	//			fRadius - Radius to search within
	//			nTries - Number of tries to attempt
	//			&mins - mins of the Entity
	//			&maxs - maxs of the Entity
	//			&outPos - Return point
	// Output : Returns true and fills in outPos if it found a spot.
	//-----------------------------------------------------------------------------
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround )
	{
		// This function moves the box out in each dimension in each step trying to find empty space like this:
		//
		//											  X  
		//							   X			  X  
		// Step 1:   X     Step 2:    XXX   Step 3: XXXXX
		//							   X 			  X  
		//											  X  
		//
			 
		Vector mins, maxs;
		pMainEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		mins -= pMainEnt->GetAbsOrigin();
		maxs -= pMainEnt->GetAbsOrigin();

		// Put some padding on their bbox.
		float flPadSize = 5;
		Vector vTestMins = mins - Vector( flPadSize, flPadSize, flPadSize );
		Vector vTestMaxs = maxs + Vector( flPadSize, flPadSize, flPadSize );

		// First test the starting origin.
		if ( UTIL_IsSpaceEmpty( pMainEnt, vOrigin + vTestMins, vOrigin + vTestMaxs ) )
		{
			if ( bDropToGround )
			{
				outPos = DropToGround( pMainEnt, vOrigin, vTestMins, vTestMaxs );
			}
			else
			{
				outPos = vOrigin;
			}
			return true;
		}

		Vector vDims = vTestMaxs - vTestMins;

		// Keep branching out until we get too far.
		int iCurIteration = 0;
		int nMaxIterations = 15;
		
		int offset = 0;
		do
		{
			for ( int iDim=0; iDim < 3; iDim++ )
			{
				float flCurOffset = offset * vDims[iDim];

				for ( int iSign=0; iSign < 2; iSign++ )
				{
					Vector vBase = vOrigin;
					vBase[iDim] += (iSign*2-1) * flCurOffset;
				
					if ( UTIL_IsSpaceEmpty( pMainEnt, vBase + vTestMins, vBase + vTestMaxs ) )
					{
						// Ensure that there is a clear line of sight from the spawnpoint entity to the actual spawn point.
						// (Useful for keeping things from spawning behind walls near a spawn point)
						trace_t tr;
						UTIL_TraceLine( vOrigin, vBase, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &tr );

						if ( tr.fraction != 1.0 )
						{
							continue;
						}
						
						if ( bDropToGround )
							outPos = DropToGround( pMainEnt, vBase, vTestMins, vTestMaxs );
						else
							outPos = vBase;

						return true;
					}
				}
			}

			++offset;
		} while ( iCurIteration++ < nMaxIterations );

	//	Warning( "EntityPlacementTest for ent %d:%s failed!\n", pMainEnt->entindex(), pMainEnt->GetClassname() );
		return false;
	}

	int UTIL_HumansInGame( bool ignoreSpectators )
	{
		int iCount = 0;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *entity = CCSPlayer::Instance( i );

			if ( entity && !FNullEnt( entity->edict() ) )
			{
				if ( FStrEq( entity->GetPlayerName(), "" ) )
					continue;

				if ( FBitSet( entity->GetFlags(), FL_FAKECLIENT ) )
					continue;

				if ( ignoreSpectators && entity->GetTeamNumber() != TEAM_TERRORIST && entity->GetTeamNumber() != TEAM_CT )
					continue;

				if ( ignoreSpectators && entity->State_Get() == STATE_PICKINGCLASS )
					continue;

				iCount++;
			}
		}

		return iCount;
	}

	// --------------------------------------------------------------------------------------------------- //
	// CCSGameRules implementation.
	// --------------------------------------------------------------------------------------------------- //

	CCSGameRules::CCSGameRules()
	{
		m_iRoundTime = 0;
		m_iRoundWinStatus = WINNER_NONE;
		m_iFreezeTime = 0;

		m_fRoundStartTime = 0;
		m_bAllowWeaponSwitch = true;
		m_bFreezePeriod = true;
		m_iNumTerrorist = m_iNumCT = 0;	// number of players per team
		m_flRestartRoundTime = 0.1f; // restart first round as soon as possible
		m_iDefuserCarrier = 0;
		m_flNextRespawnWave[0] = 0.0f;
		m_flNextRespawnWave[1] = 0.0f;
		m_iNumSpawnableTerrorist = m_iNumSpawnableCT = 0;
		m_bFirstConnected = false;
		m_bCompleteReset = false;
		m_iAccountTerrorist = m_iAccountCT = 0;
		m_iNumCTWins = 0;
		m_iNumTerroristWins = 0;
		m_iNumConsecutiveCTLoses = 0;
		m_iNumConsecutiveTerroristLoses = 0;
		m_bTargetBombed = false;
		m_bBombDefused = false;
		m_iTotalRoundsPlayed = -1;
		m_iUnBalancedRounds = 0;
		m_flGameStartTime = 0;
		m_iHostagesRemaining = 0;
		m_bLevelInitialized = false;
		m_bLogoMap = false;
		m_tmNextPeriodicThink = 0;

		m_bMapHasBombTarget = false;
		m_bMapHasRescueZone = false;

		m_iSpawnPointCount_Terrorist = 0;
		m_iSpawnPointCount_CT = 0;

		m_bTCantBuy = false;
		m_bCTCantBuy = false;
		m_bMapHasBuyZone = false;

		m_iLoserBonus = 0;

		m_iHostagesRescued = 0;
		m_iHostagesTouched = 0;
		m_flNextHostageAnnouncement = 0.0f;

		m_iHaveEscaped = 0;
		m_bMapHasEscapeZone = false;
		m_iNumEscapers = 0;
		m_iNumEscapeRounds = 0;

		m_iMapHasVIPSafetyZone = 0;
		m_pVIP = NULL;
		m_iConsecutiveVIP = 0;

		m_bMapHasBombZone = false;
		m_bBombDropped = false;
		m_bBombPlanted = false;
		m_pLastBombGuy = NULL;

		m_bAllowWeaponSwitch = true;

		m_flNextHostageAnnouncement = gpGlobals->curtime;	// asap.

		nextlevel.SetValue( "" );

		ReadMultiplayCvars();

		m_pPrices = NULL;
		m_bBlackMarket = false;
		m_bDontUploadStats = false;

		// Create the team managers
		for ( int i = 0; i < ARRAYSIZE( sTeamNames ); i++ )
		{
			CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "cs_team_manager" ));
			pTeam->Init( sTeamNames[i], i );

			g_Teams.AddToTail( pTeam );
		}

		if ( filesystem->FileExists( UTIL_VarArgs( "maps/cfg/%s.cfg", STRING(gpGlobals->mapname) ) ) )
		{
			// Execute a map specific cfg file - as in Day of Defeat
			engine->ServerCommand( UTIL_VarArgs( "exec %s.cfg */maps\n", STRING(gpGlobals->mapname) ) );
			engine->ServerExecute();
		}

#ifndef CLIENT_DLL
		// stats

		if ( g_flGameStatsUpdateTime == 0.0f )
		{
			memset( g_iWeaponPurchases, 0, sizeof( g_iWeaponPurchases) );
			memset( g_iTerroristVictories, 0, sizeof( g_iTerroristVictories) );
			memset( g_iCounterTVictories, 0, sizeof( g_iTerroristVictories) );
			g_flGameStatsUpdateTime = CS_GAME_STATS_UPDATE_PERIOD; //Next update is between 22 and 24 hours.
		}
#endif
	}

	void CCSGameRules::AddPricesToTable( weeklyprice_t prices )
	{
		int iIndex = m_StringTableBlackMarket->FindStringIndex( "blackmarket_prices" );

		if ( iIndex == INVALID_STRING_INDEX )
		{
			m_StringTableBlackMarket->AddString( "blackmarket_prices", sizeof( weeklyprice_t), &prices );
		}
		else
		{
			m_StringTableBlackMarket->SetStringUserData( iIndex, sizeof( weeklyprice_t), &prices );
		}

		SetBlackMarketPrices( false );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CCSGameRules::~CCSGameRules()
	{
		// Note, don't delete each team since they are in the gEntList and will 
		// automatically be deleted from there, instead.
		g_Teams.Purge();
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CCSGameRules::UpdateClientData( CBasePlayer *player )
	{
	}

	//-----------------------------------------------------------------------------
	// Purpose: TF2 Specific Client Commands
	// Input  :
	// Output :
	//-----------------------------------------------------------------------------
	bool CCSGameRules::ClientCommand( const char* command, CBaseEntity *pEdict )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pEdict );

		if ( FStrEq( command, "changeteam" ) )
		{
			return true;
		}
		else if( pPlayer->ClientCommand( command ) )
		{
			return true;
		}
		else if( BaseClass::ClientCommand( command, pEdict ) )
		{
			return true;
		}
		else if ( TheBots->ServerCommand( command ) )
		{
			return true;
		}
		else
		{
			return TheBots->ClientCommand( pPlayer, command );
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------
	void CCSGameRules::PlayerSpawn( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pBasePlayer );
		if ( !pPlayer )
			Error( "PlayerSpawn" );

		if ( pPlayer->State_Get() != STATE_ACTIVE )
			return;

		pPlayer->EquipSuit();

		// A random kit is one kit per round, so CS:GO strips whatever defuser a player still carries
		// before it hands the round's kit out below (CCSGameRules::PlayerSpawn).
		if ( mp_defuser_allocation.GetInt() == DEFUSER_ALLOCATION_RANDOM )
			pPlayer->RemoveDefuser();

		bool addDefault = true;

		CBaseEntity	*pWeaponEntity = NULL;
		while ( ( pWeaponEntity = gEntList.FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != NULL )
		{
			if ( addDefault )
			{
				// remove all our weapons and armor before touching the first game_player_equip
				pPlayer->RemoveAllItems( true );
			}
			pWeaponEntity->Touch( pPlayer );
			addDefault = false;
		}


		if ( addDefault || pPlayer->m_bIsVIP )
			pPlayer->GiveDefaultItems();

		// Free armour (mp_free_armor). Applied after the loadout so it also reaches players on maps
		// that hand out their items through game_player_equip, and it tops a surviving player back up
		// to full armour instead of only filling an empty vest.
		const int nFreeArmor = mp_free_armor.GetInt();

		if ( nFreeArmor > 0 )
		{
			pPlayer->SetArmorValue( 100 );

			if ( nFreeArmor >= 2 )
				pPlayer->m_bHasHelmet = true;
		}

		// Defuse kit allocation (mp_defuser_allocation). Only meaningful on bomb defuse maps and
		// only for CTs; with 1 ("one random CT") just the player picked at round start gets one.
		if ( IsBombDefuseMap() && pPlayer->GetTeamNumber() == TEAM_CT )
		{
			const int nAllocation = mp_defuser_allocation.GetInt();

			if ( nAllocation == DEFUSER_ALLOCATION_ALL ||
				 ( nAllocation == DEFUSER_ALLOCATION_RANDOM && m_iDefuserCarrier == pPlayer->entindex() ) )
			{
				pPlayer->GiveDefuser();
			}
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Pick the CT who receives the round's single free defuse kit, when
	//			mp_defuser_allocation is 1 ("one random CT"). Called once per round restart.
	//-----------------------------------------------------------------------------
	void CCSGameRules::AllocateDefuserForRound()
	{
		m_iDefuserCarrier = 0;

		if ( !IsBombDefuseMap() || mp_defuser_allocation.GetInt() != DEFUSER_ALLOCATION_RANDOM )
			return;

		int nCandidates = 0;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

			if ( !pPlayer || pPlayer->GetTeamNumber() != TEAM_CT )
				continue;

			if ( pPlayer->PlayerClass() < FIRST_CT_CLASS || pPlayer->PlayerClass() > LAST_CT_CLASS )
				continue;

			// Reservoir sampling: every CT ends up the carrier with equal probability.
			if ( random->RandomInt( 0, nCandidates ) == 0 )
				m_iDefuserCarrier = pPlayer->entindex();

			++nCandidates;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Length of one respawn wave for a team. With mp_use_respawn_waves 0 there are no
	//			waves at all and CS:GO returns 0 here, which is what makes a respawn instant.
	//-----------------------------------------------------------------------------
	float CCSGameRules::GetRespawnWaveMaxLength( int iTeam ) const
	{
		if ( mp_use_respawn_waves.GetInt() == 0 )
			return 0.0f;

		if ( iTeam == TEAM_CT )
			return mp_respawnwavetime_ct.GetFloat();

		if ( iTeam == TEAM_TERRORIST )
			return mp_respawnwavetime_t.GetFloat();

		return 0.0f;
	}

	//-----------------------------------------------------------------------------
	// Purpose: mp_use_respawn_waves, the CS:GO respawn loop. A team has its own wave clock; every
	//			time the clock has run out the whole team comes back together and the clock is pushed
	//			forward by one wave. Mode 2 instead waits until the team is wiped before it starts the
	//			next wave, so a player who dies next to a living teammate stays down.
	//-----------------------------------------------------------------------------
	void CCSGameRules::CheckRespawnWaves( void )
	{
		const bool bCTsRespawn = mp_respawn_on_death_ct.GetBool();
		const bool bTsRespawn = mp_respawn_on_death_t.GetBool();

		if ( !bCTsRespawn && !bTsRespawn )
			return;

		// The round restart respawns everybody anyway, and a freeze period must not have players
		// coming back behind the freeze.
		if ( IsIntermission() || IsFreezePeriod() )
			return;

		for ( int nTeam = TEAM_TERRORIST; nTeam <= TEAM_CT; nTeam++ )
		{
			if ( nTeam == TEAM_CT ? !bCTsRespawn : !bTsRespawn )
				continue;

			const int nWave = ( nTeam == TEAM_CT ) ? 0 : 1;

			if ( mp_use_respawn_waves.GetInt() == 2 )
			{
				// Respawn when the whole team is dead, not on the clock.
				bool bTeamAlive = false;

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

					if ( pPlayer && pPlayer->GetTeamNumber() == nTeam && pPlayer->IsAlive() )
					{
						bTeamAlive = true;
						break;
					}
				}

				if ( bTeamAlive )
					continue;
			}

			if ( m_flNextRespawnWave[nWave] > gpGlobals->curtime )
				continue;

			m_flNextRespawnWave[nWave] = gpGlobals->curtime + GetRespawnWaveMaxLength( nTeam );

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

				if ( !pPlayer || pPlayer->GetTeamNumber() != nTeam || pPlayer->IsAlive() )
					continue;

				pPlayer->DoRespawn();
			}
		}
	}

	void CCSGameRules::BroadcastSound( const char *sound, int team )
	{
		CBroadcastRecipientFilter filter;
		filter.MakeReliable();

		if( team != -1 )
		{
			filter.RemoveAllRecipients();
			filter.AddRecipientsByTeam( GetGlobalTeam(team) );
		}

		UserMessageBegin ( filter, "SendAudio" );
			WRITE_STRING( sound );
		MessageEnd();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------

	// return a multiplier that should adjust the damage done by a blast at position vecSrc to something at the position
	// vecEnd.  This will take into account the density of an entity that blocks the line of sight from one position to
	// the other.
	//
	// this algorithm was taken from the HL2 version of RadiusDamage.
	float CCSGameRules::GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pEntityToIgnore)
	{
		float retval = 0.0;
		trace_t tr;

		UTIL_TraceLine(vecSrc, vecEnd, MASK_SHOT, pEntityToIgnore, COLLISION_GROUP_NONE, &tr);
		if (tr.fraction == 1.0)
		{
			retval = 1.0;
		}
		else if (!(tr.DidHitWorld()) && (tr.m_pEnt != NULL) && (tr.m_pEnt != pEntityToIgnore) && (tr.m_pEnt->GetOwnerEntity() != pEntityToIgnore))
		{
			// if we didn't hit world geometry perhaps there's still damage to be done here.

			CBaseEntity *blockingEntity = tr.m_pEnt;

			// check to see if this part of the player is visible if entities are ignored.
			UTIL_TraceLine(vecSrc, vecEnd, CONTENTS_SOLID, NULL, COLLISION_GROUP_NONE, &tr);

			if (tr.fraction == 1.0)
			{
				if ((blockingEntity != NULL) && (blockingEntity->VPhysicsGetObject() != NULL))
				{
					int nMaterialIndex = blockingEntity->VPhysicsGetObject()->GetMaterialIndex();

					float flDensity;
					float flThickness;
					float flFriction;
					float flElasticity;

					physprops->GetPhysicsProperties( nMaterialIndex, &flDensity,
						&flThickness, &flFriction, &flElasticity );

					const float DENSITY_ABSORB_ALL_DAMAGE = 3000.0;
					float scale = flDensity / DENSITY_ABSORB_ALL_DAMAGE;
					if ((scale >= 0.0) && (scale < 1.0))
					{
						retval = 1.0 - scale;
					}
					else if (scale < 0.0)
					{
						// should never happen, but just in case.
						retval = 1.0;
					}
				}
				else
				{
					retval = 0.75; // we're blocked by something that isn't an entity with a physics module or world geometry, just cut damage in half for now.
				}
			}
		}

		return retval;
	}

	// returns the percentage of the player that is visible from the given point in the world.
	// return value is between 0 and 1.
	float CCSGameRules::GetAmountOfEntityVisible(Vector & vecSrc, CBaseEntity *entity)
	{
		float retval = 0.0;

		const float damagePercentageChest = 0.40;
		const float damagePercentageHead = 0.20;
		const float damagePercentageFeet = 0.20;
		const float damagePercentageRightSide = 0.10;
		const float damagePercentageLeftSide = 0.10;

		if (!(entity->IsPlayer()))
		{
			// the entity is not a player, so the damage is all or nothing.
			Vector vecTarget;
			vecTarget = entity->BodyTarget(vecSrc, false);

			return GetExplosionDamageAdjustment(vecSrc, vecTarget, entity);
		}

		CCSPlayer *player = (CCSPlayer *)entity;

		// check what parts of the player we can see from this point and modify the return value accordingly.
		float chestHeightFromFeet;

		float armDistanceFromChest = HalfHumanWidth;

		// calculate positions of various points on the target player's body
		Vector vecFeet = player->GetAbsOrigin();

		Vector vecChest = player->BodyTarget(vecSrc, false);
		chestHeightFromFeet = vecChest.z - vecFeet.z;  // compute the distance from the chest to the feet. (this accounts for ducking and the like)

		Vector vecHead = player->GetAbsOrigin();
		vecHead.z += HumanHeight;

		Vector vecRightFacing;
		AngleVectors(player->GetAbsAngles(), NULL, &vecRightFacing, NULL);

		vecRightFacing.NormalizeInPlace();
		vecRightFacing = vecRightFacing * armDistanceFromChest;

		Vector vecLeftSide = player->GetAbsOrigin();
		vecLeftSide.x -= vecRightFacing.x;
		vecLeftSide.y -= vecRightFacing.y;
		vecLeftSide.z += chestHeightFromFeet;

		Vector vecRightSide = player->GetAbsOrigin();
		vecRightSide.x += vecRightFacing.x;
		vecRightSide.y += vecRightFacing.y;
		vecRightSide.z += chestHeightFromFeet;

		// check chest
		float damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecChest, entity);
		retval += (damagePercentageChest * damageAdjustment);

		// check top of head
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecHead, entity);
		retval += (damagePercentageHead * damageAdjustment);

		// check feet
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecFeet, entity);
		retval += (damagePercentageFeet * damageAdjustment);

		// check left "edge"
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecLeftSide, entity);
		retval += (damagePercentageLeftSide * damageAdjustment);

		// check right "edge"
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecRightSide, entity);
		retval += (damagePercentageRightSide * damageAdjustment);

		return retval;
	}

	void CCSGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity * pEntityIgnore )
	{
		// Napalm lag fix (the same thing the community "Napalm Lag Fix" SourceMod plugin detours
		// into this very function for). An ignited player carries a CEntityFlame that calls
		// RadiusDamage() every FLAME_DAMAGE_INTERVAL (0.2 s) with the burning player himself as
		// pEntityIgnore, so the fire he is carrying keeps running a full radius pass for as long as
		// he burns. This class's pass is the expensive one: unlike the HL2 default it probes each
		// entity in the sphere several times for its explosion adjustment, and the overload below
		// drops pEntityIgnore on the floor entirely, so the pass also keeps dealing burn damage back
		// to the burning player and to everyone standing on top of him. With several ignited players
		// mashed together that repeated cost is enough to hitch or even time the server out, and it
		// is what the plugin blocks. Dropping these passes is the fix: the burning player still
		// takes his FLAME_DIRECT_DAMAGE every tick, only the spread to the neighbourhood goes away.
		// mp_burn_radius_damage 1 restores the stock radius spread for servers that want it.
		if ( !mp_burn_radius_damage.GetBool() && pEntityIgnore && pEntityIgnore->IsPlayer() &&
			 ( info.GetDamageType() & DMG_BURN ) )
		{
			return;
		}

		RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore, false );
	}

	// Add the ability to ignore the world trace
	void CCSGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		flAdjustedDamage, falloff, damagePercentage;
		Vector		vecSpot;
		Vector		vecToTarget;
		Vector		vecEndPos;

		vecEndPos.Init();

		Vector vecSrc = vecSrcIn;

		damagePercentage = 1.0;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;

		int bInWater = (UTIL_PointContents ( vecSrc ) & MASK_WATER) ? true : false;
		
		vecSrc.z += 1;// in case grenade is lying on the ground

		// iterate on all entities in the vicinity.
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
		{
			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				// UNDONE: this should check a damage mask, not an ignore
				if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
				{// houndeyes don't hurt other houndeyes with their attack
					continue;
				}

				// blasts don't travel into or out of water
				if ( !bIgnoreWorld )
				{
					if (bInWater && pEntity->GetWaterLevel() == 0)
						continue;
					if (!bInWater && pEntity->GetWaterLevel() == 3)
						continue;
				}

				// radius damage can only be blocked by the world
				vecSpot = pEntity->BodyTarget( vecSrc );



				bool bHit = false;

				if( bIgnoreWorld )
				{
					vecEndPos = vecSpot;
					bHit = true;
				}
				else
				{
					// get the percentage of the target entity that is visible from the
					// explosion position.
					damagePercentage = GetAmountOfEntityVisible(vecSrc, pEntity);
					if (damagePercentage > 0.0)
					{
						vecEndPos = vecSpot;

						bHit = true;
					}
				}

				if ( bHit )
				{
					// the explosion can 'see' this entity, so hurt them!
					//vecToTarget = ( vecSrc - vecEndPos );
					vecToTarget = ( vecEndPos - vecSrc );

					// decrease damage for an ent that's farther from the bomb.
					flAdjustedDamage = vecToTarget.Length() * falloff;
					flAdjustedDamage = info.GetDamage() - flAdjustedDamage;
					flAdjustedDamage = flAdjustedDamage * damagePercentage;
				
					if ( flAdjustedDamage > 0 )
					{
						CTakeDamageInfo adjustedInfo = info;
						adjustedInfo.SetDamage( flAdjustedDamage );

						Vector dir = vecToTarget;
						VectorNormalize( dir );

						// If we don't have a damage force, manufacture one
						if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
						{
							CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc, 1.5	/* explosion scale! */ );
						}
						else
						{
							// Assume the force passed in is the maximum force. Decay it based on falloff.
							float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
							adjustedInfo.SetDamageForce( dir * flForce );
							adjustedInfo.SetDamagePosition( vecSrc );
						}

						Vector vecTarget;
						vecTarget = pEntity->BodyTarget(vecSrc, false);

						UTIL_TraceLine(vecSrc, vecTarget, MASK_SHOT, NULL, COLLISION_GROUP_NONE, &tr);

						if (tr.fraction != 1.0)
						{
							// this has to be done to make breakable glass work.
							ClearMultiDamage( );
							pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
							ApplyMultiDamage();
						}
						else
						{
							pEntity->TakeDamage( adjustedInfo );
						}
			
						// Now hit all triggers along the way that respond to damage... 
						pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, vecEndPos, dir );
					}
				}
			}
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pVictim - 
	//			*pKiller - 
	//			*pInflictor - 
	//-----------------------------------------------------------------------------
	void CCSGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		// Work out what killed the player, and send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_ID = 0;

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

		bool bHeadshot = false;

		if ( pScorer )	// Is the killer a client?
		{
			killer_ID = pScorer->GetUserID();
		
			if( info.GetDamageType() & DMG_HEADSHOT )
			{
				//to enable drawing the headshot icon as well as the weapon icon, 
				bHeadshot = true;
			}
			
			if ( pInflictor )
			{
				if ( pInflictor == pScorer )
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if ( pScorer->GetActiveWeapon() )
					{
						killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname(); //GetDeathNoticeName();
					}
				}
				else
				{
					killer_weapon_name = STRING( pInflictor->m_iClassname );  // it's just that easy
				}
			}
		}
		else
		{
			killer_weapon_name = STRING( pInflictor->m_iClassname );
		}

		// strip the NPC_* or weapon_* from the inflictor's classname
		if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		{
			killer_weapon_name += 7;
		}
		else if ( strncmp( killer_weapon_name, "NPC_", 8 ) == 0 )
		{
			killer_weapon_name += 8;
		}
		else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		{
			killer_weapon_name += 5;
		}
		else if( strncmp( killer_weapon_name, "hegrenade", 9 ) == 0 )	//"hegrenade_projectile"	
		{
			killer_weapon_name = "hegrenade";
		}
		else if( strncmp( killer_weapon_name, "flashbang", 9 ) == 0 )	//"flashbang_projectile"
		{
			killer_weapon_name = "flashbang";
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "player_death" );

		if ( event )
		{
			event->SetInt("userid", pVictim->GetUserID() );
			event->SetInt("attacker", killer_ID );
			event->SetString("weapon", killer_weapon_name );
			event->SetInt("headshot", bHeadshot ? 1 : 0 );
			event->SetInt("priority", bHeadshot ? 8 : 7 );	// HLTV event priority, not transmitted
			
			gameeventmanager->FireEvent( event );
		}

	}

	//=========================================================
	//=========================================================
	// cash_player_killed_enemy_factor: CS:GO reads a per-weapon "kill award" from the weapon data
	// (cs_weapon_parse.cpp:1075) and scales it by that factor (cs_player.cpp:7406). CS:S weapon data has
	// no such value, so this is the known CS:GO table: the knife pays 1500, the AWP 100, everything else
	// 300. It is only used when the factor is above 0, so the stock flat award is untouched by default.
	static int GetWeaponCashAward( CWeaponCSBase *pWeapon )
	{
		if ( !pWeapon )
			return 300;

		switch ( pWeapon->GetWeaponID() )
		{
		case WEAPON_KNIFE:
			return 1500;
		case WEAPON_AWP:
			return 100;
		default:
			return 300;
		}
	}

	void CCSGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

		// If we're killed by the C4, we do a subset of BaseClass::PlayerKilled()
		// Specifically, we shouldn't lose any points or show death notices, to match goldsrc
		if ( Q_strcmp(pKiller->GetClassname(), "planted_c4" ) == 0 )
		{
			// dvsents2: uncomment when removing all FireTargets
			// variant_t value;
			// g_EventQueue.AddEvent( "game_playerdie", "Use", value, 0, pVictim, pVictim );
			FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
		}
		else
		{
			BaseClass::PlayerKilled( pVictim, info );
		}

		// check for team-killing, and give monetary rewards/penalties
		// Find the killer & the scorer
		if ( !pScorer )
			return;

		CCSPlayer *pCSVictim = (CCSPlayer *)pVictim;
		CCSPlayer *pCSScorer = (CCSPlayer *)pScorer;

		// cash_player_get_killed: CS:GO pays the victim a fixed amount for dying (default 0, no stock
		// equivalent).
		if ( mp_playercashawards.GetBool() && cash_player_get_killed.GetInt() != 0 )
			pCSVictim->AddAccount( cash_player_get_killed.GetInt() );

		if ( IPointsForKill( pScorer, pVictim ) < 0 )
		{
			// team-killer!
			pCSScorer->AddAccount( cash_player_killed_teammate.GetInt() );
			++pCSScorer->m_iTeamKills;
			pCSScorer->m_bJustKilledTeammate = true;

			ClientPrint( pCSScorer, HUD_PRINTCENTER, "#Killed_Teammate" );
			if ( mp_autokick.GetBool() )
			{
				char strTeamKills[64];
				Q_snprintf( strTeamKills, sizeof( strTeamKills ), "%d", pCSScorer->m_iTeamKills );
				ClientPrint( pCSScorer, HUD_PRINTCONSOLE, "#Game_teammate_kills", strTeamKills ); // this includes a " of 3" in it
			}

			if ( pCSScorer->m_iTeamKills >= 3 && mp_autokick.GetBool() )
			{
				ClientPrint( pCSScorer, HUD_PRINTCONSOLE, "#Banned_For_Killing_Teammates" );
				engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pCSScorer->GetUserID() ) );
			}
			else if ( mp_spawnprotectiontime.GetInt() > 0 && GetRoundElapsedTime() < mp_spawnprotectiontime.GetInt() )
			{
				ClientPrint( pCSScorer, HUD_PRINTCONSOLE, "#Banned_For_Killing_Teammates" );
				engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pCSScorer->GetUserID() ) );
			}

			if ( !(pCSScorer->m_iDisplayHistoryBits & DHF_FRIEND_KILLED) )
			{
				pCSScorer->m_iDisplayHistoryBits |= DHF_FRIEND_KILLED;
				pCSScorer->HintMessage( "#Hint_careful_around_teammates", false );
			}
		}
		else
		{
			if ( pCSVictim->IsVIP() )
			{
				pCSScorer->HintMessage( "#Hint_reward_for_killing_vip", true );

				if ( mp_playercashawards.GetBool() )
					pCSScorer->AddAccount( 2500 );
			}
			else if ( mp_playercashawards.GetBool() )
			{
				// cash_player_killed_enemy_default is CS:S's flat award. A factor above 0 switches to
				// CS:GO's rule, where the reward is the killing weapon's kill award times the factor.
				const float flKillAwardFactor = cash_player_killed_enemy_factor.GetFloat();
				CWeaponCSBase *pKillWeapon = dynamic_cast< CWeaponCSBase * >( info.GetInflictor() );

				if ( flKillAwardFactor > 0.0f && pKillWeapon )
					pCSScorer->AddAccount( (int)( GetWeaponCashAward( pKillWeapon ) * flKillAwardFactor ) );
				else
					pCSScorer->AddAccount( cash_player_killed_enemy_default.GetInt() );
			}

			if ( !(pCSScorer->m_iDisplayHistoryBits & DHF_ENEMY_KILLED) )
			{
				pCSScorer->m_iDisplayHistoryBits |= DHF_ENEMY_KILLED;
				pCSScorer->HintMessage( "#Hint_win_round_by_killing_enemy", false );
			}

			// mp_dm_scoring: CS:GO's deathmatch kill score. It goes into the frag count because that
			// is the only per-player score a v34 scoreboard shows, and the base rules have already
			// given one frag for this kill.
			if ( mp_dm_scoring.GetBool() )
			{
				int nScore = GetDeathmatchKillScore( pCSScorer->GetActiveCSWeapon() );

				if ( nScore > 1 )
					pCSScorer->IncrementFragCount( nScore - 1 );
			}
		}
	}


	void CCSGameRules::InitDefaultAIRelationships()
	{
		//  Allocate memory for default relationships
		CBaseCombatCharacter::AllocateDefaultRelationships();

		// --------------------------------------------------------------
		// First initialize table so we can report missing relationships
		// --------------------------------------------------------------
		int i, j;
		for (i=0;i<NUM_AI_CLASSES;i++)
		{
			for (j=0;j<NUM_AI_CLASSES;j++)
			{
				// By default all relationships are neutral of priority zero
				CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
			}
		}
	}

	//------------------------------------------------------------------------------
	// Purpose : Return classify text for classify type
	//------------------------------------------------------------------------------
	const char *CCSGameRules::AIClassText(int classType)
	{
		switch (classType)
		{
			case CLASS_NONE:			return "CLASS_NONE";
			case CLASS_PLAYER:			return "CLASS_PLAYER";
			default:					return "MISSING CLASS in ClassifyText()";
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: When gaining new technologies in TF, prevent auto switching if we
	//  receive a weapon during the switch
	// Input  : *pPlayer - 
	//			*pWeapon - 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CCSGameRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		bool bIsBeingGivenItem = false;
		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if ( pCSPlayer && pCSPlayer->IsBeingGivenItem() )
			bIsBeingGivenItem = true;

		if ( pPlayer->GetActiveWeapon() && pPlayer->IsNetClient() && !bIsBeingGivenItem )
		{
			// Player has an active item, so let's check cl_autowepswitch.
			const char *cl_autowepswitch = engine->GetClientConVarValue( engine->IndexOfEdict( pPlayer->edict() ), "cl_autowepswitch" );
			if ( cl_autowepswitch && atoi( cl_autowepswitch ) <= 0 )
			{
				return false;
			}
		}

		if ( pPlayer->IsBot() && !bIsBeingGivenItem )
		{
			return false;
		}

		if ( !GetAllowWeaponSwitch() )
		{
			return false;
		}

		return BaseClass::FShouldSwitchWeapon( pPlayer, pWeapon );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : allow - 
	//-----------------------------------------------------------------------------
	void CCSGameRules::SetAllowWeaponSwitch( bool allow )
	{
		m_bAllowWeaponSwitch = allow;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CCSGameRules::GetAllowWeaponSwitch()
	{
		return m_bAllowWeaponSwitch;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pPlayer - 
	// Output : const char
	//-----------------------------------------------------------------------------
	const char *CCSGameRules::SetDefaultPlayerTeam( CBasePlayer *pPlayer )
	{
		Assert( pPlayer );
		return BaseClass::SetDefaultPlayerTeam( pPlayer );
	}


	void CCSGameRules::LevelInitPreEntity()
	{
		BaseClass::LevelInitPreEntity();

		// TODO for CZ-style hostages: TheHostageChatter->Precache();
	}


	// ---------------------------------------------------------------------------------------------------
	// Hostage spawn selection: mp_hostages_max, mp_hostages_spawn_farthest,
	// mp_hostages_spawn_force_positions and mp_hostages_spawn_same_every_round. CS:GO
	// (cs_gamerules.cpp:8905-9030) keeps a list of the chosen hostage spawn indices, clears that list only
	// when mp_hostages_spawn_same_every_round is 0, and removes every hostage that is not in it. CS:S has no
	// hostage spawn points, only the map-placed hostage_entity entities, so the map's own list is the pool.
	// The choice is applied again at every round start because CCSGameRules::CleanUpMap() recreates every
	// map entity, which puts the removed hostages back.
	// ---------------------------------------------------------------------------------------------------
	static CUtlVector< int > g_arrSelectedHostageSpawnIndices;

	// CS:GO's spatial pick (cs_gamerules.cpp:8958-9010): the combination whose pairwise 2D squared
	// distances sum to the largest value. The exhaustive search only runs up to 16 hostages because its
	// cost is combinatorial; above that the greedy fallback keeps the same intent.
	static void SelectFarthestHostages( int nWanted, CUtlVector< int > &arrKeep )
	{
		const int nHostages = g_Hostages.Count();

		if ( nHostages <= 16 )
		{
			CUtlVector< int > arrTry;

			for ( int i = 0; i < nWanted; ++i )
				arrTry.AddToTail( i );

			float flBestMetric = -1.0f;

			while ( true )
			{
				float flMetric = 0.0f;

				for ( int a = 0; a < arrTry.Count(); ++a )
				{
					for ( int b = a + 1; b < arrTry.Count(); ++b )
					{
						Vector vecBetween = g_Hostages[ arrTry[a] ]->GetAbsOrigin() -
							g_Hostages[ arrTry[b] ]->GetAbsOrigin();
						flMetric += vecBetween.Length2DSqr();
					}
				}

				if ( flMetric > flBestMetric )
				{
					flBestMetric = flMetric;
					arrKeep.RemoveAll();
					arrKeep.AddMultipleToTail( arrTry.Count(), arrTry.Base() );
				}

				// advance to the next combination
				int nAdvance = 0;

				while ( nAdvance < arrTry.Count() &&
						arrTry[ arrTry.Count() - 1 - nAdvance ] >= nHostages - 1 - nAdvance )
				{
					++nAdvance;
				}

				if ( nAdvance >= arrTry.Count() )
					break;

				arrTry[ arrTry.Count() - 1 - nAdvance ]++;

				for ( int i = arrTry.Count() - nAdvance; i < arrTry.Count(); ++i )
				{
					arrTry[i] = arrTry[ arrTry.Count() - 1 - nAdvance ] +
						( i - ( arrTry.Count() - nAdvance ) + 1 );
				}
			}
		}
		else
		{
			// Greedy farthest-first fallback: start from the hostage nearest the map's average position,
			// then repeatedly take the one furthest from everything kept so far.
			Vector vecCentre( 0, 0, 0 );

			for ( int i = 0; i < nHostages; ++i )
				vecCentre += g_Hostages[i]->GetAbsOrigin();

			vecCentre /= (float)nHostages;

			int nFirst = 0;
			float flClosest = 0.0f;
			bool bFirst = true;

			for ( int i = 0; i < nHostages; ++i )
			{
				float flDistance = ( g_Hostages[i]->GetAbsOrigin() - vecCentre ).Length2DSqr();

				if ( bFirst || flDistance < flClosest )
				{
					bFirst = false;
					flClosest = flDistance;
					nFirst = i;
				}
			}

			arrKeep.AddToTail( nFirst );

			while ( arrKeep.Count() < nWanted )
			{
				int nBest = -1;
				float flBest = -1.0f;

				for ( int i = 0; i < nHostages; ++i )
				{
					if ( arrKeep.Find( i ) != arrKeep.InvalidIndex() )
						continue;

					float flNearest = 0.0f;
					bool bNearest = true;

					for ( int k = 0; k < arrKeep.Count(); ++k )
					{
						float flDistance = ( g_Hostages[i]->GetAbsOrigin() -
							g_Hostages[ arrKeep[k] ]->GetAbsOrigin() ).Length2DSqr();

						if ( bNearest || flDistance < flNearest )
						{
							bNearest = false;
							flNearest = flDistance;
						}
					}

					if ( flNearest > flBest )
					{
						flBest = flNearest;
						nBest = i;
					}
				}

				if ( nBest < 0 )
					break;

				arrKeep.AddToTail( nBest );
			}
		}
	}

	// CS:GO's fallback when mp_hostages_spawn_farthest is off is a weighted random pick
	// (cs_gamerules.cpp:9020-9030). CS:S has no per-hostage spawn weight, so every map hostage weighs the
	// same and this is a plain random choice without replacement.
	static void SelectRandomHostages( int nWanted, CUtlVector< int > &arrKeep )
	{
		CUtlVector< int > arrRemaining;

		for ( int i = 0; i < g_Hostages.Count(); ++i )
			arrRemaining.AddToTail( i );

		while ( arrKeep.Count() < nWanted && arrRemaining.Count() > 0 )
		{
			int nPick = RandomInt( 0, arrRemaining.Count() - 1 );

			arrKeep.AddToTail( arrRemaining[nPick] );
			arrRemaining.FastRemove( nPick );
		}
	}

	// Removes every hostage that is not in the selection, back to front so the indices stay valid.
	static void ApplyHostageSpawnSelection( const CUtlVector< int > &arrKeep )
	{
		for ( int i = g_Hostages.Count() - 1; i >= 0; --i )
		{
			if ( arrKeep.Find( i ) != arrKeep.InvalidIndex() )
				continue;

			CHostage *pHostage = g_Hostages[i];

			g_Hostages.Remove( i );

			if ( pHostage )
				UTIL_Remove( pHostage );
		}
	}

	static void SelectMapHostages( void )
	{
		const int nHostages = g_Hostages.Count();

		if ( nHostages <= 0 )
			return;

		// mp_hostages_spawn_force_positions wins over everything else, as in CS:GO (cs_gamerules.cpp:8935).
		const char *pszForced = mp_hostages_spawn_force_positions.GetString();

		if ( pszForced && pszForced[0] )
		{
			CUtlVector< int > arrKeep;
			const char *p = pszForced;

			while ( *p )
			{
				while ( *p == ' ' || *p == ',' )
					++p;

				if ( !*p )
					break;

				const int nIndex = atoi( p );

				while ( *p && *p != ',' )
					++p;

				if ( nIndex < 0 || nIndex >= nHostages )
				{
					Warning( "mp_hostages_spawn_force_positions: index %d is out of range, the map has %d hostages\n", nIndex, nHostages );
					continue;
				}

				if ( arrKeep.Find( nIndex ) == arrKeep.InvalidIndex() )
					arrKeep.AddToTail( nIndex );
			}

			ApplyHostageSpawnSelection( arrKeep );

			return;
		}

		const int nWanted = mp_hostages_max.GetInt();

		if ( nWanted <= 0 || nHostages <= nWanted )
		{
			// stock: every hostage the map placed stays, and no selection is remembered
			g_arrSelectedHostageSpawnIndices.RemoveAll();
			return;
		}

		const bool bSameEveryRound = mp_hostages_spawn_same_every_round.GetBool();

		if ( !bSameEveryRound )
			g_arrSelectedHostageSpawnIndices.RemoveAll();

		CUtlVector< int > arrKeep;

		if ( bSameEveryRound )
		{
			// Reuse the set chosen earlier in this match. The indices stay valid because CleanUpMap()
			// recreates the map entities in map order.
			for ( int i = 0; i < g_arrSelectedHostageSpawnIndices.Count(); ++i )
			{
				const int nIndex = g_arrSelectedHostageSpawnIndices[i];

				if ( nIndex >= 0 && nIndex < nHostages )
					arrKeep.AddToTail( nIndex );
			}
		}

		if ( arrKeep.Count() != nWanted )
		{
			arrKeep.RemoveAll();

			if ( mp_hostages_spawn_farthest.GetBool() )
				SelectFarthestHostages( nWanted, arrKeep );
			else
				SelectRandomHostages( nWanted, arrKeep );

			if ( bSameEveryRound )
			{
				g_arrSelectedHostageSpawnIndices.RemoveAll();
				g_arrSelectedHostageSpawnIndices.AddMultipleToTail( arrKeep.Count(), arrKeep.Base() );
			}
		}

		ApplyHostageSpawnSelection( arrKeep );
	}

	void CCSGameRules::LevelInitPostEntity()
	{
		BaseClass::LevelInitPostEntity();

		m_bLevelInitialized = false; // re-count CT and T start spots now that they exist

		ResetDMSpawnState();

		// mp_hostages_max and friends: drop the hostages the server does not want. Repeated at every
		// round start (see RestartRound) because CleanUpMap() recreates the map's entities.
		g_arrSelectedHostageSpawnIndices.RemoveAll();
		SelectMapHostages();

		// Figure out from the entities in the map what kind of map this is (bomb run, prison escape, etc).
		CheckMapConditions();
	}
	
	INetworkStringTable *g_StringTableBlackMarket = NULL;

	void CCSGameRules::CreateCustomNetworkStringTables( void )
	{
		m_StringTableBlackMarket = g_StringTableBlackMarket;

		if ( mp_dynamicpricing.GetBool() )
		{
			m_bBlackMarket = BlackMarket_DownloadPrices();

			if ( m_bBlackMarket == false )
			{
				Msg( "ERROR: mp_dynamicpricing set to 1 but couldn't download the price list!\n" );
			}
		}
		else
		{
			m_bBlackMarket = false;
			SetBlackMarketPrices( true );
		}
	}

	float CCSGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
	{
		pPlayer->m_Local.m_flFallVelocity -= CS_PLAYER_MAX_SAFE_FALL_SPEED;

		float fallDamage = pPlayer->m_Local.m_flFallVelocity * CS_DAMAGE_FOR_FALL_SPEED * 1.25;

		// CS:GO's sv_falldamage_scale (ref cs_gamerules.cpp:6911). 1 leaves the stock damage.
		fallDamage *= sv_falldamage_scale.GetFloat();

		if ( fallDamage > 0.0f )
		{
			// let the bots know
			IGameEvent * event = gameeventmanager->CreateEvent( "player_falldamage" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetFloat( "damage", fallDamage );
				event->SetInt( "priority", 4 );	// HLTV event priority, not transmitted
				
				gameeventmanager->FireEvent( event );
			}
		}

		return fallDamage;
	} 

	
	void CCSGameRules::ClientDisconnected( edict_t *pClient )
	{
		// mp_disconnect_kills_players / mp_disconnect_kills_bots: kill the player while the entity is
		// still valid, exactly as CS:GO does before calling the base class (cs_gamerules.cpp:6937).
		// The killing blow is a self-inflicted 0 damage hit, so the normal death path runs: weapons
		// drop, the kill feed shows the death and mp_respawn_on_death schedules a respawn that the
		// engine discards when the client is gone.
		if ( mp_disconnect_kills_players.GetBool() )
		{
			CCSPlayer *pPlayer = ToCSPlayer( GetContainingEntity( pClient ) );

			if ( pPlayer && pPlayer->IsAlive() && ( mp_disconnect_kills_bots.GetBool() || !pPlayer->IsBot() ) )
			{
				CTakeDamageInfo info( pPlayer, pPlayer, 0.0f, DMG_NEVERGIB );

				pPlayer->SetHealth( 0 );
				pPlayer->Event_Killed( info );
			}
		}

		BaseClass::ClientDisconnected( pClient );

		CheckWinConditions();
	}


	// Called when game rules are destroyed by CWorld
	void CCSGameRules::LevelShutdown()
	{
		int iLevelIndex = GetCSLevelIndex( STRING( gpGlobals->mapname ) );

		if ( iLevelIndex != -1 )
		{
			g_iTerroristVictories[iLevelIndex] += m_iNumTerroristWins;
			g_iCounterTVictories[iLevelIndex] += m_iNumCTWins;
		}

		ResetDMSpawnState();

		BaseClass::LevelShutdown();
	}

	
	//---------------------------------------------------------------------------------------------------
	/**
	 * Check if the scenario has been won/lost.
	 * Return true if the scenario is over, false if the scenario is still in progress
	 */
	bool CCSGameRules::CheckWinConditions( void )
	{
		if ( mp_ignore_round_win_conditions.GetBool() )
			return false;
		
		// If a winner has already been determined.. then get the heck out of here
		if (m_iRoundWinStatus != WINNER_NONE)
		{
			// still check if we lost players to where we need to do a full reset next round...
			int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
			InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

			bool bNeededPlayers = false;
			NeededPlayersCheck( bNeededPlayers );

			return true;
		}

		// Initialize the player counts..
		int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
		InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );


		/***************************** OTHER PLAYER's CHECK *********************************************************/
		bool bNeededPlayers = false;
		if ( NeededPlayersCheck( bNeededPlayers ) )
			return false;

		/****************************** ASSASINATION/VIP SCENARIO CHECK *******************************************************/
		if ( VIPRoundEndCheck( bNeededPlayers ) )
			return true;

		/****************************** PRISON ESCAPE CHECK *******************************************************/
		if ( PrisonRoundEndCheck() )
			return true;


		/****************************** BOMB CHECK ********************************************************/
		if ( BombRoundEndCheck( bNeededPlayers ) )
			return true;


		/***************************** TEAM EXTERMINATION CHECK!! *********************************************************/
		// CounterTerrorists won by virture of elimination
		if ( TeamExterminationCheck( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT, bNeededPlayers ) )
			return true;

		
		/******************************** HOSTAGE RESCUE CHECK ******************************************************/
		if ( HostageRescueRoundEndCheck( bNeededPlayers ) )
			return true;

		// scenario not won - still in progress
		return false;
	}


	bool CCSGameRules::NeededPlayersCheck( bool &bNeededPlayers )
	{
		// We needed players to start scoring
		// Do we have them now?
		if( !m_iNumSpawnableTerrorist || !m_iNumSpawnableCT )
		{
			Msg( "Game will not start until both teams have players.\n" );
			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_scoring" );
			bNeededPlayers = true;

			m_bFirstConnected = false;
		}

		if ( !m_bFirstConnected && m_iNumSpawnableTerrorist && m_iNumSpawnableCT )
		{
			// Start the round immediately when the first person joins
			// UTIL_LogPrintf( "World triggered \"Game_Commencing\"\n" );

			m_bFreezePeriod  = false; //Make sure we are not on the FreezePeriod.
			m_bCompleteReset = true;

			TerminateRound( 3, Game_Commencing );
			m_bFirstConnected = true;
			return true;
		}

		return false;
	}


	void CCSGameRules::InitializePlayerCounts(
		int &NumAliveTerrorist,
		int &NumAliveCT,
		int &NumDeadTerrorist,
		int &NumDeadCT
		)
	{
		NumAliveTerrorist = NumAliveCT = NumDeadCT = NumDeadTerrorist = 0;
		m_iNumTerrorist = m_iNumCT = m_iNumSpawnableTerrorist = m_iNumSpawnableCT = 0;
		m_iHaveEscaped = 0;

		// Count how many dead players there are on each team.
		for ( int iTeam=0; iTeam < GetNumberOfTeams(); iTeam++ )
		{
			CTeam *pTeam = GetGlobalTeam( iTeam );

			for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
				Assert( pPlayer );
				if ( !pPlayer )
					continue;

				Assert( pPlayer->GetTeamNumber() == pTeam->GetTeamNumber() );

				switch ( pTeam->GetTeamNumber() )
				{
				case TEAM_CT:
					m_iNumCT++;

					if ( pPlayer->State_Get() != STATE_PICKINGCLASS )
						m_iNumSpawnableCT++;

					if ( pPlayer->m_lifeState != LIFE_ALIVE )
						NumDeadCT++;
					else
						NumAliveCT++;

					break;

				case TEAM_TERRORIST:
					m_iNumTerrorist++;

					if ( pPlayer->State_Get() != STATE_PICKINGCLASS )
						m_iNumSpawnableTerrorist++;

					if ( pPlayer->m_lifeState != LIFE_ALIVE )
						NumDeadTerrorist++;
					else
						NumAliveTerrorist++;

					// Check to see if this guy escaped.
					if ( pPlayer->m_bEscaped == true )
						m_iHaveEscaped++;

					break;
				}
			}
		}
	}

	bool CCSGameRules::HostageRescueRoundEndCheck( bool bNeededPlayers )
	{
		// Check to see if 50% of the hostages have been rescued.
		CHostage* hostage = NULL;

		int iNumHostages = g_Hostages.Count();
		int iNumLeftToRescue = 0;
		int i;

		for ( i=0; i<iNumHostages; i++ )
		{
			hostage = g_Hostages[i];

			if ( hostage->m_iHealth > 0 && !hostage->IsRescued() ) // We've found a live hostage. don't end the round
				iNumLeftToRescue++;
		}

		m_iHostagesRemaining = iNumLeftToRescue;

		// mp_hostages_rescuetowin: how many rescues win the round. 0 keeps the stock CS:S rule, where
		// no surviving hostage may be left behind; any other number is CS:GO's, where that many
		// rescues are enough and the remaining live hostages can be abandoned (CS:GO
		// cs_gamerules.cpp:7235). The 50% test below is the stock one and only applies to the stock
		// case, because a rescue goal of N already says how many rescues are needed.
		const int nRescueGoal = mp_hostages_rescuetowin.GetInt();

		bool bRescueCheckDone;

		if ( nRescueGoal <= 0 )
		{
			bRescueCheckDone = ( iNumLeftToRescue == 0 ) && ( iNumHostages > 0 ) &&
				( m_iHostagesRescued >= ( iNumHostages * 0.5 ) );
		}
		else
		{
			const int nNeeded = ( nRescueGoal < iNumHostages ) ? nRescueGoal : iNumHostages;
			const int nLeftCanWin = ( iNumHostages > nNeeded ) ? ( iNumHostages - nNeeded ) : 0;

			bRescueCheckDone = ( iNumHostages > 0 ) && ( iNumLeftToRescue >= nLeftCanWin ) &&
				( m_iHostagesRescued >= nNeeded );
		}

		if ( bRescueCheckDone )
		{
			m_iAccountCT += cash_team_win_by_hostage_rescue.GetInt();

			if ( !bNeededPlayers )
			{
				m_iNumCTWins ++;
				// Update the clients team score
				UpdateTeamScores();
			}
			// tell the bots all the hostages have been rescued
			IGameEvent * event = gameeventmanager->CreateEvent( "hostage_rescued_all" );
			if ( event )
			{
				gameeventmanager->FireEvent( event );
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), All_Hostages_Rescued );
			return true;
		}

		return false;
	}


	bool CCSGameRules::PrisonRoundEndCheck()
	{
		//MIKETODO: get this working when working on prison escape
		/*
		if (m_bMapHasEscapeZone == true)
		{
			float flEscapeRatio;

			flEscapeRatio = (float) m_iHaveEscaped / (float) m_iNumEscapers;

			if (flEscapeRatio >= m_flRequiredEscapeRatio)
			{
				BroadcastSound( "Event.TERWin" );
				m_iAccountTerrorist += 3150;

				if ( !bNeededPlayers )
				{
					m_iNumTerroristWins ++;
					// Update the clients team score
					UpdateTeamScores();
				}
				EndRoundMessage( "#Terrorists_Escaped", Terrorists_Escaped );
				TerminateRound( 5, WINNER_TER );
				return;
			}
			else if ( NumAliveTerrorist == 0 && flEscapeRatio < m_flRequiredEscapeRatio)
			{
				BroadcastSound( "Event.CTWin" );
				m_iAccountCT += (1 - flEscapeRatio) * 3500; // CTs are rewarded based on how many terrorists have escaped...
				
				if ( !bNeededPlayers )
				{
					m_iNumCTWins++;
					// Update the clients team score
					UpdateTeamScores();
				}
				EndRoundMessage( "#CTs_PreventEscape", CTs_PreventEscape );
				TerminateRound( 5, WINNER_CT );
				return;
			}

			else if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && m_iNumSpawnableCT > 0 )
			{
				BroadcastSound( "Event.CTWin" );
				m_iAccountCT += (1 - flEscapeRatio) * 3250; // CTs are rewarded based on how many terrorists have escaped...
				
				if ( !bNeededPlayers )
				{
					m_iNumCTWins++;
					// Update the clients team score
					UpdateTeamScores();
				}
				EndRoundMessage( "#Escaping_Terrorists_Neutralized", Escaping_Terrorists_Neutralized );
				TerminateRound( 5, WINNER_CT );
				return;
			}
			// else return;    
		}
		*/

		return false;
	}


	bool CCSGameRules::VIPRoundEndCheck( bool bNeededPlayers )
	{
		if (m_iMapHasVIPSafetyZone != 1)
			return false;

		if (m_pVIP == NULL)
			return false;

		if (m_pVIP->m_bEscaped == true)
		{
			m_iAccountCT += 3500;

			if ( !bNeededPlayers )
			{
				m_iNumCTWins ++;
				// Update the clients team score
				UpdateTeamScores();
			}

			//MIKETODO: get this working when working on VIP scenarios
			/*
			MessageBegin( MSG_SPEC, SVC_DIRECTOR );
				WRITE_BYTE ( 9 );	// command length in bytes
				WRITE_BYTE ( DRC_CMD_EVENT );	// VIP rescued
				WRITE_SHORT( ENTINDEX(m_pVIP->edict()) );	// index number of primary entity
				WRITE_SHORT( 0 );	// index number of secondary entity
				WRITE_LONG( 15 | DRC_FLAG_FINAL);   // eventflags (priority and flags)
			MessageEnd();
			*/

			// tell the bots the VIP got out
			IGameEvent * event = gameeventmanager->CreateEvent( "vip_escaped" );
			if ( event )
			{
				event->SetInt( "userid", m_pVIP->GetUserID() );
				event->SetInt( "priority", 9 );
				gameeventmanager->FireEvent( event );
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), VIP_Escaped );
			return true;
		}
		else if ( m_pVIP->m_lifeState == LIFE_DEAD )   // The VIP is dead
		{
			m_iAccountTerrorist += cash_team_terrorist_win_bomb.GetInt();

			if ( !bNeededPlayers )
			{
				m_iNumTerroristWins ++;
				// Update the clients team score
				UpdateTeamScores();
			}

			// tell the bots the VIP was killed
			IGameEvent * event = gameeventmanager->CreateEvent( "vip_killed" );
			if ( event )
			{
				event->SetInt( "userid", m_pVIP->GetUserID() );
				event->SetInt( "priority", 9 );
				gameeventmanager->FireEvent( event );
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), VIP_Assassinated );
			return true;
		}

		return false;
	}


	bool CCSGameRules::BombRoundEndCheck( bool bNeededPlayers )
	{
		// Check to see if the bomb target was hit or the bomb defused.. if so, then let's end the round!
		if ( ( m_bTargetBombed == true ) && ( m_bMapHasBombTarget == true ) )
		{
			m_iAccountTerrorist += 3500;

			if ( !bNeededPlayers )
			{
				m_iNumTerroristWins ++;
				// Update the clients team score
				UpdateTeamScores();
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), Target_Bombed );
			return true;
		}
		else
		if ( ( m_bBombDefused == true ) && ( m_bMapHasBombTarget == true ) )
		{
			m_iAccountCT += cash_team_win_by_defusing_bomb.GetInt();

			m_iAccountTerrorist += cash_team_planted_bomb_but_defused.GetInt(); // planted but defused

			if ( !bNeededPlayers )
			{
				m_iNumCTWins++;
				// Update the clients team score
				UpdateTeamScores();
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), Bomb_Defused );
			return true;
		}

		return false;
	}


	bool CCSGameRules::TeamExterminationCheck(
		int NumAliveTerrorist,
		int NumAliveCT,
		int NumDeadTerrorist,
		int NumDeadCT,
		bool bNeededPlayers
	)
	{
		const bool bCTsRespawn = mp_respawn_on_death_ct.GetBool();
		const bool bTsRespawn = mp_respawn_on_death_t.GetBool();

		if ( ( m_iNumCT > 0 && m_iNumSpawnableCT > 0 ) && ( m_iNumTerrorist > 0 && m_iNumSpawnableTerrorist > 0 ) )
		{
			if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && !bTsRespawn && m_iNumSpawnableCT > 0 )
			{
				bool nowin = false;
					
				for ( int iGrenade=0; iGrenade < g_PlantedC4s.Count(); iGrenade++ )
				{
					CPlantedC4 *pC4 = g_PlantedC4s[iGrenade];

					if ( pC4->IsBombActive() )
						nowin = true;
				}

				if ( !nowin )
				{
					if ( m_bMapHasBombTarget )
						m_iAccountCT += cash_team_elimination_bomb_map.GetInt();
					else
						m_iAccountCT += cash_team_elimination_hostage_map_ct.GetInt();

					if ( !bNeededPlayers )
					{
						m_iNumCTWins++;
						// Update the clients team score
						UpdateTeamScores();
					}

					TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win );
					return true;
				}
			}
		
			// Terrorists WON
			if ( NumAliveCT == 0 && NumDeadCT != 0 && !bCTsRespawn && m_iNumSpawnableTerrorist > 0 )
			{
				if ( m_bMapHasBombTarget )
					m_iAccountTerrorist += cash_team_elimination_bomb_map.GetInt();
				else
					m_iAccountTerrorist += cash_team_elimination_hostage_map_t.GetInt();

				if ( !bNeededPlayers )
				{
					m_iNumTerroristWins++;
					// Update the clients team score
					UpdateTeamScores();
				}

				TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );
				return true;
			}
		}
		else if ( NumAliveCT == 0 && !bCTsRespawn && NumAliveTerrorist == 0 && !bTsRespawn )
		{
			TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
			return true;
		}

		return false;
	}


	void CCSGameRules::PickNextVIP()
	{
		// MIKETODO: work on this when getting VIP maps running.
		/*
		if (IsVIPQueueEmpty() != true)
		{
			// Remove the current VIP from his VIP status and make him a regular CT.
			if (m_pVIP != NULL)
				ResetCurrentVIP();

			for (int i = 0; i <= 4; i++)
			{
				if (VIPQueue[i] != NULL)
				{
					m_pVIP = VIPQueue[i];
					m_pVIP->MakeVIP();

					VIPQueue[i] = NULL;		// remove this player from the VIP queue
					StackVIPQueue();		// and re-organize the queue
					m_iConsecutiveVIP = 0;
					return;
				}
			}
		}
		else if (m_iConsecutiveVIP >= 3)	// If it's been the same VIP for 3 rounds already.. then randomly pick a new one
		{
			m_iLastPick++;

			if (m_iLastPick > m_iNumCT)
				m_iLastPick = 1;

			int iCount = 1;

			CBaseEntity* pPlayer = NULL;
			CBasePlayer* player = NULL;
			CBasePlayer* pLastPlayer = NULL;

			pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			while (		(pPlayer != NULL) && (!FNullEnt(pPlayer->edict()))	)
			{
				if (	!(pPlayer->pev->flags & FL_DORMANT)	)
				{
					player = GetClassPtr((CBasePlayer *)pPlayer->pev);
					
					if (	(player->m_iTeam == CT) && (iCount == m_iLastPick)	)
					{
						if (	(player == m_pVIP) && (pLastPlayer != NULL)	)
							player = pLastPlayer;

						// Remove the current VIP from his VIP status and make him a regular CT.
						if (m_pVIP != NULL)
							ResetCurrentVIP();

						player->MakeVIP();
						m_iConsecutiveVIP = 0;

						return;
					}
					else if ( player->m_iTeam == CT )
						iCount++;

					if ( player->m_iTeam != SPECTATOR )
						pLastPlayer = player;
				}
				pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			}
		}
		else if (m_pVIP == NULL)  // There is no VIP and there is no one waiting to be the VIP.. therefore just pick the first CT player we can find.
		{
			CBaseEntity* pPlayer = NULL;
			CBasePlayer* player = NULL;

			pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			while (		(pPlayer != NULL) && (!FNullEnt(pPlayer->edict()))	)
			{
				if ( pPlayer->pev->flags != FL_DORMANT	)
				{
					player = GetClassPtr((CBasePlayer *)pPlayer->pev);
		
					if ( player->m_iTeam == CT )
					{
						player->MakeVIP();
						m_iConsecutiveVIP = 0;
						return;
					}
				}
				pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			}
		}
		*/
	}


	void CCSGameRules::ReadMultiplayCvars()
	{
		// CS:GO lets a hostage or defuse map have its own round length (ref cs_gamerules.cpp:7992-8007).
		// A value of 0 in either convar falls back to mp_roundtime, so leaving them alone keeps the
		// stock length. RestartRound() establishes the map type before calling this.
		float flRoundTime = mp_roundtime.GetFloat();

		if ( m_bMapHasRescueZone && mp_roundtime_hostage.GetFloat() > 0.0f )
		{
			flRoundTime = mp_roundtime_hostage.GetFloat();
		}
		else if ( m_bMapHasBombTarget && mp_roundtime_defuse.GetFloat() > 0.0f )
		{
			flRoundTime = mp_roundtime_defuse.GetFloat();
		}

		m_iRoundTime = (int)( flRoundTime * 60 );
		m_iFreezeTime = mp_freezetime.GetInt();
	}


	//-----------------------------------------------------------------------------
	// Purpose: Halftime (mp_halftime). Swap every player to the other side and reset the economy,
	//			so the second half is played with the sides exchanged.
	//-----------------------------------------------------------------------------
	void CCSGameRules::SwapTeamsForHalftime( void )
	{
		// Assassination maps are built around a fixed VIP and have no meaningful side swap.
		if ( IsVIPMap() )
			return;

		// The score belongs to the sides, not to the players, so it stays where it is: swapping it
		// keeps "CT rounds won" pointing at the side that actually won them.
		const short nCTWins = m_iNumCTWins;
		m_iNumCTWins = m_iNumTerroristWins;
		m_iNumTerroristWins = nCTWins;

		// CS:GO starts a fresh half instead of carrying the first half's loss ladder over: both
		// counters are cleared and the shared losing bonus goes back to the value a fresh match uses
		// (the same 1400 that CCSGameRules sets in the m_bCompleteReset block above, standing in for
		// CS:GO's mp_starting_losses).
		m_iNumConsecutiveCTLoses = 0;
		m_iNumConsecutiveTerroristLoses = 0;
		m_iLoserBonus = cash_team_loser_bonus.GetInt();

		UpdateTeamScores();

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

			if ( !pPlayer )
				continue;

			// SwitchTeam() drops the defuse kit, resets the player class to the model of the new side
			// and refreshes the team counts. Dead players are swapped as well, otherwise they would
			// come back on their old side when the round restarts.
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
				pPlayer->SwitchTeam( TEAM_TERRORIST );
			else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
				pPlayer->SwitchTeam( TEAM_CT );

			// Everything below is CS:GO's CCSPlayer::SwitchTeamsAtRoundReset: the second half starts
			// as if it were the first one. Weapons and armour are destroyed, the account goes back to
			// the start money, and the round's win/loss payout is skipped so the reset is not undone
			// by the cash awards later in this same RestartRound().
			pPlayer->RemoveAllItems( true );

			pPlayer->AddAccount( mp_startmoney.GetInt() - pPlayer->m_iAccount );
			pPlayer->m_iShouldHaveCash = pPlayer->m_iAccount;
			pPlayer->MarkAsNotReceivingMoneyNextRound();
		}

		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Teams are switching sides for the second half" );
	}

	void CCSGameRules::RestartRound()
	{
		if ( !IsFinite( gpGlobals->curtime ) )
		{
			Warning( "NaN curtime in RestartRound\n" );
			gpGlobals->curtime = 0.0f;
		}

		int i;

		m_iTotalRoundsPlayed++;
		
		//ClearBodyQue();

		// Hardlock the player accelaration to 5.0
		//CVAR_SET_FLOAT( "sv_accelerate", 5.0 );
		//CVAR_SET_FLOAT( "sv_friction", 4.0 );
		//CVAR_SET_FLOAT( "sv_stopspeed", 75 );

		sv_stopspeed.SetValue( 75.0f );

		// Tabulate the number of players on each team.
		int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
		InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );
		
		m_bBombDropped = false;
		m_bBombPlanted = false;
		m_bAnyHostageReached = false;
		
		if ( GetHumanTeam() != TEAM_UNASSIGNED )
		{
			MoveHumansToHumanTeam();
		}

		/*************** AUTO-BALANCE CODE *************/
		if ( mp_autoteambalance.GetInt() != 0 &&
			(m_iUnBalancedRounds >= 1) )
		{
			if ( GetHumanTeam() == TEAM_UNASSIGNED )
			{
				BalanceTeams();
			}
		}

		if ( ((m_iNumSpawnableCT - m_iNumSpawnableTerrorist) >= 2) ||
			((m_iNumSpawnableTerrorist - m_iNumSpawnableCT) >= 2)	)
		{
			m_iUnBalancedRounds++;
		}
		else
		{
			m_iUnBalancedRounds = 0;
		}

		// Warn the players of an impending auto-balance next round...
		if ( mp_autoteambalance.GetInt() != 0 &&
			(m_iUnBalancedRounds == 1)	)
		{
			if ( GetHumanTeam() == TEAM_UNASSIGNED )
			{
				UTIL_ClientPrintAll( HUD_PRINTCENTER,"#Auto_Team_Balance_Next_Round");
			}
		}

		/*************** AUTO-BALANCE CODE *************/

		if ( m_bCompleteReset )
		{
			// bounds check
			if ( mp_timelimit.GetInt() < 0 )
			{
				mp_timelimit.SetValue( 0 );
			}
			m_flGameStartTime = gpGlobals->curtime;
			if ( !IsFinite( m_flGameStartTime.Get() ) )
			{
				Warning( "Trying to set a NaN game start time\n" );
				m_flGameStartTime.GetForModify() = 0.0f;
			}

			// Reset total # of rounds played
			m_iTotalRoundsPlayed = 0;

			// Reset score info
			m_iNumTerroristWins				= 0;
			m_iNumCTWins					= 0;
			m_iNumConsecutiveTerroristLoses	= 0;
			m_iNumConsecutiveCTLoses		= 0;


			// Reset team scores
			UpdateTeamScores();


			// Reset the player stats
			for ( i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CCSPlayer *pPlayer = CCSPlayer::Instance( i );

				if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
					pPlayer->Reset();
			}
		}

		// Halftime: after the first half of the match, swap sides and reset the economy. This runs
		// before the round spawns anyone, so every player respawns on his new side with the new
		// team's model and spawn points.
		if ( mp_halftime.GetBool() && mp_maxrounds.GetInt() > 1 &&
			 m_iTotalRoundsPlayed == mp_maxrounds.GetInt() / 2 )
		{
			SwapTeamsForHalftime();
		}

		// The map type has to be known before ReadMultiplayCvars() picks the round length, so the
		// checks CS:S used to do further down RestartRound() happen here.
		// Check to see if this map has a bomb target in it

		if ( gEntList.FindEntityByClassname( NULL, "func_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= true;
		}
		else if ( gEntList.FindEntityByClassname( NULL, "info_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= false;
		}
		else
		{
			m_bMapHasBombTarget		= false;
			m_bMapHasBombZone		= false;
		}

		// Check to see if this map has hostage rescue zones

		if ( gEntList.FindEntityByClassname( NULL, "func_hostage_rescue" ) )
			m_bMapHasRescueZone = true;
		else
			m_bMapHasRescueZone = false;

		m_bFreezePeriod = true;

		ReadMultiplayCvars();

		// Check to see if there's a mapping info paramater entity
		if ( g_pMapInfo )
		{
			switch ( g_pMapInfo->m_iBuyingStatus )
			{
				case 0: 
					m_bCTCantBuy = false; 
					m_bTCantBuy = false; 
					Msg( "EVERYONE CAN BUY!\n" );
					break;
				
				case 1: 
					m_bCTCantBuy = false; 
					m_bTCantBuy = true; 
					Msg( "Only CT's can buy!!\n" );
					break;

				case 2: 
					m_bCTCantBuy = true; 
					m_bTCantBuy = false; 
					Msg( "Only T's can buy!!\n" );
					break;
				
				case 3: 
					m_bCTCantBuy = true; 
					m_bTCantBuy = true; 
					Msg( "No one can buy!!\n" );
					break;

				default: 
					m_bCTCantBuy = false; 
					m_bTCantBuy = false; 
					break;
			}
		}
		else
		{
			// by default everyone can buy
			m_bCTCantBuy = false; 
			m_bTCantBuy = false; 
		}
		
		


		// See if the map has func_buyzone entities
		// Used by CBasePlayer::HandleSignals() to support maps without these entities
		
		if ( gEntList.FindEntityByClassname( NULL, "func_buyzone" ) )
			m_bMapHasBuyZone = true;
		else
			m_bMapHasBuyZone = false;


		// GOOSEMAN : See if this map has func_escapezone entities
		if ( gEntList.FindEntityByClassname( NULL, "func_escapezone" ) )
		{
			m_bMapHasEscapeZone = true;
			m_iHaveEscaped = 0;
			m_iNumEscapers = 0; // Will increase this later when we count how many Ts are starting
			if (m_iNumEscapeRounds >= 3)
			{
				SwapAllPlayers();
				m_iNumEscapeRounds = 0;
			}

			m_iNumEscapeRounds++;  // Increment the number of rounds played... After 8 rounds, the players will do a whole sale switch..
		}
		else
			m_bMapHasEscapeZone = false;

		// Check to see if this map has VIP safety zones
		if ( gEntList.FindEntityByClassname( NULL, "func_vip_safetyzone" ) )
		{
			PickNextVIP();
			m_iConsecutiveVIP++;
			m_iMapHasVIPSafetyZone = 1;
		}
		else
			m_iMapHasVIPSafetyZone = 2;

		// Update accounts based on number of hostages remaining.. 
		int iRescuedHostageBonus = 0;

		for ( int iHostage=0; iHostage < g_Hostages.Count(); iHostage++ )
		{
			CHostage *pHostage = g_Hostages[iHostage];

			if( pHostage->IsRescuable() )	//Alive and not rescued
			{
				iRescuedHostageBonus += cash_team_hostage_alive.GetInt();
			}
			
			if ( iRescuedHostageBonus >= 2000 )
				break;
		}

		//*******Catch up code by SupraFiend. Scale up the loser bonus when teams fall into losing streaks
		if (m_iRoundWinStatus == WINNER_TER) // terrorists won
		{
			//check to see if they just broke a losing streak
			if(m_iNumConsecutiveTerroristLoses > 1)
				m_iLoserBonus = 1500;//this is the default losing bonus

			m_iNumConsecutiveTerroristLoses = 0;//starting fresh
			m_iNumConsecutiveCTLoses++;//increment the number of wins the CTs have had
		}
		else if (m_iRoundWinStatus == WINNER_CT) // CT Won
		{
			//check to see if they just broke a losing streak
			if(m_iNumConsecutiveCTLoses > 1)
				m_iLoserBonus = 1500;//this is the default losing bonus

			m_iNumConsecutiveCTLoses = 0;//starting fresh
			m_iNumConsecutiveTerroristLoses++;//increment the number of wins the Terrorists have had
		}

		//check if the losing team is in a losing streak & that the loser bonus hasen't maxed out.
		if((m_iNumConsecutiveTerroristLoses > 1) && (m_iLoserBonus < 3000))
			m_iLoserBonus += cash_team_loser_bonus_consecutive_rounds.GetInt();//help out the team in the losing streak
		else
		if((m_iNumConsecutiveCTLoses > 1) && (m_iLoserBonus < 3000))
			m_iLoserBonus += cash_team_loser_bonus_consecutive_rounds.GetInt();//help out the team in the losing streak

		// assign the wining and losing bonuses
		if (m_iRoundWinStatus == WINNER_TER) // terrorists won
		{
			m_iAccountTerrorist += iRescuedHostageBonus;
			m_iAccountCT += m_iLoserBonus;
		}
		else if (m_iRoundWinStatus == WINNER_CT) // CT Won
		{
			m_iAccountCT += iRescuedHostageBonus;
			if (m_bMapHasEscapeZone == false)	// only give them the bonus if this isn't an escape map
				m_iAccountTerrorist += m_iLoserBonus;
		}
		

		// cash_team_winner_bonus_consecutive_rounds: CS:GO pays the winner this extra amount once the
		// loser is on a losing streak (cs_gamerules.cpp:8211). Default 0 keeps the stock economy.
		const int nWinnerStreakBonus = cash_team_winner_bonus_consecutive_rounds.GetInt();

		if ( nWinnerStreakBonus != 0 )
		{
			if ( m_iRoundWinStatus == WINNER_TER && m_iNumConsecutiveCTLoses > 1 )
				m_iAccountTerrorist += nWinnerStreakBonus;
			else if ( m_iRoundWinStatus == WINNER_CT && m_iNumConsecutiveTerroristLoses > 1 )
				m_iAccountCT += nWinnerStreakBonus;
		}

		//Update CT account based on number of hostages rescued
		m_iAccountCT += m_iHostagesRescued * cash_team_rescued_hostage.GetInt();


		// Update individual players accounts and respawn players

		//**********new code by SupraFiend
		//##########code changed by MartinO 
		//the round time stamp must be set before players are spawned
		m_fRoundStartTime = gpGlobals->curtime + m_iFreezeTime;

		if ( !IsFinite( m_fRoundStartTime.Get() ) )
		{
			Warning( "Trying to set a NaN round start time\n" );
			m_fRoundStartTime.GetForModify() = 0.0f;
		}
		
		//Adrian - No cash for anyone at first rounds! ( well, only the default. )
		if ( m_bCompleteReset )
		{
			m_iAccountTerrorist = m_iAccountCT = 0; //No extra cash!.

			//We are starting fresh. So it's like no one has ever won or lost.
			m_iNumTerroristWins				= 0; 
			m_iNumCTWins					= 0;
			m_iNumConsecutiveTerroristLoses	= 0;
			m_iNumConsecutiveCTLoses		= 0;
			m_iLoserBonus					= cash_team_loser_bonus.GetInt();
		}

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			pPlayer->m_iNumSpawns	= 0;
			pPlayer->m_bTeamChanged	= false;
				
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				if ( pPlayer->DoesPlayerGetRoundStartMoney() && mp_teamcashawards.GetBool() )
				{
					pPlayer->AddAccount( m_iAccountCT );
					pPlayer->m_iShouldHaveCash = pPlayer->m_iAccount;
				}
			}
			else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			{
				m_iNumEscapers++;	// Add another potential escaper to the mix!
				if ( pPlayer->DoesPlayerGetRoundStartMoney() && mp_teamcashawards.GetBool() )
				{
					pPlayer->AddAccount( m_iAccountTerrorist );
					pPlayer->m_iShouldHaveCash = pPlayer->m_iAccount;
				}
			}

			// mp_afterroundmoney: flat cash for everyone, independent of the round result.
			if ( mp_afterroundmoney.GetInt() > 0 &&
				 ( pPlayer->GetTeamNumber() == TEAM_CT || pPlayer->GetTeamNumber() == TEAM_TERRORIST ) )
			{
				pPlayer->AddAccount( mp_afterroundmoney.GetInt() );
				pPlayer->m_iShouldHaveCash = pPlayer->m_iAccount;
			}

			// tricky, make players non solid while moving to their spawn points
			if ( (pPlayer->GetTeamNumber() == TEAM_CT) || (pPlayer->GetTeamNumber() == TEAM_TERRORIST) )
			{
				pPlayer->AddSolidFlags( FSOLID_NOT_SOLID );
			}
		}

		// decide who gets the free defuse kit before anybody respawns
		AllocateDefuserForRound();

		// know respawn all players
		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_CT && pPlayer->PlayerClass() >= FIRST_CT_CLASS && pPlayer->PlayerClass() <= LAST_CT_CLASS )
			{
				pPlayer->RoundRespawn();
			}
			if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && pPlayer->PlayerClass() >= FIRST_T_CLASS && pPlayer->PlayerClass() <= LAST_T_CLASS )
			{
				pPlayer->RoundRespawn();
			}
			else
			{
				pPlayer->ObserverRoundRespawn();
			}

			if ( pPlayer->m_iAccount > pPlayer->m_iShouldHaveCash )
			{
				m_bDontUploadStats = true;
			}
		}

		// Respawn entities (glass, doors, etc..)
		CleanUpMap();

		// CleanUpMap() recreated every map entity, so the hostages the server removed are back. Re-apply
		// the selection before m_iHostagesRemaining is taken from g_Hostages.Count() below.
		SelectMapHostages();

		// now run a tkpunish check, after the map has been cleaned up
		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_CT && pPlayer->PlayerClass() >= FIRST_CT_CLASS && pPlayer->PlayerClass() <= LAST_CT_CLASS )
			{
				pPlayer->CheckTKPunishment();
			}
			if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && pPlayer->PlayerClass() >= FIRST_T_CLASS && pPlayer->PlayerClass() <= LAST_T_CLASS )
			{
				pPlayer->CheckTKPunishment();
			}
		}

		// Give C4 to the terrorists
		if (m_bMapHasBombTarget == true	)
			GiveC4();

		// Reset game variables
		m_flIntermissionEndTime = 0;
		m_flRestartRoundTime = 0.0;
		m_flNextRespawnWave[0] = 0.0f;
		m_flNextRespawnWave[1] = 0.0f;
		m_iAccountTerrorist = m_iAccountCT = 0;
		m_iHostagesRescued = 0;
		m_iHostagesTouched = 0;
		m_iHostagesRemaining = 0;
		m_iRoundWinStatus = WINNER_NONE;
		m_bTargetBombed = m_bBombDefused = false;
		m_bCompleteReset = false;
		m_flNextHostageAnnouncement = gpGlobals->curtime;

		m_iHostagesRemaining = g_Hostages.Count();

		// fire global game event
		IGameEvent * event = gameeventmanager->CreateEvent( "round_start" );
		if ( event )
		{
			event->SetInt("timelimit", m_iRoundTime );
			event->SetInt("fraglimit", 0 );
			event->SetInt( "priority", 6 ); // HLTV event priority, not transmitted
		
			if ( m_bMapHasRescueZone )
			{
				event->SetString("objective","HOSTAGE RESCUE");
			}
			else if ( m_bMapHasEscapeZone )
			{
				event->SetString("objective","PRISON ESCAPE");
			}
			else if ( m_iMapHasVIPSafetyZone == 1 )
			{
				event->SetString("objective","VIP RESCUE");
			}
			else if ( m_bMapHasBombTarget || m_bMapHasBombZone )
			{
				event->SetString("objective","BOMB TARGET");
			}
			else
			{
				event->SetString("objective","DEATHMATCH");
			}

			gameeventmanager->FireEvent( event );
		}
	
		UploadGameStats();

		CreateWeaponManager( "weapon_*", gpGlobals->maxClients * 2 );
	}


	void CCSGameRules::GiveC4()
	{
		// mp_give_player_c4: CS:GO's switch for maps that should not spawn the bomb for a player.
		if ( !mp_give_player_c4.GetBool() )
			return;

		enum {
			ALL_TERRORISTS = 0,
			HUMAN_TERRORISTS,
		};
		int iTerrorists[2][ABSOLUTE_PLAYER_LIMIT];
		int numAliveTs[2] = { 0, 0 };
		int lastBombGuyIndex[2] = { -1, -1 };

		//Create an array of the indeces of bomb carrier candidates
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

			if( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TERRORIST && numAliveTs[ALL_TERRORISTS] < ABSOLUTE_PLAYER_LIMIT  )
			{
				if ( pPlayer == m_pLastBombGuy )
				{
					lastBombGuyIndex[ALL_TERRORISTS] = numAliveTs[ALL_TERRORISTS];
					lastBombGuyIndex[HUMAN_TERRORISTS] = numAliveTs[HUMAN_TERRORISTS];
				}

				iTerrorists[ALL_TERRORISTS][numAliveTs[ALL_TERRORISTS]] = i;
				numAliveTs[ALL_TERRORISTS]++;
				if ( !pPlayer->IsBot() )
				{
					iTerrorists[HUMAN_TERRORISTS][numAliveTs[HUMAN_TERRORISTS]] = i;
					numAliveTs[HUMAN_TERRORISTS]++;
				}
			}
		}

		int which = cv_bot_defer_to_human.GetBool();
		if ( numAliveTs[HUMAN_TERRORISTS] == 0 )
		{
			which = ALL_TERRORISTS;
		}

		//pick one of the candidates randomly
		if( numAliveTs[which] > 0 )
		{
			int index = random->RandomInt(0,numAliveTs[which]-1);
			if ( lastBombGuyIndex[which] >= 0 )
			{
				// give the C4 sequentially
				index = (lastBombGuyIndex[which] + 1) % numAliveTs[which];
			}
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iTerrorists[which][index] ) );

			Assert( pPlayer && pPlayer->GetTeamNumber() == TEAM_TERRORIST && pPlayer->IsAlive() );

			pPlayer->GiveNamedItem( WEAPON_C4_CLASSNAME );
			m_pLastBombGuy = pPlayer;

			//pPlayer->SetBombIcon();
			//pPlayer->pev->body = 1;
			
			pPlayer->m_iDisplayHistoryBits |= DHF_BOMB_RETRIEVED;
			pPlayer->HintMessage( "#Hint_you_have_the_bomb", false, true );

			// Log this information
			//UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Spawned_With_The_Bomb\"\n", 
			//	STRING( pPlayer->GetPlayerName() ),
			//	GETPLAYERUSERID( pPlayer->edict() ),
			//	GETPLAYERAUTHID( pPlayer->edict() ) );
		}

		m_bBombDropped = false;
	}

	void CCSGameRules::Think()
	{
		CGameRules::Think();

		// mp_respawn_on_death_ct/t: bring a team's dead back at its respawn wave.
		CheckRespawnWaves();

		for ( int i = 0; i < GetNumberOfTeams(); i++ )
		{
			GetGlobalTeam( i )->Think();
		}

		///// Check game rules /////
		if ( CheckGameOver() )
		{
			return;
		}

		// have we hit the max rounds?
		if ( CheckMaxRounds() )
		{
			return;
		}

		// did somebaody hit the fraglimit ?
		if ( CheckFragLimit() )
		{
			return;
		}

		if ( CheckWinLimit() )
		{
			return;
		}

		
		// Check for the end of the round.
		if ( IsFreezePeriod() )
		{
			CheckFreezePeriodExpired();
		}
		else 
		{
			CheckRoundTimeExpired();
		}

		CheckLevelInitialized();

		// mp_randomspawn: grow the deathmatch spawn point population out of the nav mesh.
		MaintainDMSpawnPopulation();

		if ( m_flRestartRoundTime > 0.0f && m_flRestartRoundTime <= gpGlobals->curtime )
		{
			bool botSpeaking = false;
			for ( int i=1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer *player = UTIL_PlayerByIndex( i );
				if (player == NULL)
					continue;

				if (!player->IsBot())
					continue;
				
				CCSBot *bot = dynamic_cast< CCSBot * >(player);
				if ( !bot )
					continue;

				if ( bot->IsUsingVoice() )
				{
					if ( gpGlobals->curtime > m_flRestartRoundTime + 10.0f )
					{
						Msg( "Ignoring speaking bot %s at round end\n", bot->GetPlayerName() );
					}
					else
					{
						botSpeaking = true;
						break;
					}
				}
			}

			if ( !botSpeaking )
			{
				RestartRound();
			}
		}
		
		if ( gpGlobals->curtime > m_tmNextPeriodicThink )
		{
			CheckRestartRound();
			m_tmNextPeriodicThink = gpGlobals->curtime + 1.0;
		}
	}


	// The bots do their processing after physics simulation etc so their visibility checks don't recompute
	// bone positions multiple times a frame.
	void CCSGameRules::EndGameFrame( void )
	{
		TheBots->StartFrame();

		BaseClass::EndGameFrame();
	}

	bool CCSGameRules::CheckGameOver()
	{
		if ( g_fGameOver )   // someone else quit the game already
		{
			// check to see if we should change levels now
			if ( m_flIntermissionEndTime < gpGlobals->curtime )
			{
				if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
				{
					g_fGameOver = true;

					Msg( "CHANGE LEVEL: %s\n", nextlevel.GetString() );
		
					engine->ChangeLevel( nextlevel.GetString(), NULL );
				}
				else
				{
					ChangeLevel(); // intermission is over
				}
			}

			return true;
		}

		return false;
	}

	bool CCSGameRules::CheckFragLimit()
	{
		if ( fraglimit.GetInt() <= 0 )
			return false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->FragCount() >= fraglimit.GetInt() )
			{
				const char *teamName = "UNKNOWN";
				if ( pPlayer->GetTeam() )
				{
					teamName = pPlayer->GetTeam()->GetName();
				}
				UTIL_LogPrintf("\"%s<%i><%s><%s>\" triggered \"Intermission_Kill_Limit\"\n", 
					pPlayer->GetPlayerName(),
					pPlayer->GetUserID(),
					pPlayer->GetNetworkIDString(),
					teamName
					);
				GoToIntermission();
				return true;
			}
		}

		return false;
	}

	bool CCSGameRules::CheckMaxRounds()
	{
		if ( mp_maxrounds.GetInt() != 0 )
		{
			if ( m_iTotalRoundsPlayed >= mp_maxrounds.GetInt() )
			{
				UTIL_LogPrintf("World triggered \"Intermission_Round_Limit\"\n");
				GoToIntermission();
				return true;
			}
		}
		
		return false;
	}


	bool CCSGameRules::CheckWinLimit()
	{
		// has one team won the specified number of rounds?
		if ( mp_winlimit.GetInt() != 0 )
		{
			if ( m_iNumCTWins >= mp_winlimit.GetInt() )
			{
				UTIL_LogPrintf("Team \"CT\" triggered \"Intermission_Win_Limit\"\n");
				GoToIntermission();
				return true;
			}
			if ( m_iNumTerroristWins >= mp_winlimit.GetInt() )
			{
				UTIL_LogPrintf("Team \"TERRORIST\" triggered \"Intermission_Win_Limit\"\n");
				GoToIntermission();
				return true;
			}
		}

		return false;
	}


	void CCSGameRules::CheckFreezePeriodExpired()
	{
		float startTime = m_fRoundStartTime;
		if ( !IsFinite( startTime ) )
		{
			Warning( "Infinite round start time!\n" );
			m_fRoundStartTime.GetForModify() = gpGlobals->curtime;
		}

		if ( IsFinite( startTime ) && gpGlobals->curtime < startTime )
		{
			return; // not time yet to start round
		}

		// Log this information
		UTIL_LogPrintf("World triggered \"Round_Start\"\n");

		char CT_sentence[40];
		char T_sentence[40];
		
		switch ( random->RandomInt( 0, 3 ) )
		{
		case 0:
			Q_strncpy(CT_sentence,"radio.moveout", sizeof( CT_sentence ) ); 
			Q_strncpy(T_sentence ,"radio.moveout", sizeof( T_sentence ) ); 
			break;

		case 1:
			Q_strncpy(CT_sentence, "radio.letsgo", sizeof( CT_sentence ) ); 
			Q_strncpy(T_sentence , "radio.letsgo", sizeof( T_sentence ) ); 
			break;

		case 2:
			Q_strncpy(CT_sentence , "radio.locknload", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.locknload", sizeof( T_sentence ) );
			break;

		default:
			Q_strncpy(CT_sentence , "radio.go", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.go", sizeof( T_sentence ) );
			break;
		}

		// More specific radio commands for the new scenarios : Prison & Assasination
		if (m_bMapHasEscapeZone == TRUE)
		{
			Q_strncpy(CT_sentence , "radio.elim", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.getout", sizeof( T_sentence ) );
		}
		else if (m_iMapHasVIPSafetyZone == 1)
		{
			Q_strncpy(CT_sentence , "radio.vip", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.locknload", sizeof( T_sentence ) );
		}

		// Freeze period expired: kill the flag
		m_bFreezePeriod = false;

		IGameEvent * event = gameeventmanager->CreateEvent( "round_freeze_end" );
		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}

		// Update the timers for all clients and play a sound
		bool bCTPlayed = false;
		bool bTPlayed = false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = CCSPlayer::Instance( i );
			if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
			{
				if ( pPlayer->State_Get() == STATE_ACTIVE )
				{
					if ( (pPlayer->GetTeamNumber() == TEAM_CT) && !bCTPlayed )
					{
						pPlayer->Radio( CT_sentence );
						bCTPlayed = true;
					}
					else if ( (pPlayer->GetTeamNumber() == TEAM_TERRORIST) && !bTPlayed )
					{
						pPlayer->Radio( T_sentence );
						bTPlayed = true;
					}

					if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
					{
						pPlayer->ResetMaxSpeed();
					}
				}
				
				//pPlayer->SyncRoundTimer();
			}
		}
	}


	void CCSGameRules::CheckRoundTimeExpired()
	{
		if ( mp_ignore_round_win_conditions.GetBool() )
			return;
		
		if ( GetRoundRemainingTime() > 0 || m_iRoundWinStatus != WINNER_NONE ) 
			return; //We haven't completed other objectives, so go for this!.

		if( !m_bFirstConnected )
			return;

		// New code to get rid of round draws!!

		if ( m_bMapHasBombTarget )
		{
			//If the bomb is planted, don't let the round timer end the round.
			//keep going until the bomb explodes or is defused
			if( !m_bBombPlanted )
			{
				m_iAccountCT += cash_team_win_by_time_running_out_bomb.GetInt();
				
				m_iNumCTWins++;

				TerminateRound( mp_round_restart_delay.GetFloat(), Target_Saved );
				UpdateTeamScores();
				MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(TEAM_TERRORIST);
			}
		}
		else if ( m_bMapHasRescueZone )
		{
			m_iAccountTerrorist += cash_team_win_by_time_running_out_hostage.GetInt(); 
			
			m_iNumTerroristWins++;
			TerminateRound( mp_round_restart_delay.GetFloat(), Hostages_Not_Rescued );
			UpdateTeamScores();
			MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(TEAM_CT);
		}
		else if ( m_bMapHasEscapeZone )
		{
			m_iNumCTWins++;
			TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Not_Escaped );
			UpdateTeamScores();
		}
		else if ( m_iMapHasVIPSafetyZone == 1 )
		{
			m_iAccountTerrorist += 3250;
			m_iNumTerroristWins++;

			TerminateRound( mp_round_restart_delay.GetFloat(), VIP_Not_Escaped );
			UpdateTeamScores();
		}
	}

	void CCSGameRules::GoToIntermission( void )
	{
		Msg( "Going to intermission...\n" );

		BaseClass::GoToIntermission();

		// set all players to FL_FROZEN
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer )
			{
				pPlayer->AddFlag( FL_FROZEN );
			}
		}

		// freeze players while in intermission
		m_bFreezePeriod = true;
	}

	static void PrintToConsole( CBasePlayer *player, const char *text )
	{
		if ( player )
		{
			ClientPrint( player, HUD_PRINTCONSOLE, text );
		}
		else
		{
			Msg( "%s", text );
		}
	}

	void CCSGameRules::DumpTimers( void ) const
	{
		extern ConVar bot_join_delay;
		CBasePlayer *player = UTIL_GetCommandClient();
		CFmtStr str;

		PrintToConsole( player, str.sprintf( "Timers and related info at %f:\n", gpGlobals->curtime ) );
		PrintToConsole( player, str.sprintf( "m_bCompleteReset: %d\n", m_bCompleteReset ) );
		PrintToConsole( player, str.sprintf( "m_iTotalRoundsPlayed: %d\n", m_iTotalRoundsPlayed ) );
		PrintToConsole( player, str.sprintf( "m_iRoundTime: %d\n", m_iRoundTime.Get() ) );
		PrintToConsole( player, str.sprintf( "m_iRoundWinStatus: %d\n", m_iRoundWinStatus ) );

		PrintToConsole( player, str.sprintf( "first connected: %d\n", m_bFirstConnected ) );
		PrintToConsole( player, str.sprintf( "intermission end time: %f\n", m_flIntermissionEndTime ) );
		PrintToConsole( player, str.sprintf( "freeze period: %d\n", m_bFreezePeriod.Get() ) );
		PrintToConsole( player, str.sprintf( "round restart time: %f\n", m_flRestartRoundTime ) );
		PrintToConsole( player, str.sprintf( "game start time: %f\n", m_flGameStartTime.Get() ) );
		PrintToConsole( player, str.sprintf( "m_fRoundStartTime: %f\n", m_fRoundStartTime.Get() ) );
		PrintToConsole( player, str.sprintf( "freeze time: %d\n", m_iFreezeTime ) );
		PrintToConsole( player, str.sprintf( "next think: %f\n", m_tmNextPeriodicThink ) );

		PrintToConsole( player, str.sprintf( "fraglimit: %d\n", fraglimit.GetInt() ) );
		PrintToConsole( player, str.sprintf( "mp_maxrounds: %d\n", mp_maxrounds.GetInt() ) );
		PrintToConsole( player, str.sprintf( "mp_winlimit: %d\n", mp_winlimit.GetInt() ) );
		PrintToConsole( player, str.sprintf( "bot_quota: %d\n", cv_bot_quota.GetInt() ) );
		PrintToConsole( player, str.sprintf( "bot_quota_mode: %s\n", cv_bot_quota_mode.GetString() ) );
		PrintToConsole( player, str.sprintf( "bot_join_after_player: %d\n", cv_bot_join_after_player.GetInt() ) );
		PrintToConsole( player, str.sprintf( "bot_join_delay: %d\n", bot_join_delay.GetInt() ) );
		PrintToConsole( player, str.sprintf( "nextlevel: %s\n", nextlevel.GetString() ) );

		int humansInGame = UTIL_HumansInGame( true );
		int botsInGame = UTIL_BotsInGame();
		PrintToConsole( player, str.sprintf( "%d humans and %d bots in game\n", humansInGame, botsInGame ) );

		PrintToConsole( player, str.sprintf( "num CTs (spawnable): %d (%d)\n", m_iNumCT, m_iNumSpawnableCT ) );
		PrintToConsole( player, str.sprintf( "num Ts (spawnable): %d (%d)\n", m_iNumTerrorist, m_iNumSpawnableTerrorist ) );

		if ( g_fGameOver )
		{
			PrintToConsole( player, str.sprintf( "Game is over!\n" ) );
		}
		PrintToConsole( player, str.sprintf( "\n" ) );
	}

	CON_COMMAND( mp_dump_timers, "Prints round timers to the console for debugging" )
	{
		if ( !UTIL_IsCommandIssuedByServerAdmin() )
			return;
		
		if ( CSGameRules() )
		{
			CSGameRules()->DumpTimers();
		}
	}


	// living players on the given team need to be marked as not receiving any money
	// next round.
	void CCSGameRules::MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(int team)
	{
		int playerNum;
		for (playerNum = 1; playerNum <= gpGlobals->maxClients; ++playerNum)
		{
			CCSPlayer *player = (CCSPlayer *)UTIL_PlayerByIndex(playerNum);
			if (player == NULL)
			{
				continue;
			}

			if ((player->GetTeamNumber() == team) && (player->IsAlive()))
			{
				player->MarkAsNotReceivingMoneyNextRound();
			}
		}
	}


	void CCSGameRules::CheckLevelInitialized( void )
	{
		if ( !m_bLevelInitialized )
		{
			// Count the number of spawn points for each team
			// This determines the maximum number of players allowed on each

			CBaseEntity* ent = NULL; 
			
			m_iSpawnPointCount_Terrorist	= 0;
			m_iSpawnPointCount_CT			= 0;

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_terrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) )
				{
					m_iSpawnPointCount_Terrorist++;
				}
				else
				{
					Warning("Invalid terrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
						ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[2],ent->GetAbsOrigin()[2] );
				}
			}

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_counterterrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) ) 
				{
					m_iSpawnPointCount_CT++;
				}
				else
				{
					Warning("Invalid counterterrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
						ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[2],ent->GetAbsOrigin()[2] );
				}
			}

			// Is this a logo map?
			if ( gEntList.FindEntityByClassname( NULL, "info_player_logo" ) )
				m_bLogoMap = true;

			m_bLevelInitialized = true;
		}
	}

	void CCSGameRules::ShowSpawnPoints( void )
	{
		CBaseEntity* ent = NULL;
		
		while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_terrorist" ) ) != NULL )
		{
			if ( IsSpawnPointValid( ent, NULL ) )
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, 600 );
			}
			else
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, 600);
			}
		}

		while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_counterterrorist" ) ) != NULL )
		{
			if ( IsSpawnPointValid( ent, NULL ) ) 
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, 600 );
			}
			else
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, 600 );
			}
		}
	}

	void CCSGameRules::CheckRestartRound( void )
	{
		// Restart the game if specified by the server
		int iRestartDelay = mp_restartgame.GetInt();

		if ( iRestartDelay > 0 )
		{
			if ( iRestartDelay > 60 )
				iRestartDelay = 60;

			// log the restart
			UTIL_LogPrintf( "World triggered \"Restart_Round_(%i_%s)\"\n", iRestartDelay, iRestartDelay == 1 ? "second" : "seconds" );

			UTIL_LogPrintf( "Team \"CT\" scored \"%i\" with \"%i\" players\n", m_iNumCTWins, m_iNumCT );
			UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", m_iNumTerroristWins, m_iNumTerrorist );

			// let the players know
			char strRestartDelay[64];
			Q_snprintf( strRestartDelay, sizeof( strRestartDelay ), "%d", iRestartDelay );
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Game_will_restart_in", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );
			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_will_restart_in", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );

			m_flRestartRoundTime = gpGlobals->curtime + iRestartDelay;
			m_bCompleteReset = true;
			mp_restartgame.SetValue( 0 );
		}
	}


	class SetHumanTeamFunctor
	{
	public:
		SetHumanTeamFunctor( int targetTeam )
		{
			m_targetTeam = targetTeam;
			m_sourceTeam = ( m_targetTeam == TEAM_CT ) ? TEAM_TERRORIST : TEAM_CT;

			m_traitors.MakeReliable();
			m_loyalists.MakeReliable();
			m_loyalists.AddAllPlayers();
		}

		bool operator()( CBasePlayer *basePlayer )
		{
			CCSPlayer *player = ToCSPlayer( basePlayer );
			if ( !player )
				return true;

			if ( player->IsBot() )
				return true;

			if ( player->GetTeamNumber() != m_sourceTeam )
				return true;

			if ( player->State_Get() == STATE_PICKINGCLASS )
				return true;

			if ( CSGameRules()->TeamFull( m_targetTeam ) )
				return false;

			if ( CSGameRules()->TeamStacked( m_targetTeam, m_sourceTeam ) )
				return false;

			player->SwitchTeam( m_targetTeam );
			m_traitors.AddRecipient( player );
			m_loyalists.RemoveRecipient( player );

			return true;
		}

		void SendNotice( void )
		{
			if ( m_traitors.GetRecipientCount() > 0 )
			{
				UTIL_ClientPrintFilter( m_traitors, HUD_PRINTCENTER, "#Player_Balanced" );
				UTIL_ClientPrintFilter( m_loyalists, HUD_PRINTCENTER, "#Teams_Balanced" );
			}
		}

	private:
		int m_targetTeam;
		int m_sourceTeam;

		CRecipientFilter m_traitors;
		CRecipientFilter m_loyalists;
	};


	void CCSGameRules::MoveHumansToHumanTeam( void )
	{
		int targetTeam = GetHumanTeam();
		if ( targetTeam != TEAM_TERRORIST && targetTeam != TEAM_CT )
			return;

		SetHumanTeamFunctor setTeam( targetTeam );
		ForEachPlayer( setTeam );

		setTeam.SendNotice();
	}


	void CCSGameRules::BalanceTeams( void )
	{
		int iTeamToSwap = TEAM_UNASSIGNED;
		int iNumToSwap;

		if (m_iMapHasVIPSafetyZone == 1) // The ratio for teams is different for Assasination maps
		{
			int iDesiredNumCT, iDesiredNumTerrorist;
			
			if ( (m_iNumCT + m_iNumTerrorist)%2 != 0)	// uneven number of players
				iDesiredNumCT			= (int)((m_iNumCT + m_iNumTerrorist) * 0.55) + 1;
			else
				iDesiredNumCT			= (int)((m_iNumCT + m_iNumTerrorist)/2);
			iDesiredNumTerrorist	= (m_iNumCT + m_iNumTerrorist) - iDesiredNumCT;

			if ( m_iNumCT < iDesiredNumCT )
			{
				iTeamToSwap = TEAM_TERRORIST;
				iNumToSwap = iDesiredNumCT - m_iNumCT;
			}
			else if ( m_iNumTerrorist < iDesiredNumTerrorist )
			{
				iTeamToSwap = TEAM_CT;
				iNumToSwap = iDesiredNumTerrorist - m_iNumTerrorist;
			}
			else
				return;
		}
		else
		{
			if (m_iNumCT > m_iNumTerrorist)
			{
				iTeamToSwap = TEAM_CT;
				iNumToSwap = (m_iNumCT - m_iNumTerrorist)/2;
				
			}
			else if (m_iNumTerrorist > m_iNumCT)
			{
				iTeamToSwap = TEAM_TERRORIST;
				iNumToSwap = (m_iNumTerrorist - m_iNumCT)/2;
			}
			else
			{
				return;	// Teams are even.. Get out of here.
			}
		}

		if (iNumToSwap > 3) // Don't swap more than 3 players at a time.. This is a naive method of avoiding infinite loops.
			iNumToSwap = 3;

		int iTragetTeam = TEAM_UNASSIGNED;

		if ( iTeamToSwap == TEAM_CT )
		{
			iTragetTeam = TEAM_TERRORIST;
		}
		else if ( iTeamToSwap == TEAM_TERRORIST )
		{
			iTragetTeam = TEAM_CT;
		}
		else
		{
			// no valid team to swap
			return;
		}

		CRecipientFilter traitors;
		CRecipientFilter loyalists;

		traitors.MakeReliable();
		loyalists.MakeReliable();
		loyalists.AddAllPlayers();

		for (int i = 0; i < iNumToSwap; i++)
		{
			// last person to join the server
			int iHighestUserID = -1;
			CCSPlayer *pPlayerToSwap = NULL;

			// check if target team is full, exit if so
			if ( TeamFull(iTragetTeam) )
				break;

			// search for player with highest UserID = most recently joined to switch over
			for ( int j = 1; j <= gpGlobals->maxClients; j++ )
			{
				CCSPlayer *pPlayer = (CCSPlayer *)UTIL_PlayerByIndex( j );

				if ( !pPlayer )
					continue;

				CCSBot *bot = dynamic_cast< CCSBot * >(pPlayer);
				if ( bot )
					continue; // don't swap bots - the bot system will handle that

				if ( pPlayer &&
					 ( m_pVIP != pPlayer ) && 
					 ( pPlayer->GetTeamNumber() == iTeamToSwap ) && 
					 ( engine->GetPlayerUserId( pPlayer->edict() ) > iHighestUserID ) &&
					 ( pPlayer->State_Get() != STATE_PICKINGCLASS ) )
					{
						iHighestUserID = engine->GetPlayerUserId( pPlayer->edict() );
						pPlayerToSwap = pPlayer;
					}
			}

			if ( pPlayerToSwap != NULL )
			{
				traitors.AddRecipient( pPlayerToSwap );
				loyalists.RemoveRecipient( pPlayerToSwap );
				pPlayerToSwap->SwitchTeam( iTragetTeam );
			}
		}

		if ( traitors.GetRecipientCount() > 0 )
		{
			UTIL_ClientPrintFilter( traitors, HUD_PRINTCENTER, "#Player_Balanced" );
			UTIL_ClientPrintFilter( loyalists, HUD_PRINTCENTER, "#Teams_Balanced" );
		}
	}


	// CS:GO cs_gamerules.cpp:13701 - the team size cap is based on the number of spawn points, except
	// on a team that spawns at random deathmatch spawn points, where it is the player cap. Without this
	// a map that only has info_deathmatch_spawn entities would report both teams as full at zero
	// players, because it has no info_player_terrorist/counterterrorist points.
	int CCSGameRules::MaxNumPlayersOnTerrTeam()
	{
		bool bRandomTSpawn = ( mp_randomspawn.GetInt() == 1 || mp_randomspawn.GetInt() == TEAM_TERRORIST );

		return bRandomTSpawn ? MAX_PLAYERS : m_iSpawnPointCount_Terrorist;
	}

	int CCSGameRules::MaxNumPlayersOnCTTeam()
	{
		bool bRandomCTSpawn = ( mp_randomspawn.GetInt() == 1 || mp_randomspawn.GetInt() == TEAM_CT );

		return bRandomCTSpawn ? MAX_PLAYERS : m_iSpawnPointCount_CT;
	}

	bool CCSGameRules::TeamFull( int team_id )
	{
		CheckLevelInitialized();

		switch ( team_id )
		{
		case TEAM_TERRORIST:
			return m_iNumTerrorist >= MaxNumPlayersOnTerrTeam();

		case TEAM_CT:
			return m_iNumCT >= MaxNumPlayersOnCTTeam();
		}

		return false;
	}
	
	int CCSGameRules::GetHumanTeam()
	{
		if ( FStrEq( "CT", mp_humanteam.GetString() ) )
		{
			return TEAM_CT;
		}
		else if ( FStrEq( "T", mp_humanteam.GetString() ) )
		{
			return TEAM_TERRORIST;
		}
		
		return TEAM_UNASSIGNED;
	}

	int CCSGameRules::SelectDefaultTeam( bool ignoreBots /*= false*/ )
	{
		if ( ignoreBots && ( FStrEq( cv_bot_join_team.GetString(), "T" ) || FStrEq( cv_bot_join_team.GetString(), "CT" ) ) )
		{
			ignoreBots = false;	// don't ignore bots when they can't switch teams
		}

		if ( ignoreBots && !mp_autoteambalance.GetBool() )
		{
			ignoreBots = false;	// don't ignore bots when they can't switch teams
		}

		int team = TEAM_UNASSIGNED;
		int numTerrorists = m_iNumTerrorist;
		int numCTs = m_iNumCT;
		if ( ignoreBots )
		{
			numTerrorists = UTIL_HumansOnTeam( TEAM_TERRORIST );
			numCTs = UTIL_HumansOnTeam( TEAM_CT );
		}

		// Choose the team that's lacking players
		if ( numTerrorists < numCTs )
		{
			team = TEAM_TERRORIST;
		}
		else if ( numTerrorists > numCTs )
		{
			team = TEAM_CT;
		}
		// Choose the team that's losing
		else if ( m_iNumTerroristWins < m_iNumCTWins )
		{
			team = TEAM_TERRORIST;
		}
		else if ( m_iNumCTWins < m_iNumTerroristWins )
		{
			team = TEAM_CT;
		}
		else
		{
			// Teams and scores are equal, pick a random team
			if ( random->RandomInt( 0, 1 ) == 0 )
			{
				team = TEAM_CT;
			}
			else
			{
				team = TEAM_TERRORIST;
			}
		}

		if ( TeamFull( team ) )
		{
			// Pick the opposite team
			if ( team == TEAM_TERRORIST )
			{
				team = TEAM_CT;
			}
			else
			{
				team = TEAM_TERRORIST;
			}

			// No choices left
			if ( TeamFull( team ) )
				return TEAM_UNASSIGNED;
		}

		return team;
	}

	//checks to see if the desired team is stacked, returns true if it is
	bool CCSGameRules::TeamStacked( int newTeam_id, int curTeam_id  )
	{
		//players are allowed to change to their own team
		if(newTeam_id == curTeam_id)
			return false;

		// if mp_limitteams is 0, don't check
		if ( mp_limitteams.GetInt() == 0 )
			return false;

		switch ( newTeam_id )
		{
		case TEAM_TERRORIST:
			if(curTeam_id != TEAM_UNASSIGNED && curTeam_id != TEAM_SPECTATOR)
			{
				if((m_iNumTerrorist + 1) > (m_iNumCT + mp_limitteams.GetInt() - 1))
					return true;
				else
					return false;
			}
			else
			{
				if((m_iNumTerrorist + 1) > (m_iNumCT + mp_limitteams.GetInt()))
					return true;
				else
					return false;
			}
			break;
		case TEAM_CT:
			if(curTeam_id != TEAM_UNASSIGNED && curTeam_id != TEAM_SPECTATOR)
			{
				if((m_iNumCT + 1) > (m_iNumTerrorist + mp_limitteams.GetInt() - 1))
					return true;
				else
					return false;
			}
			else
			{
				if((m_iNumCT + 1) > (m_iNumTerrorist + mp_limitteams.GetInt()))
					return true;
				else
					return false;
			}
			break;
		}

		return false;
	}


	//=========================================================
	//=========================================================
	bool CCSGameRules::FPlayerCanRespawn( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pBasePlayer );
		if ( !pPlayer )
			Error( "FPlayerCanRespawn: pPlayer=0" );

		// Player cannot respawn twice in a round
		if ( pPlayer->m_iNumSpawns > 0 && m_bFirstConnected )
			return false;

		// If they're dead after the map has ended, and it's about to start the next round,
		// wait for the round restart to respawn them.
		if ( gpGlobals->curtime < m_flRestartRoundTime )
			return false;

		// Only valid team members can spawn
		if ( pPlayer->GetTeamNumber() != TEAM_CT && pPlayer->GetTeamNumber() != TEAM_TERRORIST )
			return false;

		// Only players with a valid class can spawn
		if ( pPlayer->GetClass() == CS_CLASS_NONE )
			return false;

		// Player cannot respawn until next round if more than 20 seconds in

		// Tabulate the number of players on each team.
		m_iNumCT = GetGlobalTeam( TEAM_CT )->GetNumPlayers();
		m_iNumTerrorist = GetGlobalTeam( TEAM_TERRORIST )->GetNumPlayers();

		if ( m_iNumTerrorist > 0 && m_iNumCT > 0 )
		{
			if ( gpGlobals->curtime > (m_fRoundStartTime + 20) )
			{
				//If this player just connected and fadetoblack is on, then maybe
				//the server admin doesn't want him peeking around.
				color32_s clr = {0,0,0,0};
				if ( mp_fadetoblack.GetBool() )
				{
					UTIL_ScreenFade( pPlayer, clr, 3, 3, FFADE_OUT | FFADE_STAYOUT );
				}

				return false;
			}
		}

		// Player cannot respawn while in the Choose Appearance menu
		//if ( pPlayer->m_iMenu == Menu_ChooseAppearance )
		//	return false;

		return true;
	}

	void CCSGameRules::TerminateRound(float tmDelay, int iReason )
	{
		variant_t emptyVariant;
		int iWinnerTeam = WINNER_NONE;
		const char *text = "UNKNOWN";
				
		// UTIL_ClientPrintAll( HUD_PRINTCENTER, sentence );

		switch ( iReason )
		{
// Terror wins:
			case Target_Bombed:	
				text = "#Target_Bombed";
				iWinnerTeam = WINNER_TER;
				break;

			case VIP_Assassinated:
				text = "#VIP_Assassinated";
				iWinnerTeam = WINNER_TER;
				break;

			case Terrorists_Escaped:
				text = "#Terrorists_Escaped";
				iWinnerTeam = WINNER_TER;
				break;

			case Terrorists_Win:
				text = "#Terrorists_Win";
				iWinnerTeam = WINNER_TER;
				break;

			case Hostages_Not_Rescued:
				text = "#Hostages_Not_Rescued";
				iWinnerTeam = WINNER_TER;
				break;

			case VIP_Not_Escaped:
				text = "#VIP_Not_Escaped";
				iWinnerTeam = WINNER_TER;
				break;
// CT wins:
			case VIP_Escaped:
				text = "#VIP_Escaped";
				iWinnerTeam = WINNER_CT;
				break;

			case CTs_PreventEscape:
				text = "#CTs_PreventEscape";
				iWinnerTeam = WINNER_CT;
				break;

			case Escaping_Terrorists_Neutralized:
				text = "#Escaping_Terrorists_Neutralized";
				iWinnerTeam = WINNER_CT;
				break;

			case Bomb_Defused:
				text = "#Bomb_Defused";
				iWinnerTeam = WINNER_CT;
				break;

			case CTs_Win:
				text = "#CTs_Win";
				iWinnerTeam = WINNER_CT;
				break;

			case All_Hostages_Rescued:
				text = "#All_Hostages_Rescued";
				iWinnerTeam = WINNER_CT;
				break;

			case Target_Saved:
				text = "#Target_Saved";
				iWinnerTeam = WINNER_CT;
				break;

			case Terrorists_Not_Escaped:
				text = "#Terrorists_Not_Escaped";
				iWinnerTeam = WINNER_CT;
				break;
// no winners:
			case Game_Commencing:
				text = "#Game_Commencing";
				iWinnerTeam = WINNER_DRAW;
				break;

			case Round_Draw:
				text = "#Round_Draw";
				iWinnerTeam = WINNER_DRAW;
				break;

			default:
				DevMsg("TerminateRound: unknown round end ID %i\n", iReason );
				break;
		}

		m_iRoundWinStatus = iWinnerTeam;
		m_flRestartRoundTime = gpGlobals->curtime + tmDelay;

		if ( iWinnerTeam == WINNER_CT )
		{
			for( int i=0;i<g_Hostages.Count();i++ )
				g_Hostages[i]->AcceptInput( "CTsWin", NULL, NULL, emptyVariant, 0 );
		}

		else if ( iWinnerTeam == WINNER_TER )
		{
			for( int i=0;i<g_Hostages.Count();i++ )
				g_Hostages[i]->AcceptInput( "TerroristsWin", NULL, NULL, emptyVariant, 0 );
		}
		else
		{
			Assert( iWinnerTeam == WINNER_NONE || iWinnerTeam == WINNER_DRAW );
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "round_end" );
		if ( event )
		{
			event->SetInt( "winner", iWinnerTeam );
			event->SetInt( "reason", iReason );
			event->SetString( "message", text );
			event->SetInt( "priority", 6 ); // HLTV event priority, not transmitted
			gameeventmanager->FireEvent( event );
		}
	
		if ( GetMapRemainingTime() == 0.0f  )
		{
			UTIL_LogPrintf("World triggered \"Intermission_Time_Limit\"\n");
			GoToIntermission();
		}
	}


	void CCSGameRules::UpdateTeamScores()
	{
		CTeam *pTerrorists = GetGlobalTeam( TEAM_TERRORIST );
		CTeam *pCTs = GetGlobalTeam( TEAM_CT );

		Assert( pTerrorists && pCTs );

		if( pTerrorists )
			pTerrorists->SetScore( m_iNumTerroristWins );

		if( pCTs )
			pCTs->SetScore( m_iNumCTWins );
	}


	void CCSGameRules::CheckMapConditions()
	{
		// Check to see if this map has a bomb target in it
		if ( gEntList.FindEntityByClassname( NULL, "func_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= true;
		}
		else if ( gEntList.FindEntityByClassname( NULL, "info_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= false;
		}
		else
		{
			m_bMapHasBombTarget		= false;
			m_bMapHasBombZone		= false;
		}

		// See if the map has func_buyzone entities
		// Used by CBasePlayer::HandleSignals() to support maps without these entities
		if ( gEntList.FindEntityByClassname( NULL, "func_buyzone" ) )
		{
			m_bMapHasBuyZone = true;
		}
		else
		{
			m_bMapHasBuyZone = false;
		}

		// Check to see if this map has hostage rescue zones
		if ( gEntList.FindEntityByClassname( NULL, "func_hostage_rescue" ) )
		{
			m_bMapHasRescueZone = true;
		}
		else
		{
			m_bMapHasRescueZone = false;
		}

		// GOOSEMAN : See if this map has func_escapezone entities
		if ( gEntList.FindEntityByClassname( NULL, "func_escapezone" ) )
		{
			m_bMapHasEscapeZone = true;
		}
		else
		{
			m_bMapHasEscapeZone = false;
		}

		// Check to see if this map has VIP safety zones
		if ( gEntList.FindEntityByClassname( NULL, "func_vip_safetyzone" ) )
		{
			m_iMapHasVIPSafetyZone = 1;
		}
		else
		{
			m_iMapHasVIPSafetyZone = 2;
		}
	}


	void CCSGameRules::SwapAllPlayers()
	{
		// MOTODO we have to make sure that enought spaning points exits
		Assert ( 0 );
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			/* CCSPlayer *pPlayer = CCSPlayer::Instance( i );
			if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
				pPlayer->SwitchTeam(); */
		}

		// Swap Team victories
		int iTemp;

		iTemp = m_iNumCTWins;
		m_iNumCTWins = m_iNumTerroristWins;
		m_iNumTerroristWins = iTemp;
		
		// Update the clients team score
		UpdateTeamScores();
	}


	bool FindInList( const char **pStrings, const char *pToFind )
	{
		int i = 0;
		while ( pStrings[i][0] != 0 )
		{
			if ( Q_stricmp( pStrings[i], pToFind ) == 0 )
				return true;
			i++;
		}

		return false;
	}

	void CCSGameRules::CleanUpMap()
	{
		if (IsLogoMap())
			return;

		// Recreate all the map entities from the map data (preserving their indices),
		// then remove everything else except the players.

		// Get rid of all entities except players.
		CBaseEntity *pCur = gEntList.FirstEnt();
		while ( pCur )
		{
			CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >( pCur );
			// Weapons with owners don't want to be removed..
			if ( pWeapon )
			{
				if ( pWeapon->ShouldRemoveOnRoundRestart() )
				{
					UTIL_Remove( pCur );
				}
			}
			// remove entities that has to be restored on roundrestart (breakables etc)
			else if ( !FindInList( s_PreserveEnts, pCur->GetClassname() ) )
			{
				UTIL_Remove( pCur );
			}
			
			pCur = gEntList.NextEnt( pCur );
		}
		
		// Really remove the entities so we can have access to their slots below.
		gEntList.CleanupDeleteList();

		// Cancel all queued events, in case a func_bomb_target fired some delayed outputs that
		// could kill respawning CTs
		g_EventQueue.Clear();

		// Now reload the map entities.
		class CCSMapEntityFilter : public IMapEntityFilter
		{
		public:
			virtual bool ShouldCreateEntity( const char *pClassname )
			{
				// Don't recreate the preserved entities.
				if ( !FindInList( s_PreserveEnts, pClassname ) )
				{
					return true;
				}
				else
				{
					// Increment our iterator since it's not going to call CreateNextEntity for this ent.
					if ( m_iIterator != g_MapEntityRefs.InvalidIndex() )
						m_iIterator = g_MapEntityRefs.Next( m_iIterator );
				
					return false;
				}
			}


			virtual CBaseEntity* CreateNextEntity( const char *pClassname )
			{
				if ( m_iIterator == g_MapEntityRefs.InvalidIndex() )
				{
					// This shouldn't be possible. When we loaded the map, it should have used 
					// CCSMapLoadEntityFilter, which should have built the g_MapEntityRefs list
					// with the same list of entities we're referring to here.
					Assert( false );
					return NULL;
				}
				else
				{
					CMapEntityRef &ref = g_MapEntityRefs[m_iIterator];
					m_iIterator = g_MapEntityRefs.Next( m_iIterator );	// Seek to the next entity.

					if ( ref.m_iEdict == -1 || engine->PEntityOfEntIndex( ref.m_iEdict ) )
					{
						// Doh! The entity was delete and its slot was reused.
						// Just use any old edict slot. This case sucks because we lose the baseline.
						return CreateEntityByName( pClassname );
					}
					else
					{
						// Cool, the slot where this entity was is free again (most likely, the entity was 
						// freed above). Now create an entity with this specific index.
						return CreateEntityByName( pClassname, ref.m_iEdict );
					}
				}
			}

		public:
			int m_iIterator; // Iterator into g_MapEntityRefs.
		};
		CCSMapEntityFilter filter;
		filter.m_iIterator = g_MapEntityRefs.Head();

		// DO NOT CALL SPAWN ON info_node ENTITIES!

		MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );
	}


	bool CCSGameRules::IsThereABomber()
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = CCSPlayer::Instance( i );

			if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
			{
				if ( pPlayer->GetTeamNumber() == TEAM_CT )
					continue;

				if ( pPlayer->HasC4() )
					 return true; //There you are.
			}
		}

		//Didn't find a bomber.
		return false;
	}


	void CCSGameRules::EndRound()
	{
		// fake a round end
		CSGameRules()->TerminateRound( 0.0f, Round_Draw );
	}

	CBaseEntity *CCSGameRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
	{
		// gat valid spwan point
		CBaseEntity *pSpawnSpot = pPlayer->EntSelectSpawnPoint();

		// drop down to ground
		Vector GroundPos = DropToGround( pPlayer, pSpawnSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX );

		// Move the player to the place it said.
		pPlayer->Teleport( &pSpawnSpot->GetAbsOrigin(), &pSpawnSpot->GetLocalAngles(), &vec3_origin );
		pPlayer->m_Local.m_vecPunchAngle = vec3_angle;
		
		return pSpawnSpot;
	}
	
	// checks if the spot is clear of players
	bool CCSGameRules::IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer )
	{
		if ( !pSpot->IsTriggered( pPlayer ) )
		{
			return false;
		}

		Vector mins = GetViewVectors()->m_vHullMin;
		Vector maxs = GetViewVectors()->m_vHullMax;

		Vector vTestMins = pSpot->GetAbsOrigin() + mins;
		Vector vTestMaxs = pSpot->GetAbsOrigin() + maxs;
		
		// First test the starting origin.
		return UTIL_IsSpaceEmpty( pPlayer, vTestMins, vTestMaxs );
	}


	// UTIL_IsVisibleToTeam() lives in game_shared/cstrike/bot/bot_util.cpp
extern bool UTIL_IsVisibleToTeam( const Vector &spot, int team );

	// Return false only if every living member of the given team is closer to the spot than
	// mp_randomspawn_dist. The whole test is skipped when either convar is off.
	// CS:GO shared/cstrike15/bot/bot_util.cpp:414
	bool UTIL_IsRandomSpawnFarEnoughAwayFromTeam( const Vector &spot, int team )
	{
		if ( mp_randomspawn_dist.GetInt() <= 0 )
			return true;

		if ( mp_randomspawn_los.GetInt() == 0 )
			return true;

		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *player = UTIL_PlayerByIndex( i );

			if ( player == NULL )
				continue;

			if ( !player->IsAlive() )
				continue;

			if ( player->GetTeamNumber() != team )
				continue;

			if ( mp_randomspawn_dist.GetInt() > 0 && ( player->GetAbsOrigin() ).DistTo( spot ) < mp_randomspawn_dist.GetInt() )
			{
				// this one is too close, keep looking
				continue;
			}
			else
			{
				return true;
			}
		}

		return false;
	}

	// CS:GO cs_gamerules.cpp:18728 - a deathmatch kill is worth mp_dm_kill_base_score, plus 10 when
	// the kill was made with the knife, plus 2 for a weapon cheaper than 1400 and plus 1 for a weapon
	// cheaper than 3500. CS:GO looks the weapon up through the econ loadout slot; weapons are
	// addressed by their info in this tree, which carries the same price.
	int CCSGameRules::GetDeathmatchKillScore( CBaseCombatWeapon *pWeapon )
	{
		CWeaponCSBase *pCSWeapon = dynamic_cast< CWeaponCSBase * >( pWeapon );

		if ( !pCSWeapon )
			return 0;

		if ( pCSWeapon->GetWeaponID() == WEAPON_KNIFE )
			return 10 + mp_dm_kill_base_score.GetInt();

		int nScore = mp_dm_kill_base_score.GetInt();
		int nPrice = pCSWeapon->GetCSWpnData().GetWeaponPrice();

		if ( nPrice < 0 )
			return 1;	// no price in the weapon script, CS:GO falls back to a single point

		if ( nPrice < 1400 )
			nScore += 2;
		else if ( nPrice < 3500 )
			nScore += 1;

		return nScore;
	}

	// --------------------------------------------------------------------------------------------------- //
	// Deathmatch spawn point generation (ported from CS:GO server/cstrike15/cs_nav_mesh.cpp:280)
	//
	// CS:GO generates its own info_deathmatch_spawn points from the nav mesh on maps that do not have
	// deathmatch spawn points, so a stock map works with random spawning without being recompiled.
	// The rules are CS:GO's: 50 spawns at most, one per second, nav areas larger than 100x100 units,
	// 300 units apart, off the ground (spawned 16 units up), clear of world geometry and reachable
	// from a team spawn point.
	// --------------------------------------------------------------------------------------------------- //
	#define DM_DESIRED_SPAWN_POINTS	50
	#define DM_SPAWN_MIN_AREA_SIZE	100.0f
	#define DM_SPAWN_MIN_DISTANCE	300.0f

	static CUtlVector<EHANDLE> g_DMSpawnPoints;		// spawn points this generator created
	static float g_flNextDMSpawnRefresh = 0.0f;
	static int g_nDesiredDMSpawns = DM_DESIRED_SPAWN_POINTS;
	static int g_nDMSpawnFailures = 0;

	// Forget everything about the previous map. The entities themselves are gone by now.
	static void ResetDMSpawnState( void )
	{
		g_DMSpawnPoints.RemoveAll();
		g_flNextDMSpawnRefresh = 0.0f;
		g_nDesiredDMSpawns = DM_DESIRED_SPAWN_POINTS;
		g_nDMSpawnFailures = 0;
	}

	// CS:GO cs_nav_mesh.cpp:484 - reject a spot that is inside world geometry, with some extra head
	// room so a player cannot spawn in a ceiling.
	static bool IsDMSpawnBlockedByWorld( const Vector &pos )
	{
		trace_t trace;
		Vector rayStart = pos;
		Vector rayEnd = pos + Vector( 0, 0, -16 );	// back on ground
		Vector hullMin = VEC_HULL_MIN;
		Vector hullMax = VEC_HULL_MAX + Vector( 0, 0, 32 );

		UTIL_TraceHull( rayStart, rayEnd, hullMin, hullMax, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		return trace.startsolid;
	}

	// CS:GO cs_nav_mesh.cpp:378 - a spawn point nobody can walk to is useless, so the area has to be
	// reachable from a team spawn point.
	static bool IsDMSpawnAreaReachable( CNavArea *pArea )
	{
		CBaseEntity *pCTSpawn = gEntList.FindEntityByClassname( NULL, "info_player_counterterrorist" );

		if ( !pCTSpawn )
			return true;	// nothing to test the path against

		CNavArea *pCTSpawnArea = TheNavMesh->GetNearestNavArea( pCTSpawn->GetAbsOrigin() );

		if ( pCTSpawnArea )
		{
			ShortestPathCost cost;

			if ( NavAreaBuildPath( pArea, pCTSpawnArea, NULL, cost ) )
				return true;
		}

		CBaseEntity *pTSpawn = gEntList.FindEntityByClassname( NULL, "info_player_terrorist" );

		if ( !pTSpawn )
			return false;

		CNavArea *pTSpawnArea = TheNavMesh->GetNearestNavArea( pTSpawn->GetAbsOrigin() );

		if ( !pTSpawnArea )
			return true;

		ShortestPathCost cost2;

		return NavAreaBuildPath( pArea, pTSpawnArea, NULL, cost2 );
	}

	static void MaintainDMSpawnPopulation( void )
	{
		if ( mp_randomspawn.GetInt() == 0 )
			return;

		bool bDisableAutoGeneratedDMSpawns = ( g_pMapInfo ? g_pMapInfo->m_bDisableAutoGeneratedDMSpawns : false );

		if ( bDisableAutoGeneratedDMSpawns )
			return;

		if ( g_nDesiredDMSpawns <= 0 || gpGlobals->curtime < g_flNextDMSpawnRefresh )
			return;

		g_flNextDMSpawnRefresh = gpGlobals->curtime + 1.0f;

		if ( !TheNavMesh || !TheNavMesh->IsLoaded() )
			return;

		unsigned int nAreaCount = TheNavMesh->GetNavAreaCount();

		if ( nAreaCount == 0 )
			return;

		int actualCount = 0;

		for ( int i = 0; i < g_DMSpawnPoints.Count(); ++i )
		{
			if ( g_DMSpawnPoints[i] != NULL )
				++actualCount;
		}

		// If the map isn't large enough to support as many spawns as we want, shrink the target to
		// whatever we could get instead of searching forever.
		if ( g_nDMSpawnFailures >= 100 )
		{
			g_nDesiredDMSpawns = actualCount;
			g_nDMSpawnFailures = 0;

			Warning( "Giving up attempts to make more random spawn points. Current count: %i.\n", actualCount );
		}

		if ( actualCount >= g_nDesiredDMSpawns )
			return;

		int need = g_nDesiredDMSpawns - actualCount;
		int spawnsToTry = ( need > 10 ) ? need : 10;	// CS:GO: max( 10, need )

		for ( int k = 0; k < spawnsToTry; k++ )
		{
			g_nDMSpawnFailures++;

			for ( int attempts = 0; attempts < 10; attempts++ )
			{
				CNavArea *testArea = TheNavMesh->GetNavAreaByID( random->RandomInt( 1, nAreaCount ) );

				if ( !testArea || testArea->GetSizeX() <= DM_SPAWN_MIN_AREA_SIZE || testArea->GetSizeY() <= DM_SPAWN_MIN_AREA_SIZE )
					continue;

				// don't put a spawn point too close to another one
				bool bBadNavArea = false;

				for ( int n = 0; n < g_DMSpawnPoints.Count(); ++n )
				{
					if ( g_DMSpawnPoints[n] == NULL )
						continue;

					Vector between = g_DMSpawnPoints[n]->GetAbsOrigin() - testArea->GetCenter();

					if ( between.IsLengthLessThan( DM_SPAWN_MIN_DISTANCE ) )
					{
						bBadNavArea = true;
						break;
					}
				}

				if ( bBadNavArea )
					continue;

				// Move the spawn point up slightly so it isn't in the ground; the player drops onto the
				// floor from there when they spawn.
				Vector potentialSpawnPoint = testArea->GetCenter() + Vector( 0, 0, 16 );

				if ( IsDMSpawnBlockedByWorld( potentialSpawnPoint ) )
					continue;

				if ( !IsDMSpawnAreaReachable( testArea ) )
					continue;

				CBaseEntity *pDMSpawn = CreateEntityByName( "info_deathmatch_spawn" );

				if ( pDMSpawn )
				{
					pDMSpawn->SetAbsOrigin( potentialSpawnPoint );

					DispatchSpawn( pDMSpawn );
					g_DMSpawnPoints.AddToTail( pDMSpawn );

					g_nDMSpawnFailures = 0;
				}

				break;
			}
		}
	}

	// Nav areas, and so the spawn points made from them, are only computed once per map load, so a
	// freshly nav-edited map would keep spawning players at the old spots until it was reloaded. This
	// throws the generated points away and builds a new set from the current nav mesh.
	CON_COMMAND_F( dm_reset_spawns, "Remove the generated deathmatch spawn points and build a new set from the nav mesh.", FCVAR_GAMEDLL )
	{
		if ( mp_randomspawn.GetInt() == 0 )
		{
			Msg( "dm_reset_spawns: random spawning is off, set mp_randomspawn first.\n" );
			return;
		}

		if ( g_pMapInfo && g_pMapInfo->m_bDisableAutoGeneratedDMSpawns )
		{
			Msg( "dm_reset_spawns: this map sets disableautogenerateddmspawns, so it has no generated spawn points.\n" );
			return;
		}

		if ( !TheNavMesh || !TheNavMesh->IsLoaded() )
		{
			Msg( "dm_reset_spawns: no nav mesh is loaded.\n" );
			return;
		}

		int nRemoved = 0;

		for ( int i = 0; i < g_DMSpawnPoints.Count(); ++i )
		{
			CBaseEntity *pDMSpawn = g_DMSpawnPoints[i];

			if ( pDMSpawn )
			{
				UTIL_Remove( pDMSpawn );
				++nRemoved;
			}
		}

		// Target the full population again: a map that had run out of areas may have shrunk the target
		// to what it managed to place, and a new nav mesh can support more.
		ResetDMSpawnState();

		// Fill the population now rather than waiting for the next Think. The rest of the target
		// arrives over the following seconds, one generator step per second.
		MaintainDMSpawnPopulation();

		Msg( "dm_reset_spawns: removed %d spawn points, %d of %d placed.\n", nRemoved, g_DMSpawnPoints.Count(), g_nDesiredDMSpawns );
	}

	// Returns true if no living member of nHideFromTeam (or of either team when 0) can see the spot.
	// CS:GO cs_gamerules.cpp:15978
	bool CCSGameRules::IsSpawnPointHiddenFromOtherPlayers( CBaseEntity *pSpot, CBasePlayer *pPlayer, int nHideFromTeam )
	{
		Vector vecSpot = pSpot->GetAbsOrigin() + Vector( 0, 0, 32 );

		if ( nHideFromTeam > 0 )
		{
			if ( nHideFromTeam == TEAM_CT && UTIL_IsVisibleToTeam( vecSpot, TEAM_CT ) )
				return false;
			else if ( nHideFromTeam == TEAM_TERRORIST && UTIL_IsVisibleToTeam( vecSpot, TEAM_TERRORIST ) )
				return false;
		}
		else if ( nHideFromTeam == 0 && ( UTIL_IsVisibleToTeam( vecSpot, TEAM_CT ) ) ||
			( UTIL_IsVisibleToTeam( vecSpot, TEAM_TERRORIST ) ) )
			return false;

		return true;
	}

	bool CCSGameRules::IsThereABomb()
	{
		bool bBombFound = false;

		/* are there any bombs, either laying around, or in someone's inventory? */
		if( gEntList.FindEntityByClassname( NULL, WEAPON_C4_CLASSNAME ) != 0 )
		{
			bBombFound = true;
		}
		/* what about planted bombs!? */
		else if( gEntList.FindEntityByClassname( NULL, PLANTED_C4_CLASSNAME ) != 0 )
		{
			bBombFound = true;
		}
		
		return bBombFound;
	}

	void CCSGameRules::HostageTouched()
	{
		if( gpGlobals->curtime > m_flNextHostageAnnouncement && m_iRoundWinStatus == WINNER_NONE )
		{
			//BroadcastSound( "Event.HostageTouched" );
			m_flNextHostageAnnouncement = gpGlobals->curtime + 60.0;
		}		

		AddHostageRescueTime();
	}

	//-----------------------------------------------------------------------------
	// Purpose: mp_hostages_rescuetime. The first time a CT reaches a hostage this round the round
	//			gets extra time, once per round and never after the round is already decided
	//			(ref cs_gamerules.cpp:7200-7215). m_iRoundTime is networked and the round timer reads
	//			it live, so the clock on the HUD moves immediately.
	//-----------------------------------------------------------------------------
	void CCSGameRules::AddHostageRescueTime( void )
	{
		if ( m_bAnyHostageReached )
			return;

		m_bAnyHostageReached = true;

		if ( m_iRoundWinStatus != WINNER_NONE )
			return;

		const int nExtraTime = (int)( mp_hostages_rescuetime.GetFloat() * 60.0f );

		if ( nExtraTime <= 0 )
			return;

		m_iRoundTime = m_iRoundTime.Get() + nExtraTime;

		// CS:S has no "#hostagerescuetime" token in its localization files.
		UTIL_ClientPrintAll( HUD_PRINTTALK, "Hostage reached: extra round time added" );
	}

	void CCSGameRules::CreateStandardEntities()
	{
		// Create the player resource
		g_pPlayerResource = (CPlayerResource*)CBaseEntity::Create( "cs_player_manager", vec3_origin, vec3_angle );
	
		// Create the entity that will send our data to the client.
#ifdef _DEBUG
		CBaseEntity *pEnt = 
#endif
			CBaseEntity::Create( "cs_gamerules", vec3_origin, vec3_angle );
		Assert( pEnt );
	}

#define MY_USHRT_MAX	0xffff
#define MY_UCHAR_MAX	0xff

bool DataHasChanged( void )
{
	for ( int i = 0; i < CS_NUM_LEVELS; i++ )
	{
		if ( g_iTerroristVictories[i] != 0 || g_iCounterTVictories[i] != 0 )
			return true;
	}

	for ( int i = 0; i < WEAPON_MAX; i++ )
	{
		if ( g_iWeaponPurchases[i] != 0 )
			return true;
	}

	return false;
}

void CCSGameRules::UploadGameStats( void )
{
	g_flGameStatsUpdateTime -= gpGlobals->curtime;

	if ( g_flGameStatsUpdateTime > 0 )
		return;

	if ( IsBlackMarket() == false )
		return;

	if ( m_bDontUploadStats == true )
		return;

	if ( DataHasChanged() == true )
	{
		cs_gamestats_t stats;
		memset( &stats, 0, sizeof(stats) );

		// Header
		stats.header.iVersion = CS_STATS_BLOB_VERSION;
		Q_strncpy( stats.header.szGameName, "cstrike", sizeof(stats.header.szGameName) );
		Q_strncpy( stats.header.szMapName, STRING( gpGlobals->mapname ), sizeof( stats.header.szMapName ) );

		ConVar *hostip = cvar->FindVar( "hostip" );
		if ( hostip )
		{
			int ip = hostip->GetInt();
			stats.header.ipAddr[0] = ip >> 24;
			stats.header.ipAddr[1] = ( ip >> 16 ) & MY_UCHAR_MAX;
			stats.header.ipAddr[2] = ( ip >> 8 ) & MY_UCHAR_MAX;
			stats.header.ipAddr[3] = ( ip ) & MY_UCHAR_MAX;
		}			

		ConVar *hostport = cvar->FindVar( "hostip" );
		if ( hostport )
		{
			stats.header.port = hostport->GetInt();
		}			

		stats.header.serverid = 0;

		stats.iMinutesPlayed = clamp( (short)( gpGlobals->curtime / 60 ), 0, MY_USHRT_MAX ); 

		memcpy( stats.iTerroristVictories, g_iTerroristVictories, sizeof( g_iTerroristVictories) );
		memcpy( stats.iCounterTVictories, g_iCounterTVictories, sizeof( g_iCounterTVictories) );
		memcpy( stats.iBlackMarketPurchases, g_iWeaponPurchases, sizeof( g_iWeaponPurchases) );

		stats.iAutoBuyPurchases = g_iAutoBuyPurchases;
		stats.iReBuyPurchases = g_iReBuyPurchases;

		stats.iAutoBuyM4A1Purchases = g_iAutoBuyM4A1Purchases;
		stats.iAutoBuyAK47Purchases = g_iAutoBuyAK47Purchases;
		stats.iAutoBuyFamasPurchases = g_iAutoBuyFamasPurchases;
		stats.iAutoBuyGalilPurchases = g_iAutoBuyGalilPurchases;
		stats.iAutoBuyVestHelmPurchases = g_iAutoBuyVestHelmPurchases;
		stats.iAutoBuyVestPurchases = g_iAutoBuyVestPurchases;

		const void *pvBlobData = ( const void * )( &stats );
		unsigned int uBlobSize = sizeof( stats );

		if ( gamestatsuploader )
		{
			gamestatsuploader->UploadGameStats( 
				STRING( gpGlobals->mapname ),
				CS_STATS_BLOB_VERSION,
				uBlobSize,
				pvBlobData );
		}


		memset( g_iWeaponPurchases, 0, sizeof( g_iWeaponPurchases) );
		memset( g_iTerroristVictories, 0, sizeof( g_iTerroristVictories) );
		memset( g_iCounterTVictories, 0, sizeof( g_iTerroristVictories) );

		g_iAutoBuyPurchases = 0;
		g_iReBuyPurchases = 0;

		g_iAutoBuyM4A1Purchases = 0;
		g_iAutoBuyAK47Purchases = 0;
		g_iAutoBuyFamasPurchases = 0;
		g_iAutoBuyGalilPurchases = 0;
		g_iAutoBuyVestHelmPurchases = 0;
		g_iAutoBuyVestPurchases = 0;
	}

	g_flGameStatsUpdateTime = CS_GAME_STATS_UPDATE_PERIOD; //Next update is between 22 and 24 hours.
}
#endif	// CLIENT_DLL

CBaseCombatWeapon *CCSGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	CBaseCombatWeapon *bestWeapon = NULL;

	// search all the weapons looking for the closest next
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *weapon = pPlayer->GetWeapon(i);
		if ( !weapon )
			continue;

		if ( !weapon->CanBeSelected() || weapon == pCurrentWeapon )
			continue;

#ifndef CLIENT_DLL
		CCSPlayer *csPlayer = ToCSPlayer(pPlayer);
		CWeaponCSBase *csWeapon = static_cast< CWeaponCSBase * >(weapon);
		if ( csPlayer && csPlayer->IsBot() && !TheCSBots()->IsWeaponUseable( csWeapon ) )
			continue;
#endif // CLIENT_DLL

		if ( bestWeapon )
		{
			if ( weapon->GetSlot() < bestWeapon->GetSlot() )
			{
				bestWeapon = weapon;
			}
			else if ( weapon->GetSlot() == bestWeapon->GetSlot() && weapon->GetPosition() < bestWeapon->GetPosition() )
			{
				bestWeapon = weapon;
			}
		}
		else
		{
			bestWeapon = weapon;
		}
	}

	return bestWeapon;
}

float CCSGameRules::GetMapRemainingTime()
{
#ifdef GAME_DLL
	if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
	{
		return 0;
	}
#endif

	// if timelimit is disabled, return -1
	if ( mp_timelimit.GetInt() <= 0 )
		return -1;

	// timelimit is in minutes
	float flTimeLeft =  ( m_flGameStartTime + mp_timelimit.GetInt() * 60 ) - gpGlobals->curtime;

	// never return a negative value
	if ( flTimeLeft < 0 )
		flTimeLeft = 0;

	return flTimeLeft;
}

float CCSGameRules::GetMapElapsedTime( void )
{
	return gpGlobals->curtime;
}

float CCSGameRules::GetRoundRemainingTime()
{
	return (float) (m_fRoundStartTime + m_iRoundTime) - gpGlobals->curtime; 
}

float CCSGameRules::GetRoundStartTime()
{
	return m_fRoundStartTime;
}


float CCSGameRules::GetRoundElapsedTime()
{
	return gpGlobals->curtime - m_fRoundStartTime;
}


bool CCSGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		swap(collisionGroup0,collisionGroup1);
	}
	
	//Don't stand on COLLISION_GROUP_WEAPONs
	if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}

	// TODO: make a CS-SPECIFIC COLLISION GROUP FOR PHYSICS PROPS THAT USE THIS COLLISION BEHAVIOR.

	
	if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		return false;
	}

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		// let debris and multiplayer objects collide
		return true;
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}


bool CCSGameRules::IsFreezePeriod()
{
	return m_bFreezePeriod;
}


bool CCSGameRules::IsVIPMap() const
{
	//MIKETODO: VIP mode
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: True while the match is in intermission (the end-of-match countdown before the level
//			changes). Declared in cs_gamerules.h but never implemented in this tree, so calling it
//			left an unresolved symbol in the server DLL and the engine crashed when loading it.
//			Mirrors CHL2MPRules::IsIntermission().
//-----------------------------------------------------------------------------
bool CCSGameRules::IsIntermission() const
{
#ifndef CLIENT_DLL
	return m_flIntermissionEndTime > gpGlobals->curtime;
#endif

	return false;
}

bool CCSGameRules::IsBombDefuseMap() const
{
	return m_bMapHasBombTarget;
}

bool CCSGameRules::IsHostageRescueMap() const
{
	return m_bMapHasRescueZone;
}

bool CCSGameRules::IsLogoMap() const
{
	return m_bLogoMap;
}

float CCSGameRules::GetBuyTimeLength() const
{
	return ( mp_buytime.GetFloat() * 60 );
}

bool CCSGameRules::IsBuyTimeElapsed()
{
	return ( GetRoundElapsedTime() > GetBuyTimeLength() );
}

int CCSGameRules::DefaultFOV()
{
	return 90;
}

const CViewVectors* CCSGameRules::GetViewVectors() const
{
	return &g_CSViewVectors;
}


//-----------------------------------------------------------------------------
// Purpose: Init CS ammo definitions
//-----------------------------------------------------------------------------

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			1	

// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


static CCSAmmoDef ammoDef;
CCSAmmoDef* GetCSAmmoDef()
{
	GetAmmoDef(); // to initialize the ammo info
	return &ammoDef;
}

CAmmoDef* GetAmmoDef()
{
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;
		
		ammoDef.AddAmmoType( BULLET_PLAYER_50AE,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_50AE_max",		2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_762MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_762mm_max",	2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_556MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_max",	2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_556MM_BOX,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_box_max",2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_338MAG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_338mag_max",	2800 * BULLET_IMPULSE_EXAGGERATION, 0, 12, 16 );
		ammoDef.AddAmmoType( BULLET_PLAYER_9MM,			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_9mm_max",		2000 * BULLET_IMPULSE_EXAGGERATION, 0, 5, 10 );
		ammoDef.AddAmmoType( BULLET_PLAYER_BUCKSHOT,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_buckshot_max", 600 * BULLET_IMPULSE_EXAGGERATION,  0, 3, 6 );
		ammoDef.AddAmmoType( BULLET_PLAYER_45ACP,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_45acp_max",	2100 * BULLET_IMPULSE_EXAGGERATION, 0, 6, 10 );
		ammoDef.AddAmmoType( BULLET_PLAYER_357SIG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_max",	2000 * BULLET_IMPULSE_EXAGGERATION, 0, 4, 8 );
		ammoDef.AddAmmoType( BULLET_PLAYER_57MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_57mm_max",		2000 * BULLET_IMPULSE_EXAGGERATION, 0, 4, 8 );
		ammoDef.AddAmmoType( AMMO_TYPE_HEGRENADE,		DMG_BLAST,	TRACER_LINE, 0, 0,	1/*max carry*/, 1, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_FLASHBANG,		0,			TRACER_LINE, 0,	0,	2/*max carry*/, 1, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_SMOKEGRENADE,	0,			TRACER_LINE, 0, 0,	1/*max carry*/, 1, 0 );

		//Adrian: I set all the prices to 0 just so the rest of the buy code works
		//This should be revisited.
		ammoDef.AddAmmoCost( BULLET_PLAYER_50AE, 0, 7 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_762MM, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_556MM, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_556MM_BOX, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_338MAG, 0, 10 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_9MM, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_BUCKSHOT, 0, 8 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_45ACP, 0, 25 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG, 0, 13 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_57MM, 0, 50 );
	}

	return &ammoDef;
}

#ifndef CLIENT_DLL
const char *CCSGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	char *pszPrefix = NULL;

	if ( !pPlayer )  // dedicated server output
	{
		pszPrefix = "";
	}
	else
	{
		// team only
		if ( bTeamOnly == TRUE )
		{
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				if ( pPlayer->m_lifeState == LIFE_ALIVE )
				{
					pszPrefix = "(Counter-Terrorist)";
				}
				else 
				{
					pszPrefix = "*DEAD*(Counter-Terrorist)";
				}
			}
			else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			{
				if ( pPlayer->m_lifeState == LIFE_ALIVE )
				{
					pszPrefix = "(Terrorist)";
				}
				else
				{
					pszPrefix = "*DEAD*(Terrorist)";
				}
			}
			else if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			{
				pszPrefix = "(Spectator)";
			}
		}
		// everyone
		else
		{
			if ( pPlayer->m_lifeState == LIFE_ALIVE )
			{
				pszPrefix = "";
			}
			else
			{
				if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
				{
					pszPrefix = "*DEAD*";	
				}
				else
				{
					pszPrefix = "*SPEC*";
				}
			}
		}
	}

	return pszPrefix;
}

const char *CCSGameRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )  // dedicated server output
	{
		return NULL;
	}

	// only teammates see locations
	if ( !bTeamOnly )
		return NULL;

	// only living players have locations
	if ( pPlayer->GetTeamNumber() != TEAM_CT && pPlayer->GetTeamNumber() != TEAM_TERRORIST )
		return NULL;

	if ( !pPlayer->IsAlive() )
		return NULL;

	return pPlayer->GetLastKnownPlaceName();
}

const char *CCSGameRules::GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )  // dedicated server output
	{
		return NULL;
	}

	const char *pszFormat = NULL;

	// team only
	if ( bTeamOnly == TRUE )
	{
		if ( pPlayer->GetTeamNumber() == TEAM_CT )
		{
			if ( pPlayer->m_lifeState == LIFE_ALIVE )
			{
				const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
				if ( chatLocation && *chatLocation )
				{
					pszFormat = "Cstrike_Chat_CT_Loc";
				}
				else
				{
					pszFormat = "Cstrike_Chat_CT";
				}
			}
			else 
			{
				pszFormat = "Cstrike_Chat_CT_Dead";
			}
		}
		else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
			if ( pPlayer->m_lifeState == LIFE_ALIVE )
			{
				const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
				if ( chatLocation && *chatLocation )
				{
					pszFormat = "Cstrike_Chat_T_Loc";
				}
				else
				{
					pszFormat = "Cstrike_Chat_T";
				}
			}
			else
			{
				pszFormat = "Cstrike_Chat_T_Dead";
			}
		}
		else if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			pszFormat = "Cstrike_Chat_Spec";
		}
	}
	// everyone
	else
	{
		if ( pPlayer->m_lifeState == LIFE_ALIVE )
		{
			pszFormat = "Cstrike_Chat_All";
		}
		else
		{
			if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			{
				pszFormat = "Cstrike_Chat_AllDead";	
			}
			else
			{
				pszFormat = "Cstrike_Chat_AllSpec";
			}
		}
	}

	return pszFormat;
}

void CCSGameRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	const char *pszNewName = engine->GetClientConVarValue( pPlayer->entindex(), "name" );

	const char *pszOldName = pPlayer->GetPlayerName();

	CCSPlayer *pCSPlayer = (CCSPlayer*)pPlayer;

	// first check if new name is really different
	if (  pszOldName[0] != 0 && 
		  Q_strncmp( pszOldName, pszNewName, MAX_PLAYER_NAME_LENGTH-1 ) )
	{
		// ok, player send different name

		// check if player is allowed to change it's name
		if ( pCSPlayer->CanChangeName() )
		{
			// dont allow name changes for dead players except bots
			if ( pCSPlayer->m_lifeState != LIFE_ALIVE && !pCSPlayer->IsBot() )
			{
				//  tell engine to continue using old name
				engine->ClientCommand( pPlayer->edict(), "name \"%s\"", pszOldName );

				// remember that name was changed
				Q_snprintf( pCSPlayer->m_szNewName, sizeof( pCSPlayer->m_szNewName ), "%s", pszNewName );
			
				// tell client that it's name will be changed next round
				ClientPrint( pCSPlayer, HUD_PRINTTALK, "#Name_change_at_respawn" );
			}
			else
			{
				// change name instantly
				pCSPlayer->ChangeName( pszNewName );
			}
		}
		else
		{
			// no change allowed, force engine to use old name again
			engine->ClientCommand( pPlayer->edict(), "name \"%s\"", pszOldName );

			// tell client that he hit the name change time limit
			ClientPrint( pCSPlayer, HUD_PRINTTALK, "#Name_change_limit_exceeded" );
		}
	}
	

	pCSPlayer->m_bShowHints = true;
	if ( pCSPlayer->IsNetClient() )
	{
		const char *pShowHints = engine->GetClientConVarValue( engine->IndexOfEdict( pCSPlayer->edict() ), "cl_autohelp" );
		if ( pShowHints && atoi( pShowHints ) <= 0 )
		{
			pCSPlayer->m_bShowHints = false;
		}
	}
}

bool CCSGameRules::FAllowNPCs( void )
{
	return false;
}

bool CCSGameRules::IsFriendlyFireOn( void )
{
	// mp_teammates_are_enemies makes teammates valid targets, so friendly fire is always on.
	return friendlyfire.GetBool() || mp_teammates_are_enemies.GetBool();
}

//-----------------------------------------------------------------------------
// Purpose: mp_teammates_are_enemies. Teammates stop being teammates for every rule that asks
//			(damage, team-kill penalties, bots), so they can be shot exactly like an enemy.
//-----------------------------------------------------------------------------
int CCSGameRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( mp_teammates_are_enemies.GetBool() )
		return GR_NOTTEAMMATE;

	return BaseClass::PlayerRelationship( pPlayer, pTarget );
}

CON_COMMAND( map_showspawnpoints, "Shows player spawn points (red=invalid)" )
{
	CSGameRules()->ShowSpawnPoints();
}

void DrawSphere( const Vector& pos, float radius, int r, int g, int b, float lifetime )
{
	Vector edge, lastEdge;
	NDebugOverlay::Line( pos, pos + Vector( 0, 0, 50 ), r, g, b, true, lifetime );

	lastEdge = Vector( radius + pos.x, pos.y, pos.z );
	float angle;
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = radius * BotCOS( angle ) + pos.x;
		edge.y = pos.y;
		edge.z = radius * BotSIN( angle ) + pos.z;

		NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

		lastEdge = edge;
	}

	lastEdge = Vector( pos.x, radius + pos.y, pos.z );
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = pos.x;
		edge.y = radius * BotCOS( angle ) + pos.y;
		edge.z = radius * BotSIN( angle ) + pos.z;

		NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

		lastEdge = edge;
	}

	lastEdge = Vector( pos.x, radius + pos.y, pos.z );
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = radius * BotCOS( angle ) + pos.x;
		edge.y = radius * BotSIN( angle ) + pos.y;
		edge.z = pos.z;

		NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

		lastEdge = edge;
	}
}

CON_COMMAND_F( map_showbombradius, "Shows bomb radius from the center of each bomb site and planted bomb.", FCVAR_CHEAT )
{
	float flBombDamage = 500.0f;
	if ( g_pMapInfo )
		flBombDamage = g_pMapInfo->m_flBombRadius;
	float flBombRadius = flBombDamage * 3.5f;
	Msg( "Bomb Damage is %.0f, Radius is %.0f\n", flBombDamage, flBombRadius );

	CBaseEntity* ent = NULL;
	while ( ( ent = gEntList.FindEntityByClassname( ent, "func_bomb_target" ) ) != NULL )
	{
		const Vector &pos = ent->WorldSpaceCenter();
		DrawSphere( pos, flBombRadius, 255, 255, 0, 10 );
	}

	ent = NULL;
	while ( ( ent = gEntList.FindEntityByClassname( ent, "planted_c4" ) ) != NULL )
	{
		const Vector &pos = ent->WorldSpaceCenter();
		DrawSphere( pos, flBombRadius, 255, 0, 0, 10 );
	}
}

CON_COMMAND_F( map_setbombradius, "Sets the bomb radius for the map.", FCVAR_CHEAT )
{
	if ( engine->Cmd_Argc() != 2 )
		return;

	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pMapInfo )
		CBaseEntity::Create( "info_map_parameters", vec3_origin, vec3_angle );

	if ( !g_pMapInfo )
		return;

	g_pMapInfo->m_flBombRadius = atof( engine->Cmd_Argv( 1 ) );
	map_showbombradius();
}

void CreateBlackMarketString( void )
{
	g_StringTableBlackMarket = networkstringtable->CreateStringTable( "BlackMarketTable" , 1 );
}

int CCSGameRules::GetStartMoney( void )
{
	if ( IsBlackMarket() )
	{
		return atoi( mp_startmoney.GetDefault() );
	}

	return mp_startmoney.GetInt();
}

#endif


int CCSGameRules::GetBlackMarketPriceForWeapon( int iWeaponID )
{
	if ( m_pPrices == NULL )
	{
		GetBlackMarketPriceList();
	}

	return m_pPrices->iCurrentPrice[iWeaponID];
}

int CCSGameRules::GetBlackMarketPreviousPriceForWeapon( int iWeaponID )
{
	if ( m_pPrices == NULL )
	{
		GetBlackMarketPriceList();
	}

	return m_pPrices->iPreviousPrice[iWeaponID];
}

const weeklyprice_t *CCSGameRules::GetBlackMarketPriceList( void )
{
	if ( m_StringTableBlackMarket == NULL )
	{
		m_StringTableBlackMarket = networkstringtable->FindTable( CS_GAMERULES_BLACKMARKET_TABLE_NAME);
	}

	if ( m_pPrices == NULL )
	{
		int iSize = 0;
		m_pPrices = (const weeklyprice_t *)m_StringTableBlackMarket->GetStringUserData( 0, &iSize );
	}

	PrepareEquipmentInfo();

	return m_pPrices;
}

void CCSGameRules::SetBlackMarketPrices( bool bSetDefaults )
{
	for ( int i = 1; i < WEAPON_MAX; i++ )
	{
		if ( i == WEAPON_SHIELDGUN )
			continue;

		CCSWeaponInfo *info = GetWeaponInfo( (CSWeaponID)i );

		if ( info == NULL )
			continue;

		if ( bSetDefaults == false )
		{
			info->SetWeaponPrice( GetBlackMarketPriceForWeapon( i ) );
			info->SetPreviousPrice( GetBlackMarketPreviousPriceForWeapon( i ) );
		}
		else
		{
			info->SetWeaponPrice( info->GetDefaultPrice() );
		}
	}
}

#ifdef CLIENT_DLL

CCSGameRules::CCSGameRules()
{
	CSGameRules()->m_StringTableBlackMarket = NULL;
	m_pPrices = NULL;
	m_bBlackMarket = false;
}

void TestTable( void )
{
	CSGameRules()->m_StringTableBlackMarket = networkstringtable->FindTable( CS_GAMERULES_BLACKMARKET_TABLE_NAME);

	if ( CSGameRules()->m_StringTableBlackMarket == NULL )
		return;

	int iIndex = CSGameRules()->m_StringTableBlackMarket->FindStringIndex( "blackmarket_prices" );
	int iSize = 0;

	const weeklyprice_t *pPrices = NULL;
	
	pPrices = (const weeklyprice_t *)(CSGameRules()->m_StringTableBlackMarket)->GetStringUserData( iIndex, &iSize );
}

#ifdef DEBUG
ConCommand cs_testtable( "cs_testtable", TestTable );
#endif

#endif



