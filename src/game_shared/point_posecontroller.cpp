//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Controls the pose parameters of a model
//
//===========================================================================//


#include "cbase.h"
#include "point_posecontroller.h"


//-----------------------------------------------------------------------------
// SERVER CLASS
//-----------------------------------------------------------------------------

#include "baseanimating.h"
#include "props.h"


#define MAX_POSE_CYCLE_FREQUENCY 10.0f
#define MAX_POSE_FMOD_RATE 10.0f
#define MAX_POSE_FMOD_AMPLITUDE 10.0f


LINK_ENTITY_TO_CLASS( point_posecontroller, CPoseController );	


BEGIN_DATADESC( CPoseController )
	DEFINE_AUTO_ARRAY( m_hProps, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_chPoseIndex, FIELD_CHARACTER ),
	DEFINE_FIELD( m_bDisablePropLookup, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bPoseValueParity, FIELD_BOOLEAN ),
	// Keys
	DEFINE_KEYFIELD( m_iszPropName, FIELD_STRING, "PropName" ),
	DEFINE_KEYFIELD( m_iszPoseParameterName, FIELD_STRING, "PoseParameterName" ),
	DEFINE_KEYFIELD( m_fPoseValue, FIELD_FLOAT, "PoseValue" ),
	DEFINE_KEYFIELD( m_fCycleFrequency, FIELD_FLOAT, "CycleFrequency" ),
	DEFINE_KEYFIELD( m_nFModType, FIELD_INTEGER, "FModType" ),
	DEFINE_KEYFIELD( m_fFModTimeOffset, FIELD_FLOAT, "FModTimeOffset" ),
	DEFINE_KEYFIELD( m_fFModRate, FIELD_FLOAT, "FModRate" ),
	DEFINE_KEYFIELD( m_fFModAmplitude, FIELD_FLOAT, "FModAmplitude" ),
	// Functions
	DEFINE_FUNCTION( Think ),
	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING,	"SetPropName", InputSetPropName ),
	DEFINE_INPUTFUNC( FIELD_STRING,	"SetPoseParameterName", InputSetPoseParameterName ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetPoseValue", InputSetPoseValue ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetCycleFrequency", InputSetCycleFrequency ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetFModType", InputSetFModType ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetFModTimeOffset", InputSetFModTimeOffset ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetFModRate", InputSetFModRate ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetFModAmplitude", InputSetFModAmplitude ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "RandomizeFMod", InputRandomizeFMod ),
	DEFINE_INPUTFUNC( FIELD_VOID, "GetFMod", InputGetFMod ),
END_DATADESC()


IMPLEMENT_SERVERCLASS_ST(CPoseController, DT_PoseController)
	SendPropArray3( SENDINFO_ARRAY3(m_hProps), SendPropEHandle( SENDINFO_ARRAY(m_hProps) ) ),
	SendPropArray3( SENDINFO_ARRAY3(m_chPoseIndex), SendPropInt( SENDINFO_ARRAY(m_chPoseIndex), 5, SPROP_UNSIGNED ) ),	// bits sent must be enough to represent MAXSTUDIOPOSEPARAM
	SendPropBool( SENDINFO(m_bPoseValueParity) ),
	SendPropFloat( SENDINFO(m_fPoseValue), 11, 0, 0.0f, 1.0f ),
	SendPropFloat( SENDINFO(m_fCycleFrequency), 11, 0, -MAX_POSE_CYCLE_FREQUENCY, MAX_POSE_CYCLE_FREQUENCY ),
	SendPropInt( SENDINFO(m_nFModType), 3, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO(m_fFModTimeOffset), 11, 0, -1.0f, 1.0f ),
	SendPropFloat( SENDINFO(m_fFModRate), 11, 0, -MAX_POSE_FMOD_RATE, MAX_POSE_FMOD_RATE ),
	SendPropFloat( SENDINFO(m_fFModAmplitude), 11, 0, 0.0f, MAX_POSE_FMOD_AMPLITUDE ),
END_SEND_TABLE()


