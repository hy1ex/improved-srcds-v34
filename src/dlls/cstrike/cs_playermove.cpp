//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

extern ConVar mp_buy_anywhere;
extern ConVar mp_plant_c4_anywhere;
#include "player_command.h"
#include "igamemovement.h"
#include "in_buttons.h"
#include "ipredictionsystem.h"
#include "iservervehicle.h"
#include "cs_player.h"


static CMoveData g_MoveData;
CMoveData *g_pMoveData = &g_MoveData;

IPredictionSystem *IPredictionSystem::g_pPredictionSystems = NULL;


//-----------------------------------------------------------------------------
// Sets up the move data for TF2
//-----------------------------------------------------------------------------
class CCSPlayerMove : public CPlayerMove
{
DECLARE_CLASS( CCSPlayerMove, CPlayerMove );

public:
	virtual void	StartCommand( CBasePlayer *player, CUserCmd *cmd );
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move );
};

// PlayerMove Interface
static CCSPlayerMove g_PlayerMove;

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CPlayerMove *PlayerMove()
{
	return &g_PlayerMove;
}

//-----------------------------------------------------------------------------
// Main setup, finish
//-----------------------------------------------------------------------------

void CCSPlayerMove::StartCommand( CBasePlayer *player, CUserCmd *cmd )
{
	CCSPlayer *pPlayer = ToCSPlayer( player );

	// Reset this.. it gets reset each frame that we're in a bomb zone.
	pPlayer->m_bInBombZone = false;
	pPlayer->m_bInBuyZone = false;
	pPlayer->m_bInHostageRescueZone = false;

	// mp_plant_c4_anywhere: pin the networked bomb-zone bit for the same reason as the buy zone below.
	// CC4::PrimaryAttack() runs on the client during weapon prediction and both it and the stock C4 HUD
	// read this sendprop, so a plant allowed only by a server-side convar would never animate. The bit
	// still means "the C4 may be planted here"; m_iBombSiteIndex keeps coming from func_bomb_target and
	// stays -1 outside a zone.
	if ( mp_plant_c4_anywhere.GetBool() )
		pPlayer->m_bInBombZone = true;

	// mp_buy_anywhere: pin the networked buy-zone bit. The stock v34 client decides on its own
	// whether to open the buy menu and can only learn about buy zones through this sendprop, so it
	// has to stay set for a player who is allowed to buy outside a func_buyzone. The buy window is
	// the same gate as in CBuyZone::BuyZoneTouch: the bit means "this player may buy right now",
	// not "this player is standing in a zone".
	const int nBuyAnywhere = mp_buy_anywhere.GetInt();

	if ( ( nBuyAnywhere == 1 || nBuyAnywhere == pPlayer->GetTeamNumber() ) && pPlayer->IsBuyTimeAvailable() )
	{
		pPlayer->m_bInBuyZone = true;
	}

	BaseClass::StartCommand( player, cmd );
}

//-----------------------------------------------------------------------------
// Purpose: This is called pre player movement and copies all the data necessary
//          from the player for movement. (Server-side, the client-side version
//          of this code can be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CCSPlayerMove::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	player->AvoidPhysicsProps( ucmd );

	BaseClass::SetupMove( player, ucmd, pHelper, move );

	IServerVehicle *pVehicle = player->GetVehicle();
	if (pVehicle && gpGlobals->frametime != 0)
	{
		pVehicle->SetupMove( player, ucmd, pHelper, move ); 
	}
}


//-----------------------------------------------------------------------------
// Purpose: This is called post player movement to copy back all data that
//          movement could have modified and that is necessary for future
//          movement. (Server-side, the client-side version of this code can 
//          be found in prediction.cpp.)
//-----------------------------------------------------------------------------
void CCSPlayerMove::FinishMove( CBasePlayer *player, CUserCmd *ucmd, CMoveData *move )
{
	// Call the default FinishMove code.
	BaseClass::FinishMove( player, ucmd, move );

	IServerVehicle *pVehicle = player->GetVehicle();
	if (pVehicle && gpGlobals->frametime != 0)
	{
		pVehicle->FinishMove( player, ucmd, move );
	}
}
