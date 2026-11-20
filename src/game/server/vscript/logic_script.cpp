//========= Copyright (c) 2026, improved-srcds-v34 contributors ============//
//
// Purpose: logic_script, the map-placed script entity. Its `vscripts` keyvalue
//          is handled by the shared entity keyvalue hook, so a logic_script
//          behaves exactly like any other entity carrying scripts; the inputs
//          below are what makes it useful from map logic.
//
//=============================================================================//

#include "cbase.h"
#include "vscript.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CLogicScript : public CBaseEntity
{
public:
	DECLARE_CLASS( CLogicScript, CBaseEntity );
	DECLARE_DATADESC();

	void InputRunScriptCode( inputdata_t &inputdata );
	void InputRunScriptFile( inputdata_t &inputdata );
	void InputCallScriptFunction( inputdata_t &inputdata );

private:
	HSCRIPT GetOrCreateScope();

	string_t m_iszGroup;
};

LINK_ENTITY_TO_CLASS( logic_script, CLogicScript );

BEGIN_DATADESC( CLogicScript )
	// Accepted for compatibility with Valve's entity; it only groups scripts
	// in Hammer and has no runtime meaning here.
	DEFINE_KEYFIELD( m_iszGroup, FIELD_STRING, "Group" ),

	DEFINE_INPUTFUNC( FIELD_STRING, "RunScriptCode", InputRunScriptCode ),
	DEFINE_INPUTFUNC( FIELD_STRING, "RunScriptFile", InputRunScriptFile ),
	DEFINE_INPUTFUNC( FIELD_STRING, "CallScriptFunction", InputCallScriptFunction ),
END_DATADESC()

HSCRIPT CLogicScript::GetOrCreateScope()
{
	if ( !VScript_IsActive() )
		return 0;

	HSCRIPT hScope = VScript_GetOwnerScope( this );
	if ( !hScope )
		hScope = VScript_CreateScope( this );

	return hScope;
}

//-----------------------------------------------------------------------------
// Purpose: Run a snippet of Squirrel in this entity's scope.
//-----------------------------------------------------------------------------
void CLogicScript::InputRunScriptCode( inputdata_t &inputdata )
{
	HSCRIPT hScope = GetOrCreateScope();
	if ( !hScope )
		return;

	const char *pszCode = inputdata.value.String();
	VScript_RunCodeInScope( pszCode, "logic_script:RunScriptCode", hScope );
}

//-----------------------------------------------------------------------------
// Purpose: Run a .nut file in this entity's scope.
//-----------------------------------------------------------------------------
void CLogicScript::InputRunScriptFile( inputdata_t &inputdata )
{
	HSCRIPT hScope = GetOrCreateScope();
	if ( !hScope )
		return;

	VScript_RunFileInScope( inputdata.value.String(), hScope, true );
}

//-----------------------------------------------------------------------------
// Purpose: Call a function defined in this entity's scope.
//-----------------------------------------------------------------------------
void CLogicScript::InputCallScriptFunction( inputdata_t &inputdata )
{
	HSCRIPT hScope = GetOrCreateScope();
	if ( !hScope )
		return;

	const char *pszFunc = inputdata.value.String();
	if ( !pszFunc[0] )
		return;

	VScript_CallScopeFunction( hScope, pszFunc, this, 0, NULL, true );
}