void CPoseController::SetPoseIndex( int i, int iValue )
{
	if ( iValue == -1 )
	{
		// Using this as invalid lets us network less bits
		iValue = MAXSTUDIOPOSEPARAM;
	}

	if ( m_chPoseIndex[ i ] != iValue )
	{
		// Only set a new index (to avoid network spam)
		m_chPoseIndex.Set( i, iValue );
	}
}

void CPoseController::BuildPropList( void )
{
	int iPropNum = 0;
	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, m_iszPropName );

	while ( pEnt && iPropNum < MAX_POSE_CONTROLLED_PROPS )
	{
		CBaseAnimating *pProp = dynamic_cast<CBaseAnimating*>( pEnt );
		if ( pProp )
		{
			if ( m_hProps[ iPropNum ] != pProp )
			{
				// Only set new handles (to avoid network spam)
				m_hProps.Set( iPropNum, pProp );	
			}

			// Update the pose parameter index
			SetPoseIndex( iPropNum, pProp->LookupPoseParameter( m_iszPoseParameterName.ToCStr() ) );
			
			++iPropNum;
		}

		// Get the next entity with specified targetname
		pEnt = gEntList.FindEntityByName( pEnt, m_iszPropName );
	}

	// Nullify the remaining handles
	while ( iPropNum < MAX_POSE_CONTROLLED_PROPS )
	{
		if ( m_hProps[ iPropNum ] != NULL )
			m_hProps.Set( iPropNum, NULL );

		++iPropNum;
	}

	m_bDisablePropLookup = false;
	
	SetNextThink( gpGlobals->curtime + 1.0 );
}

void CPoseController::Think( void )
{
	if ( !m_bDisablePropLookup )
	{
		// Refresh the list of models
		BuildPropList();

		SetNextThink( gpGlobals->curtime + 1.0 );
	}
}

void CPoseController::Spawn( void )
{
	BaseClass::Spawn();

	// Talk to the client class when data changes
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	BuildPropList();
	
	// Think to refresh the list of models
	SetThink( &CPoseController::Think );
	SetNextThink( gpGlobals->curtime + 1.0 );
}

void CPoseController::BuildPoseIndexList( void )
{
	for ( int iPropNum = 0; iPropNum < MAX_POSE_CONTROLLED_PROPS; ++iPropNum )
	{
		CBaseAnimating *pProp = dynamic_cast<CBaseAnimating*>( m_hProps[ iPropNum ].Get() );

		if ( pProp )
		{
			// Update the pose parameter index
			SetPoseIndex( iPropNum, pProp->LookupPoseParameter( m_iszPoseParameterName.ToCStr() ) );
		}
	}
}

float CPoseController::GetPoseValue( void )
{
	return m_fPoseValue;
}

void CPoseController::SetProp( CBaseAnimating *pProp )
{
	// Control a prop directly by pointer
	if ( m_hProps[ 0 ] != pProp )
	{
		// Only set new handles (to avoid network spam)
		m_hProps.Set( 0, pProp );
	}

	// Update the pose parameter index
	SetPoseIndex( 0, pProp->LookupPoseParameter( m_iszPoseParameterName.ToCStr() ) );

	// Nullify the remaining handles
	for ( int iPropNum = 1; iPropNum < MAX_POSE_CONTROLLED_PROPS; ++iPropNum )
	{
		if ( m_hProps[ iPropNum ] != NULL )
			m_hProps.Set( iPropNum, NULL );
	}

	m_bDisablePropLookup = true;
}

void CPoseController::SetPropName( const char *pName )
{
	m_iszPropName = MAKE_STRING( pName );

	BuildPropList();
}

void CPoseController::SetPoseParameterName( const char *pName )
{
	m_iszPoseParameterName = MAKE_STRING( pName );

	BuildPoseIndexList();
}

void CPoseController::SetPoseValue( float fValue )
{
	m_fPoseValue = clamp( fValue, 0.0f, 1.0f );

	// Force the client to set the current pose
	m_bPoseValueParity = !m_bPoseValueParity;

	SetCurrentPose( m_fPoseValue );
}

void CPoseController::SetCycleFrequency( float fValue )
{
	m_fCycleFrequency = clamp( fValue, -MAX_POSE_CYCLE_FREQUENCY, MAX_POSE_CYCLE_FREQUENCY );
}

