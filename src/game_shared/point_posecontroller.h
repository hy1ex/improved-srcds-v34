//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Controls the pose parameters of a model
//
//===========================================================================//


#define MAX_POSE_CONTROLLED_PROPS 4		// Number of entities by the same name that can be controlled


// Type of frequency modulations
enum PoseController_FModType_t
{
	POSECONTROLLER_FMODTYPE_NONE = 0,
	POSECONTROLLER_FMODTYPE_SINE,
	POSECONTROLLER_FMODTYPE_SQUARE,
	POSECONTROLLER_FMODTYPE_TRIANGLE,
	POSECONTROLLER_FMODTYPE_SAWTOOTH,
	POSECONTROLLER_FMODTYPE_NOISE,

	POSECONTROLLER_FMODTYPE_TOTAL,
};


//-----------------------------------------------------------------------------
// SERVER CLASS
//-----------------------------------------------------------------------------

#include "baseentity.h"


class CPoseController : public CBaseEntity
{
public:
	DECLARE_CLASS( CPoseController, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn( void );

	void Think( void );

	void BuildPropList( void );
	void BuildPoseIndexList( void );
	void SetPoseIndex( int i, int iValue );
	void SetCurrentPose( float fCurrentPoseValue );

	float GetPoseValue( void );

	void SetProp( CBaseAnimating *pProp );
	void SetPropName( const char *pName );
	void SetPoseParameterName( const char *pName );
	void SetPoseValue( float fValue );
	void SetCycleFrequency( float fValue );
	void SetFModType( int nType );
	void SetFModTimeOffset( float fValue );
	void SetFModRate( float fValue );
	void SetFModAmplitude( float fValue );
	void RandomizeFMod( float fExtremeness );

	// Input handlers
	void InputSetPropName( inputdata_t &inputdata );
	void InputSetPoseParameterName( inputdata_t &inputdata );
	void InputSetPoseValue( inputdata_t &inputdata );
	void InputSetCycleFrequency( inputdata_t &inputdata );
	void InputSetFModType( inputdata_t &inputdata );
	void InputSetFModTimeOffset( inputdata_t &inputdata );
	void InputSetFModRate( inputdata_t &inputdata );
	void InputSetFModAmplitude( inputdata_t &inputdata );
	void InputRandomizeFMod( inputdata_t &inputdata );
	void InputGetFMod( inputdata_t &inputdata );

private:

	CNetworkArray( EHANDLE, m_hProps, MAX_POSE_CONTROLLED_PROPS );				// Handles to controlled models
	CNetworkArray( unsigned char, m_chPoseIndex, MAX_POSE_CONTROLLED_PROPS );	// Pose parameter indices for each model

	bool		m_bDisablePropLookup;

	CNetworkVar( bool, m_bPoseValueParity );

	string_t	m_iszPropName;				// Targetname of the models to control
	string_t	m_iszPoseParameterName;		// Pose parameter name to control

	CNetworkVar( float, m_fPoseValue );			// Normalized pose parameter value (maps to each pose parameter's min and max range)
	CNetworkVar( float, m_fCycleFrequency );	// Cycles per second

	// Frequency modulation variables
	CNetworkVar( PoseController_FModType_t, m_nFModType );
	CNetworkVar( float, m_fFModTimeOffset );
	CNetworkVar( float, m_fFModRate );
	CNetworkVar( float, m_fFModAmplitude );
};
