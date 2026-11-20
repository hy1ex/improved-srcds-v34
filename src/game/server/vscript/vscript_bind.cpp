//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: The script-visible API. Every binding here validates its arguments
//          and only touches state the stock v34 client already understands:
//          entity fields, inputs, convars, game events and existing netprops.
//          No binding may add replicated state or client-side behaviour.
//
//=============================================================================//

#include "cbase.h"
#include "entitylist.h"
#include "eventqueue.h"
#include "dt_send.h"
#include "server_class.h"
#include "vscript.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar sv_vscript_allow_convars;

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------
namespace
{
	const char *ScriptValueToString( IScriptCallContext &ctx, int i, char *pszBuffer, int nBufferSize )
	{
		pszBuffer[0] = '\0';

		if ( !ctx.Exists( i ) )
			return pszBuffer;

		switch ( ctx.ArgType( i ) )
		{
		case SCRIPT_INT:
			Q_snprintf( pszBuffer, nBufferSize, "%d", ctx.ArgInt( i ) );
			break;
		case SCRIPT_FLOAT:
			Q_snprintf( pszBuffer, nBufferSize, "%f", ctx.ArgFloat( i ) );
			break;
		case SCRIPT_BOOL:
			Q_snprintf( pszBuffer, nBufferSize, "%d", ctx.ArgBool( i ) ? 1 : 0 );
			break;
		case SCRIPT_STRING:
			Q_strncpy( pszBuffer, ctx.ArgString( i ), nBufferSize );
			break;
		case SCRIPT_VECTOR:
			{
				Vector vec = ctx.ArgVector( i );
				Q_snprintf( pszBuffer, nBufferSize, "%f %f %f", vec.x, vec.y, vec.z );
			}
			break;
		case SCRIPT_ENTITY:
			{
				CBaseEntity *pEntity = ctx.ArgEntity( i );
				Q_strncpy( pszBuffer, pEntity ? pEntity->GetClassname() : "null", nBufferSize );
			}
			break;
		default:
			break;
		}

		return pszBuffer;
	}

	CBaseEntity *FindEntityByNameArg( CBaseEntity *pStart, const char *pszName )
	{
		if ( !pszName || !pszName[0] )
			return NULL;

		return gEntList.FindEntityByName( pStart, pszName );
	}

	CBaseEntity *FindEntityByClassnameArg( CBaseEntity *pStart, const char *pszClassname )
	{
		if ( !pszClassname || !pszClassname[0] )
			return NULL;

		return gEntList.FindEntityByClassname( pStart, pszClassname );
	}

	//-----------------------------------------------------------------------------
	// Netprops: resolved against the entity's server send table, never guessed.
	//-----------------------------------------------------------------------------
	struct NetPropRef_t
	{
		int				iOffset;
		SendPropType	type;
		int				nBits;
	};

	bool FindNetProp( SendTable *pTable, const char *pszName, int iBaseOffset, NetPropRef_t &out )
	{
		if ( !pTable )
			return false;

		for ( int i = 0; i < pTable->m_nProps; i++ )
		{
			SendProp *pProp = &pTable->m_pProps[i];
			if ( !pProp || !pProp->GetName() )
				continue;

			if ( pProp->GetType() == DPT_DataTable )
			{
				if ( FindNetProp( pProp->GetDataTable(), pszName, iBaseOffset + pProp->GetOffset(), out ) )
					return true;
				continue;
			}

			if ( Q_stricmp( pProp->GetName(), pszName ) )
				continue;

			out.iOffset = iBaseOffset + pProp->GetOffset();
			out.type = pProp->GetType();
			out.nBits = pProp->m_nBits;
			return true;
		}

		return false;
	}

	bool ResolveNetProp( CBaseEntity *pEntity, const char *pszName, NetPropRef_t &out )
	{
		if ( !pEntity || !pszName )
			return false;

		IServerNetworkable *pNetworkable = pEntity->GetNetworkable();
		if ( !pNetworkable )
			return false;

		ServerClass *pClass = pNetworkable->GetServerClass();
		if ( !pClass || !pClass->m_pTable )
			return false;

		return FindNetProp( pClass->m_pTable, pszName, 0, out );
	}

	int *NetPropIntPtr( CBaseEntity *pEntity, const NetPropRef_t &prop )
	{
		return reinterpret_cast<int *>( reinterpret_cast<unsigned char *>( pEntity ) + prop.iOffset );
	}

	float *NetPropFloatPtr( CBaseEntity *pEntity, const NetPropRef_t &prop )
	{
		return reinterpret_cast<float *>( reinterpret_cast<unsigned char *>( pEntity ) + prop.iOffset );
	}

	Vector *NetPropVectorPtr( CBaseEntity *pEntity, const NetPropRef_t &prop )
	{
		return reinterpret_cast<Vector *>( reinterpret_cast<unsigned char *>( pEntity ) + prop.iOffset );
	}