void CPoseController::SetFModType( int nType )
{
	if ( nType < 0 || nType >= POSECONTROLLER_FMODTYPE_TOTAL )
		return;

	m_nFModType = static_cast<PoseController_FModType_t>(nType);
}

void CPoseController::SetFModTimeOffset( float fValue )
{
	m_fFModTimeOffset = clamp( fValue, -1.0f, 1.0f );
}

void CPoseController::SetFModRate( float fValue )
{
	m_fFModRate = clamp( fValue, -MAX_POSE_FMOD_RATE, MAX_POSE_FMOD_RATE );
}

void CPoseController::SetFModAmplitude( float fValue )
{
	m_fFModAmplitude = clamp( fValue, 0.0f, MAX_POSE_FMOD_AMPLITUDE );
}

void CPoseController::RandomizeFMod( float fExtremeness )
{
	fExtremeness = clamp( fExtremeness, 0.0f, 1.0f );

	SetFModType( RandomInt( 1, POSECONTROLLER_FMODTYPE_TOTAL - 1 ) );
	SetFModTimeOffset( RandomFloat( -1.0, 1.0f ) );
	SetFModRate( RandomFloat( fExtremeness * -MAX_POSE_FMOD_RATE, fExtremeness * MAX_POSE_FMOD_RATE ) );
	SetFModAmplitude( RandomFloat( 0.0f, fExtremeness * MAX_POSE_FMOD_AMPLITUDE ) );
}

void CPoseController::InputSetPropName( inputdata_t &inputdata )
{
	SetPropName( inputdata.value.String() );
}

void CPoseController::InputSetPoseParameterName( inputdata_t &inputdata )
{
	SetPoseParameterName( inputdata.value.String() );
}

void CPoseController::InputSetPoseValue( inputdata_t &inputdata )
{
	SetPoseValue( inputdata.value.Float() );
}

void CPoseController::InputSetCycleFrequency( inputdata_t &inputdata )
{
	SetCycleFrequency( inputdata.value.Float() );
}

void CPoseController::InputSetFModType( inputdata_t &inputdata )
{
	SetFModType( inputdata.value.Int() );
}

void CPoseController::InputSetFModTimeOffset( inputdata_t &inputdata )
{
	SetFModTimeOffset( inputdata.value.Float() );
}

void CPoseController::InputSetFModRate( inputdata_t &inputdata )
{
	SetFModRate( inputdata.value.Float() );
}

void CPoseController::InputSetFModAmplitude( inputdata_t &inputdata )
{
	SetFModAmplitude( inputdata.value.Float() );
}

void CPoseController::InputRandomizeFMod( inputdata_t &inputdata )
{
	RandomizeFMod( inputdata.value.Float() );
}

void CPoseController::InputGetFMod( inputdata_t &inputdata )
{
	DevMsg( "FMod values for pose controller %s\nTYPE: %i\nTIME OFFSET: %f\nRATE: %f\nAMPLITUDE: %f\n", 
			GetEntityName(), 
			m_nFModType.Get(), 
			m_fFModTimeOffset.Get(), 
			m_fFModRate.Get(), 
			m_fFModAmplitude.Get() );
}

void CPoseController::SetCurrentPose( float fCurrentPoseValue )
{
	for ( int iPropNum = 0; iPropNum < MAX_POSE_CONTROLLED_PROPS; ++iPropNum )
	{
		// Control each model's pose parameter
		CBaseAnimating *pProp = dynamic_cast<CBaseAnimating*>( m_hProps[ iPropNum ].Get() );

		if ( pProp )
		{
			float fPoseValueMin;
			float fPoseValueMax;

			// Map to the pose parameter's range
			pProp->GetPoseParameterRange( m_chPoseIndex[ iPropNum ], fPoseValueMin, fPoseValueMax );
			pProp->SetPoseParameter( m_chPoseIndex[ iPropNum ], fPoseValueMin + fCurrentPoseValue * ( fPoseValueMax - fPoseValueMin ) );
		}
	}
}
