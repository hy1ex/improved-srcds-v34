//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "cs_gameinterface.h"
#include "AI_ResponseSystem.h"

#include "utllinkedlist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlLinkedList<CMapEntityRef, unsigned short> g_MapEntityRefs;

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = 1;  // allow single player for the test maps (but we default to multi)
	maxplayers = MAX_PLAYERS;
	
	defaultMaxPlayers = 32;	// Default to 32 players unless they change it.
}

class CCSMapLoadEntityFilter : public IMapEntityFilter
{
public:
	virtual bool ShouldCreateEntity( const char *pClassname )
	{
		// During map load, create all the entities.
		return true;
	}

	virtual CBaseEntity* CreateNextEntity( const char *pClassname )
	{
		CBaseEntity *pRet = CreateEntityByName( pClassname );

		CMapEntityRef ref;
		ref.m_iEdict = -1;
		ref.m_iSerialNumber = -1;

		if ( pRet )
		{
			ref.m_iEdict = pRet->entindex();
			if ( pRet->edict() )
				ref.m_iSerialNumber = pRet->edict()->m_NetworkSerialNumber;
		}

		g_MapEntityRefs.AddToTail( ref );
		return pRet;
	}
};

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
	if ( Q_strcmp( STRING(gpGlobals->mapname), "cs_" ) )
	{
		// don't precache AI responses (hostages) if it's not a hostage rescure map
		extern IResponseSystem *g_pResponseSystem;
		g_pResponseSystem->PrecacheResponses( false );	
	}
	
	g_MapEntityRefs.Purge();
	CCSMapLoadEntityFilter filter;
	MapEntity_ParseAllEntities( pMapEntities, &filter );
}
	