	char *NetPropStringPtr( CBaseEntity *pEntity, const NetPropRef_t &prop )
	{
		return reinterpret_cast<char *>( reinterpret_cast<unsigned char *>( pEntity ) + prop.iOffset );
	}

	bool NetPropIsIntLike( SendPropType type )
	{
		return type == DPT_Int;
	}

	// This SDK has no ConVarRef, so look the command up directly.
	ConVar *FindScriptConVar( const char *pszName )
	{
		if ( !pszName || !pszName[0] )
			return NULL;

		ConCommandBase *pBase = const_cast<ConCommandBase *>( ConCommandBase::FindCommand( pszName ) );
		if ( !pBase || pBase->IsCommand() )
			return NULL;

		return static_cast<ConVar *>( pBase );
	}
}

//-----------------------------------------------------------------------------
// Output
//-----------------------------------------------------------------------------
static bool Bind_print( IScriptCallContext &ctx )
{
	char szValue[512];
	Msg( "%s", ScriptValueToString( ctx, 0, szValue, sizeof( szValue ) ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_printl( IScriptCallContext &ctx )
{
	char szValue[512];
	Msg( "%s\n", ScriptValueToString( ctx, 0, szValue, sizeof( szValue ) ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Msg( IScriptCallContext &ctx )
{
	char szValue[512];
	Msg( "%s", ScriptValueToString( ctx, 0, szValue, sizeof( szValue ) ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Warning( IScriptCallContext &ctx )
{
	char szValue[512];
	Warning( "[VScript] %s\n", ScriptValueToString( ctx, 0, szValue, sizeof( szValue ) ) );
	ctx.ReturnNull();
	return true;
}

//-----------------------------------------------------------------------------
// Time and math
//-----------------------------------------------------------------------------
static bool Bind_Time( IScriptCallContext &ctx )
{
	ctx.ReturnFloat( gpGlobals->curtime );
	return true;
}

static bool Bind_FrameTime( IScriptCallContext &ctx )
{
	ctx.ReturnFloat( gpGlobals->frametime );
	return true;
}

static bool Bind_RandomInt( IScriptCallContext &ctx )
{
	ctx.ReturnInt( RandomInt( ctx.ArgInt( 0, 0 ), ctx.ArgInt( 1, 0 ) ) );
	return true;
}

static bool Bind_RandomFloat( IScriptCallContext &ctx )
{
	ctx.ReturnFloat( RandomFloat( ctx.ArgFloat( 0, 0.0f ), ctx.ArgFloat( 1, 1.0f ) ) );
	return true;
}

static bool Bind_Vector( IScriptCallContext &ctx )
{
	Vector vec( ctx.ArgFloat( 0, 0.0f ), ctx.ArgFloat( 1, 0.0f ), ctx.ArgFloat( 2, 0.0f ) );
	ctx.ReturnVector( vec );
	return true;
}

static bool Bind_GetMapName( IScriptCallContext &ctx )
{
	ctx.ReturnString( STRING( gpGlobals->mapname ) );
	return true;
}

//-----------------------------------------------------------------------------
// Script files
//-----------------------------------------------------------------------------
static bool Bind_IncludeScript( IScriptCallContext &ctx )
{
	const char *pszFile = ctx.ArgString( 0 );
	if ( !pszFile[0] )
	{
		ctx.Error( "IncludeScript needs a file name" );
		ctx.ReturnNull();
		return false;
	}

	ctx.ReturnBool( VScript_RunFileInScope( pszFile, VScript_GetCurrentScope(), true ) );
	return true;
}

static bool Bind_DoIncludeScript( IScriptCallContext &ctx )
{
	// The scope argument of Valve's version is a table; this implementation
	// always includes into the calling scope, which is the documented behaviour
	// for top-level and entity-scope scripts.
	const char *pszFile = ctx.ArgString( 0 );
	if ( !pszFile[0] )
	{
		ctx.Error( "DoIncludeScript needs a file name" );
		ctx.ReturnNull();
		return false;
	}

	ctx.ReturnBool( VScript_RunFileInScope( pszFile, VScript_GetCurrentScope(), true ) );
	return true;
}

//-----------------------------------------------------------------------------
// Assets
//-----------------------------------------------------------------------------
static bool Bind_PrecacheModel( IScriptCallContext &ctx )
{
	const char *pszModel = ctx.ArgString( 0 );
	if ( !pszModel[0] )
	{
		ctx.ReturnInt( 0 );
		return true;
	}

	ctx.ReturnInt( CBaseEntity::PrecacheModel( pszModel ) );
	return true;
}

static bool Bind_PrecacheSound( IScriptCallContext &ctx )
{
	const char *pszSound = ctx.ArgString( 0 );
	if ( !pszSound[0] )
	{
		ctx.ReturnBool( false );
		return true;
	}

	ctx.ReturnBool( CBaseEntity::PrecacheSound( pszSound ) );
	return true;
}

//-----------------------------------------------------------------------------
// Entity I/O and thinks
//-----------------------------------------------------------------------------
static bool Bind_EntFire( IScriptCallContext &ctx )
{
	char szValue[512];
	ScriptValueToString( ctx, 2, szValue, sizeof( szValue ) );

	VScript_QueueEntFire( NULL, ctx.ArgString( 0 ), ctx.ArgString( 1 ), szValue,
						  ctx.ArgFloat( 3, 0.0f ), ctx.ArgEntity( 4 ), ctx.ArgEntity( 4 ) );

	ctx.ReturnNull();
	return true;
}

static bool Bind_EntFireByHandle( IScriptCallContext &ctx )
{
	CBaseEntity *pTarget = ctx.ArgEntity( 0 );
	if ( !pTarget )
	{
		ctx.Error( "EntFireByHandle: invalid entity handle" );
		ctx.ReturnNull();
		return false;
	}

	char szValue[512];
	ScriptValueToString( ctx, 2, szValue, sizeof( szValue ) );

	VScript_QueueEntFire( pTarget, NULL, ctx.ArgString( 1 ), szValue,
						  ctx.ArgFloat( 3, 0.0f ), ctx.ArgEntity( 4 ), ctx.ArgEntity( 4 ) );

	ctx.ReturnNull();
	return true;
}

static bool Bind_AddThinkToEnt( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	const char *pszFunc = ctx.ArgString( 1 );

	if ( !pEntity || !pszFunc[0] )
	{
		ctx.Error( "AddThinkToEnt: entity and function name are required" );
		ctx.ReturnNull();
		return false;
	}

	VScript_AddThinkToEntity( pEntity, pszFunc, 0.0f );
	ctx.ReturnNull();
	return true;
}

//-----------------------------------------------------------------------------
// Game events
//-----------------------------------------------------------------------------
static bool Bind_ListenToGameEvent( IScriptCallContext &ctx )
{
	const char *pszEvent = ctx.ArgString( 0 );
	const char *pszFunc = ctx.ArgString( 1 );

	if ( !pszEvent[0] || !pszFunc[0] )
	{
		ctx.Error( "ListenToGameEvent: event and function name are required" );
		ctx.ReturnInt( 0 );
		return false;
	}

	ctx.ReturnInt( VScript_ListenToGameEvent( pszEvent, VScript_GetCurrentScope(), pszFunc ) );
	return true;
}

static bool Bind_StopListeningToGameEvent( IScriptCallContext &ctx )
{
	VScript_StopListeningToGameEvent( ctx.ArgInt( 0, 0 ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_CollectGameEventCallbacks( IScriptCallContext &ctx )
{
	VScript_CollectGameEventCallbacks( VScript_GetCurrentScope() );
	ctx.ReturnNull();
	return true;
}

//-----------------------------------------------------------------------------
// Entities table
//-----------------------------------------------------------------------------
static bool Bind_Entities_FindByName( IScriptCallContext &ctx )
{
	ctx.ReturnEntity( FindEntityByNameArg( ctx.ArgEntity( 0 ), ctx.ArgString( 1 ) ) );
	return true;
}

static bool Bind_Entities_FindByClassname( IScriptCallContext &ctx )
{
	ctx.ReturnEntity( FindEntityByClassnameArg( ctx.ArgEntity( 0 ), ctx.ArgString( 1 ) ) );
	return true;
}

static bool Bind_Entities_FindByNameWithin( IScriptCallContext &ctx )
{
	const char *pszName = ctx.ArgString( 1 );
	if ( !pszName[0] )
	{
		ctx.ReturnNull();
		return true;
	}

	ctx.ReturnEntity( gEntList.FindEntityByNameWithin( ctx.ArgEntity( 0 ), pszName,
													   ctx.ArgVector( 2 ), ctx.ArgFloat( 3, 0.0f ) ) );
	return true;
}

static bool Bind_Entities_FindByClassnameWithin( IScriptCallContext &ctx )
{
	const char *pszClassname = ctx.ArgString( 1 );
	if ( !pszClassname[0] )
	{
		ctx.ReturnNull();
		return true;
	}

	ctx.ReturnEntity( gEntList.FindEntityByClassnameWithin( ctx.ArgEntity( 0 ), pszClassname,
															ctx.ArgVector( 2 ), ctx.ArgFloat( 3, 0.0f ) ) );
	return true;
}

static bool Bind_Entities_FindInSphere( IScriptCallContext &ctx )
{
	ctx.ReturnEntity( gEntList.FindEntityInSphere( ctx.ArgEntity( 0 ), ctx.ArgVector( 1 ),
												   ctx.ArgFloat( 2, 0.0f ) ) );
	return true;
}

static bool Bind_Entities_First( IScriptCallContext &ctx )
{
	ctx.ReturnEntity( gEntList.FirstEnt() );
	return true;
}

static bool Bind_Entities_Next( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	ctx.ReturnEntity( pEntity ? gEntList.NextEnt( pEntity ) : NULL );
	return true;
}

static bool Bind_Entities_CreateByClassname( IScriptCallContext &ctx )
{
	const char *pszClassname = ctx.ArgString( 0 );
	if ( !pszClassname[0] )
	{
		ctx.ReturnNull();
		return true;
	}

	ctx.ReturnEntity( CreateEntityByName( pszClassname ) );
	return true;
}

//-----------------------------------------------------------------------------
// Convars
//-----------------------------------------------------------------------------
static bool Bind_Convars_GetStr( IScriptCallContext &ctx )
{
	ConVar *pVar = FindScriptConVar( ctx.ArgString( 0 ) );
	ctx.ReturnString( pVar ? pVar->GetString() : "" );
	return true;
}

static bool Bind_Convars_GetFloat( IScriptCallContext &ctx )
{
	ConVar *pVar = FindScriptConVar( ctx.ArgString( 0 ) );
	ctx.ReturnFloat( pVar ? pVar->GetFloat() : 0.0f );
	return true;
}

static bool Bind_Convars_GetInt( IScriptCallContext &ctx )
{
	ConVar *pVar = FindScriptConVar( ctx.ArgString( 0 ) );
	ctx.ReturnInt( pVar ? pVar->GetInt() : 0 );
	return true;
}

static bool Bind_Convars_SetValue( IScriptCallContext &ctx )
{
	if ( !sv_vscript_allow_convars.GetBool() )
	{
		ctx.Error( "Convars.SetValue is disabled (sv_vscript_allow_convars 0)" );
		ctx.ReturnNull();
		return false;
	}

	const char *pszName = ctx.ArgString( 0 );
	ConVar *pVar = FindScriptConVar( pszName );
	if ( !pVar )
	{
		ctx.Error( "Convars.SetValue: unknown convar '%s'", pszName );
		ctx.ReturnNull();
		return false;
	}

	char szValue[512];
	ScriptValueToString( ctx, 1, szValue, sizeof( szValue ) );
	pVar->SetValue( szValue );

	ctx.ReturnNull();
	return true;
}

//-----------------------------------------------------------------------------
// Netprops
//-----------------------------------------------------------------------------
static bool Bind_GetPropInt( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || !NetPropIsIntLike( prop.type ) )
	{
		ctx.ReturnInt( 0 );
		return true;
	}

	ctx.ReturnInt( *NetPropIntPtr( pEntity, prop ) );
	return true;
}

static bool Bind_SetPropInt( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || !NetPropIsIntLike( prop.type ) )
	{
		ctx.Error( "NetProps.SetPropInt: no int netprop '%s' on %s", ctx.ArgString( 1 ),
				   pEntity ? pEntity->GetClassname() : "null" );
		ctx.ReturnNull();
		return false;
	}

	*NetPropIntPtr( pEntity, prop ) = ctx.ArgInt( 2, 0 );
	ctx.ReturnNull();
	return true;
}

static bool Bind_GetPropFloat( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || prop.type != DPT_Float )
	{
		ctx.ReturnFloat( 0.0f );
		return true;
	}

	ctx.ReturnFloat( *NetPropFloatPtr( pEntity, prop ) );
	return true;
}

static bool Bind_SetPropFloat( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || prop.type != DPT_Float )
	{
		ctx.Error( "NetProps.SetPropFloat: no float netprop '%s' on %s", ctx.ArgString( 1 ),
				   pEntity ? pEntity->GetClassname() : "null" );
		ctx.ReturnNull();
		return false;
	}

	*NetPropFloatPtr( pEntity, prop ) = ctx.ArgFloat( 2, 0.0f );
	ctx.ReturnNull();
	return true;
}

static bool Bind_GetPropBool( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || !NetPropIsIntLike( prop.type ) )
	{
		ctx.ReturnBool( false );
		return true;
	}

	ctx.ReturnBool( *NetPropIntPtr( pEntity, prop ) != 0 );
	return true;
}

static bool Bind_SetPropBool( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || !NetPropIsIntLike( prop.type ) )
	{
		ctx.Error( "NetProps.SetPropBool: no bool netprop '%s' on %s", ctx.ArgString( 1 ),
				   pEntity ? pEntity->GetClassname() : "null" );
		ctx.ReturnNull();
		return false;
	}

	*NetPropIntPtr( pEntity, prop ) = ctx.ArgBool( 2, false ) ? 1 : 0;
	ctx.ReturnNull();
	return true;
}

static bool Bind_GetPropVector( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || prop.type != DPT_Vector )
	{
		ctx.ReturnVector( vec3_origin );
		return true;
	}

	ctx.ReturnVector( *NetPropVectorPtr( pEntity, prop ) );
	return true;
}

static bool Bind_SetPropVector( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || prop.type != DPT_Vector )
	{
		ctx.Error( "NetProps.SetPropVector: no vector netprop '%s' on %s", ctx.ArgString( 1 ),
				   pEntity ? pEntity->GetClassname() : "null" );
		ctx.ReturnNull();
		return false;
	}

	*NetPropVectorPtr( pEntity, prop ) = ctx.ArgVector( 2 );
	ctx.ReturnNull();
	return true;
}

static bool Bind_GetPropString( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || prop.type != DPT_String )
	{
		ctx.ReturnString( "" );
		return true;
	}

	ctx.ReturnString( NetPropStringPtr( pEntity, prop ) );
	return true;
}

static bool Bind_SetPropString( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || prop.type != DPT_String )
	{
		ctx.Error( "NetProps.SetPropString: no string netprop '%s' on %s", ctx.ArgString( 1 ),
				   pEntity ? pEntity->GetClassname() : "null" );
		ctx.ReturnNull();
		return false;
	}

	// The send table does not carry the buffer size for strings, so stay well
	// inside the engine's smallest declared string field.
	const char *pszValue = ctx.ArgString( 2 );
	Q_strncpy( NetPropStringPtr( pEntity, prop ), pszValue, 128 );

	ctx.ReturnNull();
	return true;
}

static bool Bind_GetPropEntity( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || !NetPropIsIntLike( prop.type ) )
	{
		ctx.ReturnNull();
		return true;
	}

	CBaseHandle handle( *(unsigned long *)NetPropIntPtr( pEntity, prop ) );
	ctx.ReturnEntity( handle.Get() );
	return true;
}

static bool Bind_SetPropEntity( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.ArgEntity( 0 );
	NetPropRef_t prop;
	if ( !ResolveNetProp( pEntity, ctx.ArgString( 1 ), prop ) || !NetPropIsIntLike( prop.type ) )
	{
		ctx.Error( "NetProps.SetPropEntity: no entity netprop '%s' on %s", ctx.ArgString( 1 ),
				   pEntity ? pEntity->GetClassname() : "null" );
		ctx.ReturnNull();
		return false;
	}

	CBaseEntity *pTarget = ctx.ArgEntity( 2 );
	CBaseHandle handle = pTarget ? pTarget->GetRefEHandle() : CBaseHandle();
	*(unsigned long *)NetPropIntPtr( pEntity, prop ) = (unsigned long)handle.ToInt();

	ctx.ReturnNull();
	return true;
}

//-----------------------------------------------------------------------------
// CBaseEntity methods
//-----------------------------------------------------------------------------
static CBaseEntity *SelfEntity( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = ctx.Self();
	if ( !pEntity )
	{
		ctx.Error( "method called on an invalid or destroyed entity" );
		ctx.ReturnNull();
	}
	return pEntity;
}

static bool Bind_Ent_IsValid( IScriptCallContext &ctx )
{
	ctx.ReturnBool( ctx.Self() != NULL );
	return true;
}

static bool Bind_Ent_GetName( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnString( pEntity ? STRING( pEntity->GetEntityName() ) : "" );
	return true;
}

static bool Bind_Ent_SetName( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	pEntity->SetName( AllocPooledString( ctx.ArgString( 0 ) ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_GetClassname( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnString( pEntity ? pEntity->GetClassname() : "" );
	return true;
}

static bool Bind_Ent_entindex( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnInt( pEntity ? pEntity->entindex() : -1 );
	return true;
}

static bool Bind_Ent_GetOrigin( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnVector( pEntity ? pEntity->GetAbsOrigin() : vec3_origin );
	return true;
}

static bool Bind_Ent_SetOrigin( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	pEntity->SetAbsOrigin( ctx.ArgVector( 0 ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_GetAngles( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
	{
		ctx.ReturnVector( vec3_origin );
		return true;
	}

	QAngle angles = pEntity->GetAbsAngles();
	ctx.ReturnVector( Vector( angles.x, angles.y, angles.z ) );
	return true;
}

static bool Bind_Ent_SetAngles( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	Vector vecAngles = ctx.ArgVector( 0 );
	pEntity->SetAbsAngles( QAngle( vecAngles.x, vecAngles.y, vecAngles.z ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_GetForwardVector( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
	{
		ctx.ReturnVector( vec3_origin );
		return true;
	}

	Vector vecForward;
	AngleVectors( pEntity->GetAbsAngles(), &vecForward );
	ctx.ReturnVector( vecForward );
	return true;
}

static bool Bind_Ent_GetHealth( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnInt( pEntity ? pEntity->GetHealth() : 0 );
	return true;
}

static bool Bind_Ent_SetHealth( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	pEntity->SetHealth( ctx.ArgInt( 0, 0 ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_GetMaxHealth( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnInt( pEntity ? pEntity->GetMaxHealth() : 0 );
	return true;
}

static bool Bind_Ent_SetMaxHealth( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	pEntity->SetMaxHealth( ctx.ArgInt( 0, 0 ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_GetVelocity( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnVector( pEntity ? pEntity->GetAbsVelocity() : vec3_origin );
	return true;
}

static bool Bind_Ent_SetVelocity( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	pEntity->SetAbsVelocity( ctx.ArgVector( 0 ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_GetTeamNumber( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnInt( pEntity ? pEntity->GetTeamNumber() : 0 );
	return true;
}

static bool Bind_Ent_SetTeam( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	CBasePlayer *pPlayer = ToBasePlayer( pEntity );
	if ( pPlayer )
		pPlayer->ChangeTeam( ctx.ArgInt( 0, 0 ) );

	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_IsAlive( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnBool( pEntity ? pEntity->IsAlive() : false );
	return true;
}

static bool Bind_Ent_IsPlayer( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnBool( pEntity ? pEntity->IsPlayer() : false );
	return true;
}

static bool Bind_Ent_GetModelName( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	ctx.ReturnString( pEntity ? STRING( pEntity->GetModelName() ) : "" );
	return true;
}

static bool Bind_Ent_SetModel( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	pEntity->SetModel( ctx.ArgString( 0 ) );
	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_Kill( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( pEntity )
		UTIL_Remove( pEntity );

	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_TakeDamage( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	CBaseEntity *pAttacker = ctx.ArgEntity( 1 );
	CTakeDamageInfo info( pAttacker, pAttacker, ctx.ArgFloat( 0, 0.0f ), DMG_GENERIC );
	pEntity->TakeDamage( info );

	ctx.ReturnNull();
	return true;
}

static bool Bind_Ent_ValidateScriptScope( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	HSCRIPT hScope = VScript_GetOwnerScope( pEntity );
	if ( !hScope )
		hScope = VScript_CreateScope( pEntity );

	ctx.ReturnScope( hScope );
	return true;
}

static bool Bind_Ent_GetScriptScope( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
		return false;

	ctx.ReturnScope( VScript_GetOwnerScope( pEntity ) );
	return true;
}

static bool Bind_Ent_GetEyePosition( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	if ( !pEntity )
	{
		ctx.ReturnVector( vec3_origin );
		return true;
	}

	CBasePlayer *pPlayer = ToBasePlayer( pEntity );
	ctx.ReturnVector( pPlayer ? pPlayer->EyePosition() : pEntity->GetAbsOrigin() );
	return true;
}

static bool Bind_Ent_GetPlayerName( IScriptCallContext &ctx )
{
	CBaseEntity *pEntity = SelfEntity( ctx );
	CBasePlayer *pPlayer = ToBasePlayer( pEntity );
	ctx.ReturnString( pPlayer ? pPlayer->GetPlayerName() : "" );
	return true;
}

//-----------------------------------------------------------------------------
// Vector metamethods
//-----------------------------------------------------------------------------
static bool Bind_Vec_add( IScriptCallContext &ctx )
{
	ctx.ReturnVector( ctx.ArgVector( 0 ) + ctx.ArgVector( 1 ) );
	return true;
}

static bool Bind_Vec_sub( IScriptCallContext &ctx )
{
	ctx.ReturnVector( ctx.ArgVector( 0 ) - ctx.ArgVector( 1 ) );
	return true;
}

static bool Bind_Vec_mul( IScriptCallContext &ctx )
{
	Vector vec = ctx.ArgVector( 0 );
	if ( ctx.ArgType( 1 ) == SCRIPT_INT || ctx.ArgType( 1 ) == SCRIPT_FLOAT )
	{
		ctx.ReturnVector( vec * ctx.ArgFloat( 1, 1.0f ) );
		return true;
	}

	Vector vecOther = ctx.ArgVector( 1, Vector( 1, 1, 1 ) );
	ctx.ReturnVector( Vector( vec.x * vecOther.x, vec.y * vecOther.y, vec.z * vecOther.z ) );
	return true;
}

static bool Bind_Vec_div( IScriptCallContext &ctx )
{
	Vector vec = ctx.ArgVector( 0 );
	float flDivisor = ( ctx.ArgType( 1 ) == SCRIPT_INT || ctx.ArgType( 1 ) == SCRIPT_FLOAT )
		? ctx.ArgFloat( 1, 1.0f ) : 1.0f;

	if ( flDivisor == 0.0f )
		flDivisor = 1.0f;

	ctx.ReturnVector( vec / flDivisor );
	return true;
}

static bool Bind_Vec_unm( IScriptCallContext &ctx )
{
	ctx.ReturnVector( -ctx.ArgVector( 0 ) );
	return true;
}

static bool Bind_Vec_Length( IScriptCallContext &ctx )
{
	ctx.ReturnFloat( ctx.ArgVector( 0 ).Length() );
	return true;
}

static bool Bind_Vec_typeof( IScriptCallContext &ctx )
{
	ctx.ReturnString( "Vector" );
	return true;
}

//-----------------------------------------------------------------------------
// Binding tables
//-----------------------------------------------------------------------------
const ScriptBinding_t g_ScriptGlobalBindings[] =
{
	{ "print",						Bind_print,						"print(value) - write a value to the server console" },
	{ "printl",						Bind_printl,					"printl(value) - write a line to the server console" },
	{ "Msg",						Bind_Msg,						"Msg(value) - write a value to the server console" },
	{ "Warning",					Bind_Warning,					"Warning(value) - write a warning to the server console" },
	{ "Time",						Bind_Time,						"Time() - current server time" },
	{ "FrameTime",					Bind_FrameTime,					"FrameTime() - length of the last server frame" },
	{ "RandomInt",					Bind_RandomInt,					"RandomInt(min, max)" },
	{ "RandomFloat",				Bind_RandomFloat,				"RandomFloat(min, max)" },
	{ "Vector",						Bind_Vector,					"Vector(x, y, z)" },
	{ "GetMapName",					Bind_GetMapName,				"GetMapName() - current map name" },
	{ "IncludeScript",				Bind_IncludeScript,				"IncludeScript(file) - run a .nut file in the calling scope" },
	{ "DoIncludeScript",			Bind_DoIncludeScript,			"DoIncludeScript(file) - run a .nut file in the calling scope" },
	{ "PrecacheModel",				Bind_PrecacheModel,				"PrecacheModel(model)" },
	{ "PrecacheSound",				Bind_PrecacheSound,				"PrecacheSound(sound)" },
	{ "EntFire",					Bind_EntFire,					"EntFire(target, input, value, delay, activator)" },
	{ "EntFireByHandle",			Bind_EntFireByHandle,			"EntFireByHandle(entity, input, value, delay, activator)" },
	{ "AddThinkToEnt",				Bind_AddThinkToEnt,				"AddThinkToEnt(entity, function)" },
	{ "ListenToGameEvent",			Bind_ListenToGameEvent,			"ListenToGameEvent(event, function, context)" },
	{ "StopListeningToGameEvent",	Bind_StopListeningToGameEvent,	"StopListeningToGameEvent(handle)" },
	{ "__CollectGameEventCallbacks", Bind_CollectGameEventCallbacks, "__CollectGameEventCallbacks() - register OnGameEvent_<name> in this scope" },
};
const int g_nScriptGlobalBindings = ARRAYSIZE( g_ScriptGlobalBindings );

const ScriptBinding_t g_ScriptEntityListBindings[] =
{
	{ "FindByName",					Bind_Entities_FindByName,			"Entities.FindByName(prev, name)" },
	{ "FindByClassname",			Bind_Entities_FindByClassname,		"Entities.FindByClassname(prev, classname)" },
	{ "FindByNameWithin",			Bind_Entities_FindByNameWithin,		"Entities.FindByNameWithin(prev, name, origin, radius)" },
	{ "FindByClassnameWithin",		Bind_Entities_FindByClassnameWithin, "Entities.FindByClassnameWithin(prev, classname, origin, radius)" },
	{ "FindInSphere",				Bind_Entities_FindInSphere,			"Entities.FindInSphere(prev, origin, radius)" },
	{ "First",						Bind_Entities_First,				"Entities.First()" },
	{ "Next",						Bind_Entities_Next,					"Entities.Next(entity)" },
	{ "CreateByClassname",			Bind_Entities_CreateByClassname,	"Entities.CreateByClassname(classname)" },
};
const int g_nScriptEntityListBindings = ARRAYSIZE( g_ScriptEntityListBindings );

const ScriptBinding_t g_ScriptConvarBindings[] =
{
	{ "GetStr",						Bind_Convars_GetStr,			"Convars.GetStr(name)" },
	{ "GetFloat",					Bind_Convars_GetFloat,			"Convars.GetFloat(name)" },
	{ "GetInt",						Bind_Convars_GetInt,			"Convars.GetInt(name)" },
	{ "SetValue",					Bind_Convars_SetValue,			"Convars.SetValue(name, value)" },
};
const int g_nScriptConvarBindings = ARRAYSIZE( g_ScriptConvarBindings );

const ScriptBinding_t g_ScriptNetPropBindings[] =
{
	{ "GetPropInt",					Bind_GetPropInt,				"NetProps.GetPropInt(entity, name)" },
	{ "SetPropInt",					Bind_SetPropInt,				"NetProps.SetPropInt(entity, name, value)" },
	{ "GetPropFloat",				Bind_GetPropFloat,				"NetProps.GetPropFloat(entity, name)" },
	{ "SetPropFloat",				Bind_SetPropFloat,				"NetProps.SetPropFloat(entity, name, value)" },
	{ "GetPropBool",				Bind_GetPropBool,				"NetProps.GetPropBool(entity, name)" },
	{ "SetPropBool",				Bind_SetPropBool,				"NetProps.SetPropBool(entity, name, value)" },
	{ "GetPropVector",				Bind_GetPropVector,				"NetProps.GetPropVector(entity, name)" },
	{ "SetPropVector",				Bind_SetPropVector,				"NetProps.SetPropVector(entity, name, value)" },
	{ "GetPropString",				Bind_GetPropString,				"NetProps.GetPropString(entity, name)" },
	{ "SetPropString",				Bind_SetPropString,				"NetProps.SetPropString(entity, name, value)" },
	{ "GetPropEntity",				Bind_GetPropEntity,				"NetProps.GetPropEntity(entity, name)" },
	{ "SetPropEntity",				Bind_SetPropEntity,				"NetProps.SetPropEntity(entity, name, value)" },
};
const int g_nScriptNetPropBindings = ARRAYSIZE( g_ScriptNetPropBindings );

const ScriptBinding_t g_ScriptEntityBindings[] =
{
	{ "IsValid",					Bind_Ent_IsValid,				"IsValid()" },
	{ "GetName",					Bind_Ent_GetName,				"GetName()" },
	{ "SetName",					Bind_Ent_SetName,				"SetName(name)" },
	{ "GetClassname",				Bind_Ent_GetClassname,			"GetClassname()" },
	{ "entindex",					Bind_Ent_entindex,				"entindex()" },
	{ "GetOrigin",					Bind_Ent_GetOrigin,				"GetOrigin()" },
	{ "SetOrigin",					Bind_Ent_SetOrigin,				"SetOrigin(origin)" },
	{ "GetAngles",					Bind_Ent_GetAngles,				"GetAngles() - pitch/yaw/roll as a Vector" },
	{ "SetAngles",					Bind_Ent_SetAngles,				"SetAngles(angles)" },
	{ "GetForwardVector",			Bind_Ent_GetForwardVector,		"GetForwardVector()" },
	{ "GetHealth",					Bind_Ent_GetHealth,				"GetHealth()" },
	{ "SetHealth",					Bind_Ent_SetHealth,				"SetHealth(health)" },
	{ "GetMaxHealth",				Bind_Ent_GetMaxHealth,			"GetMaxHealth()" },
	{ "SetMaxHealth",				Bind_Ent_SetMaxHealth,			"SetMaxHealth(health)" },
	{ "GetVelocity",				Bind_Ent_GetVelocity,			"GetVelocity()" },
	{ "SetVelocity",				Bind_Ent_SetVelocity,			"SetVelocity(velocity)" },
	{ "GetTeamNumber",				Bind_Ent_GetTeamNumber,			"GetTeamNumber()" },
	{ "SetTeam",					Bind_Ent_SetTeam,				"SetTeam(team) - players only" },
	{ "IsAlive",					Bind_Ent_IsAlive,				"IsAlive()" },
	{ "IsPlayer",					Bind_Ent_IsPlayer,				"IsPlayer()" },
	{ "GetModelName",				Bind_Ent_GetModelName,			"GetModelName()" },
	{ "SetModel",					Bind_Ent_SetModel,				"SetModel(model)" },
	{ "Kill",						Bind_Ent_Kill,					"Kill()" },
	{ "TakeDamage",					Bind_Ent_TakeDamage,			"TakeDamage(amount, attacker)" },
	{ "ValidateScriptScope",		Bind_Ent_ValidateScriptScope,	"ValidateScriptScope()" },
	{ "GetScriptScope",				Bind_Ent_GetScriptScope,		"GetScriptScope()" },
	{ "GetEyePosition",				Bind_Ent_GetEyePosition,		"GetEyePosition()" },
	{ "GetPlayerName",				Bind_Ent_GetPlayerName,			"GetPlayerName()" },
};
const int g_nScriptEntityBindings = ARRAYSIZE( g_ScriptEntityBindings );

const ScriptBinding_t g_ScriptVectorBindings[] =
{
	{ "_add",						Bind_Vec_add,					"vector + vector" },
	{ "_sub",						Bind_Vec_sub,					"vector - vector" },
	{ "_mul",						Bind_Vec_mul,					"vector * scalar|vector" },
	{ "_div",						Bind_Vec_div,					"vector / scalar" },
	{ "_unm",						Bind_Vec_unm,					"-vector" },
	{ "Length",						Bind_Vec_Length,				"Length()" },
	{ "_typeof",					Bind_Vec_typeof,				"typeof()" },
};
const int g_nScriptVectorBindings = ARRAYSIZE( g_ScriptVectorBindings );
