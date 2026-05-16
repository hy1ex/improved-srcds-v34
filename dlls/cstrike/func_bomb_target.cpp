//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Bomb target area
//
//=============================================================================//

#include "cbase.h"
#include "cs_player.h"
#include "func_bomb_target.h"
#include "cs_gamerules.h"

LINK_ENTITY_TO_CLASS( func_bomb_target, CBombTarget );

BEGIN_DATADESC( CBombTarget )
	DEFINE_FUNCTION( BombTargetTouch ),
	DEFINE_FUNCTION( BombTargetUse ),			//needed?

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "BombExplode", OnBombExplode ),

	// Outputs
	DEFINE_OUTPUT( m_OnBombExplode,	"BombExplode" ),

END_DATADESC()

void CBombTarget::Spawn()
{
	InitTrigger();
	SetTouch( &CBombTarget::BombTargetTouch );
	SetUse( &CBombTarget::BombTargetUse );
}

void CBombTarget::BombTargetTouch( CBaseEntity* pOther )
{
	CCSPlayer *p = dynamic_cast< CCSPlayer* >( pOther );
	if ( p )
	{
		if ( p->HasC4() )
		{
			p->m_bInBombZone = true;
			p->m_iBombSiteIndex = entindex();
			if ( !(p->m_iDisplayHistoryBits & DHF_IN_TARGET_ZONE) )
			{
				p->HintMessage( "#Hint_you_are_in_targetzone", false );
				p->m_iDisplayHistoryBits |= DHF_IN_TARGET_ZONE;
			}
		}
	}
}

void CBombTarget::BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//SUB_UseTargets( NULL, USE_TOGGLE, 0 );
	DevMsg( 2, "BombTargetUse does nothing\n" );
}

// Relay to our outputs
void CBombTarget::OnBombExplode( inputdata_t &inputdata )
{
	m_OnBombExplode.FireOutput(this, this);
}
