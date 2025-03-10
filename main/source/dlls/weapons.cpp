//	
//	Copyright (c) 1999, Valve LLC. All rights reserved.
//	
//	This product contains software technology licensed from Id 
//	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
//	All Rights Reserved.
//	
//	Use, distribution, and modification of this source code and/or resulting
//	object code is restricted to non-commercial enhancements to products from
//	Valve LLC.  All other use, distribution, or modification is prohibited
//	without written permission from Valve LLC.
//
//	===== weapons.cpp ========================================================
//
//  functions governing the selection/use of weapons for players
//
// $Workfile: weapons.cpp $
// $Date: 2002/11/22 21:13:52 $
//
//-------------------------------------------------------------------------------
// $Log: weapons.cpp,v $
// Revision 1.49  2002/11/22 21:13:52  Flayra
// - mp_consistency changes
//
// Revision 1.48  2002/11/12 02:19:43  Flayra
// - Fixed problem where friendly bulletfire was doing damage
//
// Revision 1.47  2002/10/24 21:19:11  Flayra
// - Reworked jetpack effects
// - Added a few extra sounds
//
// Revision 1.46  2002/10/16 00:41:15  Flayra
// - Removed unneeds sounds and events
// - Added distress beacon event
// - Added general purpose particle event
//
// Revision 1.45  2002/09/25 20:40:09  Flayra
// - Play different sound for aliens when they get weapons
//
// Revision 1.44  2002/09/23 22:04:00  Flayra
// - Regular update
//
// Revision 1.43  2002/08/31 18:01:18  Flayra
// - Work at VALVe
//
// Revision 1.42  2002/07/26 23:01:05  Flayra
// - Precache numerical feedback event
//
// Revision 1.41  2002/07/23 16:48:37  Flayra
// - Added distress beacon
//
// Revision 1.40  2002/07/08 16:35:17  Flayra
// - Added document header, updates for cheat protection, added constants
//
//===============================================================================
#include "../util/nowarnings.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "../mod/AvHConstants.h"
#include "../mod/AvHMarineEquipmentConstants.h"
#include "../mod/AvHAlienWeaponConstants.h"
#include "../mod/AvHAlienEquipmentConstants.h"
#include "../mod/AvHPlayer.h"
#include "../mod/AvHGamerules.h"
#include "../mod/AvHNetworkMessages.h"

extern CGraph	WorldGraph;
extern int gEvilImpulse101;

int gJetpackEventID;
int gStartOverwatchEventID;
int gEndOverwatchEventID;
int gTeleportEventID;
int gBlinkEffectSuccessEventID;
int gPhaseInEventID;
int gSiegeHitEventID;
int gSiegeViewHitEventID;
int gCommanderPointsAwardedEventID;
int gAlienSightOnEventID;
int gAlienSightOffEventID;
//int gParalysisStartEventID;
int gRegenerationEventID;
int gStartCloakEventID;
int gEndCloakEventID;
int gSporeCloudEventID;
int gUmbraCloudEventID;
int gStopScreamEventID;

int gWelderEventID;
int gWelderConstEventID;
//int gWallJumpEventID;
//int gFlightEventID;
int gEmptySoundEventID;
int gNumericalInfoEventID;
int gInvalidActionEventID;
int gParticleEventID;
int gDistressBeaconEventID;
int gWeaponAnimationEventID;
int gLevelUpEventID;
int gMetabolizeSuccessEventID;

#define NOT_USED 255

DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
DLL_GLOBAL  const char *g_pModelNameLaser = "sprites/laserbeam.spr";
DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for the initial blood
DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for splattered blood

ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS];

MULTIDAMAGE gMultiDamage;

#define TRACER_FREQ		4			// Tracers fire every fourth bullet

extern bool gCanMove[];

//=========================================================
// MaxAmmoCarry - pass in a name and this function will tell
// you the maximum amount of that type of ammunition that a 
// player can carry.
//=========================================================
int MaxAmmoCarry( int iszName )
{
	for ( int i = 0;  i < MAX_WEAPONS; i++ )
	{
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo1 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo1 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo1;
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo2 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo2 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo2;
	}

	ALERT( at_console, "MaxAmmoCarry() doesn't recognize '%s'!\n", STRING( iszName ) );
	return -1;
}

	
/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

//
// ClearMultiDamage - resets the global multi damage accumulator
//
void ClearMultiDamage(void)
{
	gMultiDamage.pEntity = NULL;
	gMultiDamage.amount	= 0;
	gMultiDamage.type = 0;
}

//
// ApplyMultiDamage - inflicts contents of global multi damage register on gMultiDamage.pEntity
//
// GLOBALS USED:
//		gMultiDamage

void ApplyMultiDamage(entvars_t *pevInflictor, entvars_t *pevAttacker )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	TraceResult	tr;
	
	if ( !gMultiDamage.pEntity )
		return;

	float theDamage = gMultiDamage.amount;
	gMultiDamage.pEntity->TakeDamage(pevInflictor, pevAttacker, theDamage, gMultiDamage.type );
}


// GLOBALS USED:
//		gMultiDamage

void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType)
{
	if ( !pEntity )
		return;
	
	gMultiDamage.type |= bitsDamageType;

	if ( pEntity != gMultiDamage.pEntity )
	{
		ApplyMultiDamage(pevInflictor,pevInflictor); // UNDONE: wrong attacker!
		gMultiDamage.pEntity	= pEntity;
		gMultiDamage.amount		= 0;
	}

	gMultiDamage.amount += flDamage;
}

/*
================
SpawnBlood
================
*/
void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage)
{
	if(flDamage >= 0.0f)
	{
		if(bloodColor == DONT_BLEED)
		{
			UTIL_Sparks(vecSpot);
		}
		else
		{
			UTIL_BloodDrips( vecSpot, g_vecAttackDir, bloodColor, (int)flDamage );
		}
	}
}


int DamageDecal( CBaseEntity *pEntity, int bitsDamageType )
{
	if ( !pEntity )
		return (DECAL_GUNSHOT1 + RANDOM_LONG(0,4));
	
	return pEntity->DamageDecal( bitsDamageType );
}

void DecalGunshot( TraceResult *pTrace, int iBulletType )
{
	// Is the entity valid
	if ( !UTIL_IsValidEntity( pTrace->pHit ) )
		return;

	if ( VARS(pTrace->pHit)->solid == SOLID_BSP || VARS(pTrace->pHit)->movetype == MOVETYPE_PUSHSTEP )
	{
		CBaseEntity *pEntity = NULL;
		// Decal the wall with a gunshot
		if ( !FNullEnt(pTrace->pHit) )
			pEntity = CBaseEntity::Instance(pTrace->pHit);

//		switch( iBulletType )
//		{
//		case BULLET_PLAYER_9MM:
//		case BULLET_MONSTER_9MM:
//		case BULLET_PLAYER_MP5:
//		case BULLET_MONSTER_MP5:
//		case BULLET_PLAYER_BUCKSHOT:
//		case BULLET_PLAYER_357:
//		default:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
//			break;
//		case BULLET_MONSTER_12MM:
//			// smoke and decal
//			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
//			break;
//		case BULLET_PLAYER_CROWBAR:
//			// wall decal
//			UTIL_DecalTrace( pTrace, DamageDecal( pEntity, DMG_CLUB ) );
//			break;
//		}
	}
}



//
// EjectBrass - tosses a brass shell from passed origin at passed velocity
//
void EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype )
{
	// FIX: when the player shoots, their gun isn't in the same position as it is on the model other players see.

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_MODEL);
		WRITE_COORD( vecOrigin.x);
		WRITE_COORD( vecOrigin.y);
		WRITE_COORD( vecOrigin.z);
		WRITE_COORD( vecVelocity.x);
		WRITE_COORD( vecVelocity.y);
		WRITE_COORD( vecVelocity.z);
		WRITE_ANGLE( rotation );
		WRITE_SHORT( model );
		WRITE_BYTE ( soundtype);
		WRITE_BYTE ( 25 );// 2.5 seconds
	MESSAGE_END();
}


#if 0
// UNDONE: This is no longer used?
void ExplodeModel( const Vector &vecOrigin, float speed, int model, int count )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE ( TE_EXPLODEMODEL );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( speed );
		WRITE_SHORT( model );
		WRITE_SHORT( count );
		WRITE_BYTE ( 15 );// 1.5 seconds
	MESSAGE_END();
}
#endif


int giAmmoIndex = 0;

// Precaches the ammo and queues the ammo info for sending to clients
void AddAmmoNameToAmmoRegistry( const char *szAmmoname )
{
	// make sure it's not already in the registry
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( !CBasePlayerItem::AmmoInfoArray[i].pszName)
			continue;

		if ( stricmp( CBasePlayerItem::AmmoInfoArray[i].pszName, szAmmoname ) == 0 )
			return; // ammo already in registry, just quite
	}


	giAmmoIndex++;
	ASSERT( giAmmoIndex < MAX_AMMO_SLOTS );
	if ( giAmmoIndex >= MAX_AMMO_SLOTS )
		giAmmoIndex = 0;

	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].pszName = szAmmoname;
	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex;   // yes, this info is redundant
}


// Precaches the weapon and queues the weapon info for sending to clients
void UTIL_PrecacheOtherWeapon( const char *szClassname )
{
	edict_t	*pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOtherWeapon\n" );
		return;
	}
	
	CBaseEntity *pEntity = CBaseEntity::Instance (VARS( pent ));

	if (pEntity)
	{
		ItemInfo II;
		pEntity->Precache( );
		memset( &II, 0, sizeof II );
		if ( ((CBasePlayerItem*)pEntity)->GetItemInfo( &II ) )
		{
			CBasePlayerItem::ItemInfoArray[II.iId] = II;

			if ( II.pszAmmo1 && *II.pszAmmo1 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo1 );
			}

			if ( II.pszAmmo2 && *II.pszAmmo2 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo2 );
			}

			memset( &II, 0, sizeof II );
		}
	}

	REMOVE_ENTITY(pent);
}

// called by worldspawn
void W_Precache(void)
{
	memset( CBasePlayerItem::ItemInfoArray, 0, sizeof(CBasePlayerItem::ItemInfoArray) );
	memset( CBasePlayerItem::AmmoInfoArray, 0, sizeof(CBasePlayerItem::AmmoInfoArray) );
	giAmmoIndex = 0;

	// Marine weapons
	//UTIL_PrecacheOtherWeapon("weapon_9mmhandgun");
    //UTIL_PrecacheOtherWeapon("weapon_glock");
	//UTIL_PrecacheOther("ammo_9mmclip");

	// Player assets
	PRECACHE_UNMODIFIED_MODEL(kReadyRoomModel);
	PRECACHE_UNMODIFIED_MODEL(kMarineSoldierModel);
	PRECACHE_UNMODIFIED_MODEL(kHeavySoldierModel);
	PRECACHE_UNMODIFIED_MODEL(kAlienLevelOneModel);
	PRECACHE_UNMODIFIED_SOUND(kDistressBeaconSound);
	PRECACHE_UNMODIFIED_SOUND(kLevelUpMarineSound);
	PRECACHE_UNMODIFIED_SOUND(kLevelUpAlienSound);
	PRECACHE_UNMODIFIED_SOUND(kAlienBuildingSound1);
	PRECACHE_UNMODIFIED_SOUND(kAlienBuildingSound2);
	PRECACHE_SOUND(kMyHiveEasterEgg);
	
	//PRECACHE_UNMODIFIED_SOUND(kAlienAbilitiesGrantedSound);
	//PRECACHE_UNMODIFIED_SOUND(kAlienAbilitiesLostSound);
	
	PRECACHE_UNMODIFIED_MODEL(kAlienLevelTwoModel);
	PRECACHE_UNMODIFIED_MODEL(kAlienLevelThreeModel);
	PRECACHE_UNMODIFIED_MODEL(kAlienLevelFourModel);
	PRECACHE_UNMODIFIED_MODEL(kAlienLevelFiveModel);

    PRECACHE_UNMODIFIED_MODEL(kMarineCommanderModel);
    PRECACHE_UNMODIFIED_MODEL(kAlienGestateModel);
	// : 1072
	// Added some client side consistency checks.
	PRECACHE_UNMODIFIED_MODEL("sprites/muzzleflash1.spr");
	PRECACHE_UNMODIFIED_MODEL("sprites/muzzleflash2.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/muzzleflash3.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/digesting.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/membrane.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/hera_fog.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/spore.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/spore2.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/umbra.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/umbra2.spr"); 
	PRECACHE_UNMODIFIED_MODEL("sprites/webstrand.spr"); 

	UTIL_PrecacheOtherWeapon(kwsMine);
    UTIL_PrecacheOtherWeapon(kwsKnife);
    UTIL_PrecacheOtherWeapon(kwsMachineGun);
    UTIL_PrecacheOtherWeapon(kwsPistol);
    UTIL_PrecacheOtherWeapon(kwsShotGun);
    UTIL_PrecacheOtherWeapon(kwsHeavyMachineGun);
    UTIL_PrecacheOtherWeapon(kwsGrenadeGun);
    UTIL_PrecacheOtherWeapon(kwsGrenade);

	// Alien weapons
	UTIL_PrecacheOtherWeapon(kwsSpitGun);
	UTIL_PrecacheOther(kwsSpitProjectile);
	UTIL_PrecacheOther(kwsWebProjectile);
	UTIL_PrecacheOtherWeapon(kwsClaws);
	UTIL_PrecacheOtherWeapon(kwsSwipe);
	UTIL_PrecacheOtherWeapon(kwsSporeGun);
	UTIL_PrecacheOther(kwsSporeProjectile);
//	UTIL_PrecacheOtherWeapon(kwsParalysisGun);
	UTIL_PrecacheOtherWeapon(kwsSpikeGun);
	UTIL_PrecacheOtherWeapon(kwsBiteGun);
	UTIL_PrecacheOtherWeapon(kwsBite2Gun);
	UTIL_PrecacheOtherWeapon(kwsHealingSpray);
	UTIL_PrecacheOtherWeapon(kwsWebSpinner);
//	UTIL_PrecacheOtherWeapon(kwsBabblerGun);
	UTIL_PrecacheOtherWeapon(kwsPrimalScream);
	UTIL_PrecacheOtherWeapon(kwsParasiteGun);
	UTIL_PrecacheOtherWeapon(kwsMetabolize);
	UTIL_PrecacheOtherWeapon(kwsUmbraGun);
	UTIL_PrecacheOtherWeapon(kwsBlinkGun);
	UTIL_PrecacheOtherWeapon(kwsDivineWind);
	UTIL_PrecacheOtherWeapon(kwsBileBombGun);
	UTIL_PrecacheOtherWeapon(kwsAcidRocketGun);
	UTIL_PrecacheOtherWeapon(kwsStomp);
	UTIL_PrecacheOtherWeapon(kwsDevour);
//	UTIL_PrecacheOtherWeapon(kwsAmplify);
	UTIL_PrecacheOther(kwsBileBomb);

	// Alien abilities
	UTIL_PrecacheOtherWeapon(kwsLeap);
	UTIL_PrecacheOtherWeapon(kwsCharge);

	// Alien buildings
	UTIL_PrecacheOther(kwsAlienResourceTower);
	UTIL_PrecacheOther(kwsOffenseChamber);
	UTIL_PrecacheOther(kwsDefenseChamber);
	UTIL_PrecacheOther(kwsSensoryChamber);
	UTIL_PrecacheOther(kwsMovementChamber);
	UTIL_PrecacheOther(kesTeamWebStrand);
	
	// Equipment
	//UTIL_PrecacheOtherWeapon("weapon_tripmine");
	UTIL_PrecacheOther(kwsScan);
	UTIL_PrecacheOther(kwsPhaseGate);
	//UTIL_PrecacheOther(kwsNuke);

	// Marine buildings
	UTIL_PrecacheOther(kwsTeamCommand);
	UTIL_PrecacheOther(kwsResourceTower);
	UTIL_PrecacheOther(kwsInfantryPortal);
	UTIL_PrecacheOther(kwsTurretFactory);
	UTIL_PrecacheOther(kwsArmory);
	UTIL_PrecacheOther(kwsAdvancedArmory);
	UTIL_PrecacheOther(kwsArmsLab);
	UTIL_PrecacheOther(kwsPrototypeLab);
	UTIL_PrecacheOther(kwsObservatory);
	//UTIL_PrecacheOther(kwsChemlab);
	//UTIL_PrecacheOther(kwsMedlab);
	//UTIL_PrecacheOther(kwsNukePlant);
	UTIL_PrecacheOther(kwsDeployedTurret);
	UTIL_PrecacheOther(kwsSiegeTurret);

	// container for dropped deathmatch weapons
	UTIL_PrecacheOther("weaponbox");

	UTIL_PrecacheOtherWeapon(kwsWelder);
	UTIL_PrecacheOther(kwsDeployedMine);
	UTIL_PrecacheOther(kwsHealth);
    UTIL_PrecacheOther(kwsCatalyst);
	UTIL_PrecacheOther(kwsGenericAmmo);
	UTIL_PrecacheOther(kwsHeavyArmor);
	UTIL_PrecacheOther(kwsJetpack);
	UTIL_PrecacheOther(kwsAmmoPack);
	
	UTIL_PrecacheOther(kwsDebugEntity);

	// Precache other events
	gJetpackEventID = PRECACHE_EVENT(1, kJetpackEvent);
	//gStartOverwatchEventID = PRECACHE_EVENT(1, kStartOverwatchEvent);
	//gEndOverwatchEventID = PRECACHE_EVENT(1, kEndOverwatchEvent);
	
	// Alien upgrade events
	gRegenerationEventID = PRECACHE_EVENT(1, kRegenerationEvent);
	gStartCloakEventID = PRECACHE_EVENT(1, kStartCloakEvent);
	gEndCloakEventID = PRECACHE_EVENT(1, kEndCloakEvent);

	// Extra alien weapon events
	//gEnsnareHitEventID = PRECACHE_EVENT(1, kEnsnareHitEventName);
	gSporeCloudEventID = PRECACHE_EVENT(1, kSporeCloudEventName);
	gUmbraCloudEventID = PRECACHE_EVENT(1, kUmbraCloudEventName);
	gStopScreamEventID = PRECACHE_EVENT(1, kStopPrimalScreamSoundEvent);
	
	// Extra marine events
	gTeleportEventID = PRECACHE_EVENT(1, kTeleportEvent);
	gBlinkEffectSuccessEventID = PRECACHE_EVENT(1, kBlinkEffectSuccessEventName);
	gPhaseInEventID = PRECACHE_EVENT(1, kPhaseInEvent);
	gSiegeHitEventID = PRECACHE_EVENT(1, kSiegeHitEvent);
	gSiegeViewHitEventID = PRECACHE_EVENT(1, kSiegeViewHitEvent);
	gCommanderPointsAwardedEventID = PRECACHE_EVENT(1, kCommanderPointsAwardedEvent);
	gAlienSightOnEventID = PRECACHE_EVENT(1, kAlienSightOnEvent);
	gAlienSightOffEventID = PRECACHE_EVENT(1, kAlienSightOffEvent);
//	gParalysisStartEventID = PRECACHE_EVENT(1, kParalysisStartEventName);

	//gWallJumpEventID = PRECACHE_EVENT(1, kWallJumpEvent);
	//gFlightEventID = PRECACHE_EVENT(1, kFlightEvent);
	PRECACHE_UNMODIFIED_SOUND(kConnectSound);
	//PRECACHE_UNMODIFIED_SOUND(kDisconnectSound);
	PRECACHE_UNMODIFIED_MODEL(kNullModel);

	UTIL_PrecacheOther("monster_sentry");
	
	// Allow welder events in mapper build
	gWelderEventID = PRECACHE_EVENT(1, kWelderEventName);
	gWelderConstEventID = PRECACHE_EVENT(1, kWelderConstEventName);
	PRECACHE_EVENT(1, kWelderStartEventName);
	PRECACHE_EVENT(1, kWelderEndEventName);
	
	gEmptySoundEventID = PRECACHE_EVENT(1, kEmptySoundEvent);
	gNumericalInfoEventID = PRECACHE_EVENT(1, kNumericalInfoEvent);
	gInvalidActionEventID = PRECACHE_EVENT(1, kInvalidActionEvent);
	gParticleEventID = PRECACHE_EVENT(1, kParticleEvent);
	gDistressBeaconEventID = PRECACHE_EVENT(1, kDistressBeaconEvent);
	gWeaponAnimationEventID= PRECACHE_EVENT(1, kWeaponAnimationEvent);
	gLevelUpEventID = PRECACHE_EVENT(1, kLevelUpEvent);
	gMetabolizeSuccessEventID = PRECACHE_EVENT(1, kMetabolizeSuccessEventName);
	
	PRECACHE_UNMODIFIED_SOUND(kPhaseInSound);
	
	// Precache reload sound that is hardcoded deep in HL
	PRECACHE_UNMODIFIED_SOUND(kEmptySound);
	PRECACHE_UNMODIFIED_SOUND(kInvalidSound);

	// Not sure who's using these, but I keep getting errors that they're not precached.  Buttons?
	PRECACHE_UNMODIFIED_SOUND("buttons/spark1.wav");
	PRECACHE_UNMODIFIED_SOUND("buttons/spark2.wav");
	PRECACHE_UNMODIFIED_SOUND("buttons/spark3.wav");
	PRECACHE_UNMODIFIED_SOUND("buttons/spark4.wav");
	PRECACHE_UNMODIFIED_SOUND("buttons/spark5.wav");
	PRECACHE_UNMODIFIED_SOUND("buttons/spark6.wav");

	// For grunts.  Careful, this uses the same weapon id that the grenade gun uses
	//UTIL_PrecacheOtherWeapon("weapon_9mmAR");

	// common world objects
//	UTIL_PrecacheOther( "item_suit" );
//	UTIL_PrecacheOther( "item_battery" );
//	UTIL_PrecacheOther( "item_antidote" );
//	UTIL_PrecacheOther( "item_security" );
//	UTIL_PrecacheOther( "item_longjump" );

	// shotgun
//	UTIL_PrecacheOtherWeapon( "weapon_shotgun" );
//	UTIL_PrecacheOther( "ammo_buckshot" );
//
//	// crowbar
//	UTIL_PrecacheOtherWeapon( "weapon_rowbar" );
//
//	// glock
//	UTIL_PrecacheOtherWeapon( "weapon_9mmhandgun" );
//	UTIL_PrecacheOther( "ammo_9mmclip" );
//
//	// mp5
//	UTIL_PrecacheOtherWeapon( "weapon_9mmAR" );
//	UTIL_PrecacheOther( "ammo_9mmAR" );
//	UTIL_PrecacheOther( "ammo_ARgrenades" );

//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// python
//	UTIL_PrecacheOtherWeapon( "weapon_357" );
//	UTIL_PrecacheOther( "ammo_357" );
//#endif
//	
//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// gauss
//	UTIL_PrecacheOtherWeapon( "weapon_gauss" );
//	UTIL_PrecacheOther( "ammo_gaussclip" );
//#endif
//
//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// rpg
//	UTIL_PrecacheOtherWeapon( "weapon_rpg" );
//	UTIL_PrecacheOther( "ammo_rpgclip" );
//#endif
//
//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// crossbow
//	UTIL_PrecacheOtherWeapon( "weapon_crossbow" );
//	UTIL_PrecacheOther( "ammo_crossbow" );
//	UTIL_PrecacheOther( "weaponbox" );// container for dropped deathmatch weapons
//#endif
//
//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// egon
//	UTIL_PrecacheOtherWeapon( "weapon_egon" );
//#endif
//
//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// satchel charge
//	UTIL_PrecacheOtherWeapon( "weapon_satchel" );
//#endif
//
//	// hand grenade
//	UTIL_PrecacheOtherWeapon("weapon_handgrenade");
//
//
//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// squeak grenade
//	UTIL_PrecacheOtherWeapon( "weapon_snark" );
//#endif
//
//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	// hornetgun
//	UTIL_PrecacheOtherWeapon( "weapon_hornetgun" );
//#endif


//#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
//	if ( g_pGameRules->IsDeathmatch() )
//	{
//		UTIL_PrecacheOther( "weaponbox" );// container for dropped deathmatch weapons
//	}
//#endif

	g_sModelIndexFireball = PRECACHE_UNMODIFIED_MODEL ("sprites/zerogxplode.spr");// fireball
	g_sModelIndexWExplosion = PRECACHE_UNMODIFIED_MODEL ("sprites/WXplo1.spr");// underwater fireball
	g_sModelIndexSmoke = PRECACHE_UNMODIFIED_MODEL ("sprites/steam1.spr");// smoke
	g_sModelIndexBubbles = PRECACHE_UNMODIFIED_MODEL ("sprites/bubble2.spr");//bubbles
	g_sModelIndexBloodSpray = PRECACHE_UNMODIFIED_MODEL ("sprites/bloodspray.spr"); // initial blood
	g_sModelIndexBloodDrop = PRECACHE_UNMODIFIED_MODEL ("sprites/blood.spr"); // splattered blood 

	g_sModelIndexLaser = PRECACHE_UNMODIFIED_MODEL( (char *)g_pModelNameLaser );
	g_sModelIndexLaserDot = PRECACHE_UNMODIFIED_MODEL("sprites/laserdot.spr");


	// used by explosions
	PRECACHE_UNMODIFIED_MODEL ("models/grenade.mdl");
	PRECACHE_UNMODIFIED_MODEL ("sprites/explode1.spr");

	PRECACHE_SOUND ("weapons/debris1.wav");// explosion aftermaths
	PRECACHE_SOUND ("weapons/debris2.wav");// explosion aftermaths
	PRECACHE_SOUND ("weapons/debris3.wav");// explosion aftermaths

	PRECACHE_UNMODIFIED_SOUND (kGrenadeBounceSound1);
	PRECACHE_UNMODIFIED_SOUND (kGrenadeBounceSound2);
	PRECACHE_UNMODIFIED_SOUND (kGrenadeBounceSound3);
    PRECACHE_UNMODIFIED_SOUND (kGRHitSound);

	PRECACHE_UNMODIFIED_SOUND ("weapons/bullet_hit1.wav");	// hit by bullet
	PRECACHE_UNMODIFIED_SOUND ("weapons/bullet_hit2.wav");	// hit by bullet
	
	PRECACHE_UNMODIFIED_SOUND ("items/weapondrop1.wav");// weapon falls to the ground

}


 

TYPEDESCRIPTION	CBasePlayerItem::m_SaveData[] = 
{
	DEFINE_FIELD( CBasePlayerItem, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerItem, m_pNext, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerItem, m_iId, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CBasePlayerItem, CBaseAnimating );


TYPEDESCRIPTION	CBasePlayerWeapon::m_SaveData[] = 
{
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iPrimaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSecondaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iClip, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBasePlayerWeapon, CBasePlayerItem );


void CBasePlayerItem :: SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-24, -24, 0);
	pev->absmax = pev->origin + Vector(24, 24, 16); 
}

BOOL
CBasePlayerItem::CanDeploy( void ) 
{ 
	return TRUE; 
}

// can this weapon be put away right now?
BOOL CBasePlayerItem::CanHolster( void ) 
{ 
	return TRUE; 
};

// returns is deploy was successful
BOOL CBasePlayerItem::Deploy( )								
{ 
	return TRUE; 
}

BOOL
CBasePlayerItem::IsUseable( void ) 
{ 
	return TRUE; 
}


//=========================================================
// Sets up movetype, size, solidtype for a new weapon. 
//=========================================================
void CBasePlayerItem :: FallInit( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0) );//pointsize until it lands on the ground.
	
	SetTouch(&CBasePlayerItem::DefaultTouch );
	SetThink(&CBasePlayerItem::FallThink );

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// FallThink - Items that have just spawned run this think
// to catch them when they hit the ground. Once we're sure
// that the object is grounded, we change its solid type
// to trigger and set it in a large box that helps the
// player get it.
//=========================================================
void CBasePlayerItem::FallThink ( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner ) )
		{
			int pitch = 95 + RANDOM_LONG(0,29);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "items/weapondrop1.wav", 1, ATTN_NORM, 0, pitch);
		}

		// lie flat
		pev->angles.x = 0;
		pev->angles.z = 0;

		Materialize();
	}
	//Weapons in air for too long from collision issues with other entities. Change to SOLID_TRIGGER to fall to ground.
	else if (fabs(pev->velocity.x) < 0.1f && fabs(pev->velocity.y) < 0.1f && fabs(pev->velocity.z) < 3.0f)
	{
		pev->solid = SOLID_TRIGGER;
		//ALERT(at_console, "stuck x=%f y=%f z=%f \n", pev->velocity.x, pev->velocity.y, pev->velocity.z);
	}
}

//=========================================================
// Materialize - make a CBasePlayerItem visible and tangible
//=========================================================
void CBasePlayerItem::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	pev->solid = SOLID_TRIGGER;

	UTIL_SetOrigin( pev, pev->origin );// link into world.
	SetTouch (&CBasePlayerItem::DefaultTouch);
	SetThink (NULL);

	this->VirtualMaterialize();
}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CBasePlayerItem::AttemptToMaterialize( void )
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	pev->nextthink = gpGlobals->time + time;
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should 
// it respawn?
//=========================================================
void CBasePlayerItem :: CheckRespawn ( void )
{
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	case GR_WEAPON_RESPAWN_NO:
		return;
		break;
	}
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerItem::Respawn( void )
{
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( (char *)STRING( pev->classname ), g_pGameRules->VecWeaponRespawnSpot( this ), pev->angles, pev->owner );

	if ( pNewWeapon )
	{
		pNewWeapon->pev->effects |= EF_NODRAW;// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink(&CBasePlayerItem::AttemptToMaterialize );

		DROP_TO_FLOOR ( ENT(pev) );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->pev->nextthink = g_pGameRules->FlWeaponRespawnTime( this );
	}
	else
	{
		ALERT ( at_console, "Respawn failed to create %s!\n", STRING( pev->classname ) );
	}

	return pNewWeapon;
}

void CBasePlayerItem::DefaultTouch( CBaseEntity *pOther )
{
	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// can I have this?
	if ( !g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( this );
		}
		return;
	}

	if (pOther->AddPlayerItem( this ))
	{
		AttachToPlayer( pPlayer );

		AvHPlayer* thePlayer = dynamic_cast<AvHPlayer*>(pPlayer);
		if(thePlayer && thePlayer->GetIsAlien())
		{
			EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2-a.wav", 1, ATTN_NORM);
		}
		else
		{
			EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM);
		}
	}

	SUB_UseTargets( pOther, USE_TOGGLE, 0 ); // UNDONE: when should this happen?

	//// 2021 - Possible fix if crashes occur. Disabled because weapons can't be picked up if aliens touch them while they're falling.
	//// https://github.com/ValveSoftware/halflife/pull/1599
	//// If the item is falling and its Think remains FallItem after the player picks it up,
	//// then after the item touches the ground its Touch will be set back to DefaultTouch,
	//// so the player will pick it up again, this time Kill-ing the item (since we already have it in the inventory),
	//// which will make the pointer bad and crash the game.
	//if (m_pfnThink == &CBasePlayerItem::FallThink)
		//SetThink(NULL);
}

BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted )
{
	if ( !isPredicted )
	{
		return ( attack_time <= curtime ) ? TRUE : FALSE;
	}
	else
	{
		return ( attack_time <= 0.0 ) ? TRUE : FALSE;
	}
}


void CBasePlayerWeapon::ItemPostFrame( void )
{
	// Block attacks during +movement except for lerk.
	bool theAttackPressed = (m_pPlayer->pev->button & IN_ATTACK) && (!(m_pPlayer->pev->button & IN_ATTACK2) || m_pPlayer->pev->iuser3 == AVH_USER3_ALIEN_PLAYER3);
	bool pistolAttackUp = (m_pPlayer->m_iPistolTrigger && m_pPlayer->m_afButtonLast & IN_ATTACK && m_pPlayer->m_afButtonReleased & IN_ATTACK && m_iId == AVH_WEAPON_PISTOL);

    bool theWeaponPrimes = (this->GetWeaponPrimeTime() > 0.0f);
    bool theWeaponIsPriming = this->GetIsWeaponPriming();
    bool theWeaponIsPrimed = this->GetIsWeaponPrimed();

	if ((m_fInReload) && ( m_pPlayer->m_flNextAttack <= UTIL_WeaponTimeBase() ) )
	{
		// complete the reload. 
		int j = min( iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

		// Add them to the clip
		m_iClip += j;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;
	}

/*	// +movement: Removed case for +attack2 since it's used for movement abilities
	if ((m_pPlayer->pev->button & IN_ATTACK2) && CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ) )
	{
        if (m_pPlayer->GetCanUseWeapon())
        {
        
		    if ( pszAmmo2() && !m_pPlayer->m_rgAmmo[SecondaryAmmoIndex()] )
		    {
			    m_fFireOnEmpty = TRUE;
		    }

		    m_pPlayer->TabulateAmmo();
		    SecondaryAttack();
		    m_pPlayer->pev->button &= ~IN_ATTACK2;
        }
	}
    else 
*/

	if ( (theAttackPressed || m_bAttackQueued || pistolAttackUp) && m_pPlayer->GetCanUseWeapon())
	{
		if ((m_fInSpecialReload == 1 || m_fInSpecialReload == 2) && m_iClip != 0)
		{
			m_fInSpecialReload = 3;
			Reload();
		}
		else if (CanAttack(m_flNextPrimaryAttack, gpGlobals->time, UseDecrement()))
        {
            if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && !m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] ) )
		    {
			    m_fFireOnEmpty = TRUE;
		    }

			m_pPlayer->TabulateAmmo();
			PrimaryAttack(pistolAttackUp);
        }
		else
		{
			QueueAttack(pistolAttackUp);
		}
	}
	else if (m_pPlayer->pev->button & IN_ATTACK2)
	{
		if (m_pPlayer->pev->iuser3 == AVH_USER3_ALIEN_PLAYER2 && m_pPlayer->GetCanUseWeapon())
		{
			if (CanAttack(m_flNextPrimaryAttack, gpGlobals->time, UseDecrement()))
			{
				SecondaryAttack();
			}
		}
	}
	else if ( m_pPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload ) 
	{
        if (m_pPlayer->GetCanUseWeapon())
        {
            // reload when reload is pressed, or if no buttons are down and weapon is empty.
	        Reload();
			return;
        }
	}
	// +movement: Removed case for +attack2
	else if ( !(m_pPlayer->pev->button & (IN_ATTACK /* |IN_ATTACK2 */) ) )
	{
        if (m_pPlayer->GetCanUseWeapon())
        {

		    // no fire buttons down

		    m_fFireOnEmpty = FALSE;

		    if ( !IsUseable() && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) ) 
		    {
			    // weapon isn't useable, switch.
			    if ( !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && g_pGameRules->GetNextBestWeapon( m_pPlayer, this ) )
			    {
				    m_flNextPrimaryAttack = ( UseDecrement() ? 0.0 : gpGlobals->time ) + 0.3;
				    return;
			    }
		    }
		    else
		    {
			    // weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			    if ( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) )
			    {
                    if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
			        {
    				    Reload();
	    			    return;
		    	    }
                }
		    }

		    WeaponIdle( );
        }
		return;
	}
	
	// catch all
	if ( ShouldWeaponIdle() )
	{
		WeaponIdle();
	}
}

void CBasePlayerItem::DestroyItem( void )
{
	//KGP: this is a virtual function call for a reason...
	// it had been replaced with the contents of
	// CBasePlayerItem::VirtualDestroyItem, ignoring
	// AvHBasePlayerWeapon::VirtualDestroyItem in 3.01
	this->VirtualDestroyItem();
}

void CBasePlayerItem::VirtualDestroyItem( void )
{
	if ( m_pPlayer )
	{
		// if attached to a player, remove. 
		m_pPlayer->RemovePlayerItem( this );
	}
	
	Kill( );
}

int CBasePlayerItem::AddToPlayer( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer;
	pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
	return TRUE;
}

void CBasePlayerItem::Drop( void )
{
	SetTouch( NULL );
	SetThink(&CBasePlayerItem::SUB_Remove);
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Kill( void )
{
	SetTouch( NULL );
	SetThink(&CBasePlayerItem::SUB_Remove);
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Holster( int skiplocal /* = 0 */ )
{ 
	m_pPlayer->pev->viewmodel = 0; 
	m_pPlayer->pev->weaponmodel = 0;
}

void CBasePlayerItem::AttachToPlayer ( CBasePlayer *pPlayer )
{
	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->aiment = pPlayer->edict();
	pev->effects = EF_NODRAW; // ??
	pev->modelindex = 0;// server won't send down to clients if modelindex == 0
	pev->model = iStringNull;
	pev->owner = pPlayer->edict();
	pev->nextthink = gpGlobals->time + .1;
	SetTouch( NULL );
}

void CBasePlayerItem::Spawn()
{
    CBaseAnimating::Spawn();
}

// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem *pOriginal )
{
	if ( m_iDefaultAmmo )
	{
		return ExtractAmmo( (CBasePlayerWeapon *)pOriginal );
	}
	else
	{
		// a dead player dropped this.
		return ExtractClipAmmo( (CBasePlayerWeapon *)pOriginal );
	}
}


int CBasePlayerWeapon::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );

	pPlayer->pev->weapons |= (1<<m_iId);

	if ( !m_iPrimaryAmmoType )
	{
		m_iPrimaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo1() );
		m_iSecondaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo2() );
	}


	if (bResult)
		return AddWeapon( );
	return FALSE;
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer *pPlayer )
{
	BOOL bSend = FALSE;
	int state = 0;

	// KGP: folded m_iEnabled into the state value and converted it to a proper bitfield.
	if( pPlayer->m_pActiveItem == this )
	{ state |= WEAPON_IS_CURRENT; }
	if( pPlayer->m_fOnTarget )
	{ state |= WEAPON_ON_TARGET; }
	if( m_iEnabled )
	{ state |= WEAPON_IS_ENABLED; }
	
	// Forcing send of all data!
	if ( !pPlayer->m_fWeapon )
	{
		bSend = TRUE; 
	}
	
	// This is the current or last weapon, so the state will need to be updated
	if ( this == pPlayer->m_pActiveItem ||
		 this == pPlayer->m_pClientActiveItem )
	{
		if ( pPlayer->m_pActiveItem != pPlayer->m_pClientActiveItem )
		{
			bSend = TRUE; 
		}
	}

	// If the ammo, state, or fov has changed, update the weapon
	if ( m_iClip != m_iClientClip || 
		 state != m_iClientWeaponState || 
		 pPlayer->m_iFOV != pPlayer->m_iClientFOV )
	{
		bSend = TRUE;
	}

	if (m_iId == 22 || m_iId == 11 || m_iId == 21)
		gCanMove[pPlayer->entindex() - 1] = m_iEnabled;

	if ( bSend )
	{
		NetMsg_CurWeapon( pPlayer->pev, state, m_iId, m_iClip );
	    m_iClientClip = m_iClip;
	    m_iClientWeaponState = state;
	    pPlayer->m_fWeapon = TRUE;
	}

	if ( m_pNext )
		m_pNext->UpdateClientData( pPlayer );

	return 1;
}

void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
	if(iAnim >= 0)
	{
		if ( UseDecrement() )
			skiplocal = 1;
		else
			skiplocal = 0;
	
		m_pPlayer->pev->weaponanim = iAnim;
	
		if ( skiplocal && ENGINE_CANSKIP( m_pPlayer->edict() ) )
			return;
	
		MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, m_pPlayer->pev );
			WRITE_BYTE( iAnim );						// sequence number
			WRITE_BYTE( pev->body );					// weaponmodel bodygroup.
		MESSAGE_END();
	}
}

BOOL CBasePlayerWeapon :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry )
{
	int iIdAmmo;

	if (iMaxClip < 1)
	{
		m_iClip = -1;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	else if (m_iClip == 0)
	{
		int i;
		i = min( m_iClip + iCount, iMaxClip ) - m_iClip;
		m_iClip += i;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount - i, szName, iMaxCarry );
	}
	else
	{
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	
	// m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = iMaxCarry; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iPrimaryAmmoType = iIdAmmo;
		if (m_pPlayer->HasPlayerItem( this ) )
		{
			// play the "got ammo" sound only if we gave some ammo to a player that already had this gun.
			// if the player is just getting this gun for the first time, DefaultTouch will play the "picked up gun" sound for us.
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
	}

	return iIdAmmo > 0 ? TRUE : FALSE;
}


BOOL CBasePlayerWeapon :: AddSecondaryAmmo( int iCount, char *szName, int iMax )
{
	int iIdAmmo;

	iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMax );

	//m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] = iMax; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iSecondaryAmmoType = iIdAmmo;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
	}
	return iIdAmmo > 0 ? TRUE : FALSE;
}

//=========================================================
// IsUseable - this function determines whether or not a 
// weapon is useable by the player in its current state. 
// (does it have ammo loaded? do I have any ammo for the 
// weapon?, etc)
//=========================================================
BOOL CBasePlayerWeapon :: IsUseable( void )
{
	if ( m_iClip <= 0 )
	{
		// This looks like a nasty bug Valve didn't notice
		ASSERT(PrimaryAmmoIndex() >= 0);

		if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] <= 0 && iMaxAmmo1() != -1 )			
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type. 
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CBasePlayerWeapon :: CanDeploy( void )
{
	BOOL bHasAmmo = 0;

	// All weapons can always deploy.  Once fire is hit, it checks if it's out of ammo.  If so, the weapon switches.
	// This is needed so ammo packs function correctly, and it also feels more responsive.

//	if ( !pszAmmo1() )
//	{
//		// this weapon doesn't use ammo, can always deploy.
//		return TRUE;
//	}
//
//	if ( pszAmmo1() )
//	{
//		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
//	}
//	if ( pszAmmo2() )
//	{
//		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
//	}
//	if (m_iClip > 0)
//	{
//		bHasAmmo |= 1;
//	}
//	if (!bHasAmmo)
//	{
//		return FALSE;
//	}

	return TRUE;
}

BOOL CBasePlayerWeapon :: DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal, int body)
{
	if (!CanDeploy( ))
		return FALSE;

	m_pPlayer->TabulateAmmo();
	m_pPlayer->pev->viewmodel = MAKE_STRING(szViewModel);
	m_pPlayer->pev->weaponmodel = MAKE_STRING(szWeaponModel);
	strcpy( m_pPlayer->m_szAnimExtention, szAnimExt );
	SendWeaponAnim( iAnim, skiplocal, body );

	// Set the player animation as well
	//this->m_pPlayer->SetAnimation(PLAYER_ANIM(iAnim));

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + this->GetDeployTime();
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + this->GetDeployTime() + kDeployIdleInterval;

	return TRUE;
}


BOOL CBasePlayerWeapon :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body)
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return FALSE;

	int j = min(iClipSize - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

	if (j == 0)
		return FALSE;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + fDelay;

	//!!UNDONE -- reload sound goes here !!!
	//SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );

	m_fInReload = TRUE;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + kDeployIdleInterval;

	return TRUE;
}

BOOL CBasePlayerWeapon :: PlayEmptySound( void )
{
	if (m_iPlayEmptySound)
	{
//		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);

		int flags = FEV_NOTHOST;
		
		PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), gEmptySoundEventID, 0.0, (float *)&(m_pPlayer->pev->origin), (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

		m_iPlayEmptySound = 0;
		return 0;
	}
	return 0;
}

void CBasePlayerWeapon :: ResetEmptySound( void )
{
	m_iPlayEmptySound = 1;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::PrimaryAmmoIndex( void )
{
	return m_iPrimaryAmmoType;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::SecondaryAmmoIndex( void )
{
	return -1;
}

void CBasePlayerWeapon::Holster( int skiplocal /* = 0 */ )
{ 
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pPlayer->pev->viewmodel = 0; 
	m_pPlayer->pev->weaponmodel = 0;
}

void CBasePlayerAmmo::Spawn( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch(&CBasePlayerAmmo::DefaultTouch );
}

CBaseEntity* CBasePlayerAmmo::Respawn( void )
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );

	UTIL_SetOrigin( pev, g_pGameRules->VecAmmoRespawnSpot( this ) );// move to wherever I'm supposed to repawn.

	SetThink(&CBasePlayerAmmo::Materialize );
	pev->nextthink = g_pGameRules->FlAmmoRespawnTime( this );

	return this;
}

void CBasePlayerAmmo::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	SetTouch(&CBasePlayerAmmo::DefaultTouch );
}

void CBasePlayerAmmo :: DefaultTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	if (AddAmmo( pOther ))
	{
		if ( g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
		{
			Respawn();
		}
		else
		{
			SetTouch( NULL );
			SetThink(&CBasePlayerAmmo::SUB_Remove);
			pev->nextthink = gpGlobals->time + .1;
		}
	}
	else if (gEvilImpulse101)
	{
		// evil impulse 101 hack, kill always
		SetTouch( NULL );
		SetThink(&CBasePlayerAmmo::SUB_Remove);
		pev->nextthink = gpGlobals->time + .1;
	}
}

//=========================================================
// called by the new item with the existing item as parameter
//
// if we call ExtractAmmo(), it's because the player is picking up this type of weapon for 
// the first time. If it is spawned by the world, m_iDefaultAmmo will have a default ammo amount in it.
// if  this is a weapon dropped by a dying player, has 0 m_iDefaultAmmo, which means only the ammo in 
// the weapon clip comes along. 
//=========================================================
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iReturn = 0;

	if ( pszAmmo1() != NULL )
	{
		// blindly call with m_iDefaultAmmo. It's either going to be a value or zero. If it is zero,
		// we only get the ammo in the weapon's clip, which is what we want. 
		iReturn = pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char *)pszAmmo1(), iMaxClip(), iMaxAmmo1() );
		m_iDefaultAmmo = 0;
	}

	if ( pszAmmo2() != NULL )
	{
		iReturn = pWeapon->AddSecondaryAmmo( 0, (char *)pszAmmo2(), iMaxAmmo2() );
	}

	return iReturn;
}

//=========================================================
// called by the new item's class with the existing item as parameter
//=========================================================
int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iAmmo = 0;

	if ( m_iClip == WEAPON_NOCLIP )
	{
		iAmmo = 0;// guns with no clips always come empty if they are second-hand
	}
	else
	{
		iAmmo = m_iClip;
	}
	
	return pWeapon->m_pPlayer->GiveAmmo( iAmmo, (char *)pszAmmo1(), iMaxAmmo1() ); // , &m_iPrimaryAmmoType
}
	
//=========================================================
// RetireWeapon - no more ammo for this gun, put it away.
//=========================================================
void CBasePlayerWeapon::RetireWeapon( void )
{
	// first, no viewmodel at all.
	m_pPlayer->pev->viewmodel = iStringNull;
	m_pPlayer->pev->weaponmodel = iStringNull;
	//m_pPlayer->pev->viewmodelindex = NULL;

	g_pGameRules->GetNextBestWeapon( m_pPlayer, this );
}

//*********************************************************
// weaponbox code:
//*********************************************************

LINK_ENTITY_TO_CLASS( weaponbox, CWeaponBox );

TYPEDESCRIPTION	CWeaponBox::m_SaveData[] = 
{
	DEFINE_ARRAY( CWeaponBox, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CWeaponBox, m_cAmmoTypes, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CWeaponBox, CBaseEntity );

//=========================================================
//
//=========================================================
void CWeaponBox::Precache( void )
{
	PRECACHE_UNMODIFIED_MODEL("models/w_weaponbox.mdl");
}

//=========================================================
//=========================================================
void CWeaponBox :: KeyValue( KeyValueData *pkvd )
{
	if ( m_cAmmoTypes < MAX_AMMO_SLOTS )
	{
		PackAmmo( ALLOC_STRING(pkvd->szKeyName), atoi(pkvd->szValue) );
		m_cAmmoTypes++;// count this new ammo type.

		pkvd->fHandled = TRUE;
	}
	else
	{
		ALERT ( at_console, "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
	}
}

//=========================================================
// CWeaponBox - Spawn 
//=========================================================
void CWeaponBox::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SET_MODEL( ENT(pev), "models/w_weaponbox.mdl");
}

//=========================================================
// CWeaponBox - Kill - the think function that removes the
// box from the world.
//=========================================================
void CWeaponBox::Kill( void )
{
	CBasePlayerItem *pWeapon;
	int i;

	// destroy the weapons
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			pWeapon->SetThink(&CWeaponBox::SUB_Remove);
			pWeapon->pev->nextthink = gpGlobals->time + 0.1;
			pWeapon = pWeapon->m_pNext;
		}
	}

	// remove the box
	UTIL_Remove( this );
}

//=========================================================
// CWeaponBox - Touch: try to add my contents to the toucher
// if the toucher is a player.
//=========================================================
void CWeaponBox::Touch( CBaseEntity *pOther )
{
	if ( !(pev->flags & FL_ONGROUND ) )
	{
		return;
	}

	if ( !pOther->IsPlayer() )
	{
		// only players may touch a weaponbox.
		return;
	}

	if ( !pOther->IsAlive() )
	{
		// no dead guys.
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	int i;

// dole out ammo
	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// there's some ammo of this type. 
			pPlayer->GiveAmmo( m_rgAmmo[ i ], (char *)STRING( m_rgiszAmmo[ i ] ), MaxAmmoCarry( m_rgiszAmmo[ i ] ) );

			//ALERT ( at_console, "Gave %d rounds of %s\n", m_rgAmmo[i], STRING(m_rgiszAmmo[i]) );

			// now empty the ammo from the weaponbox since we just gave it to the player
			m_rgiszAmmo[ i ] = iStringNull;
			m_rgAmmo[ i ] = 0;
		}
	}

// go through my weapons and try to give the usable ones to the player. 
// it's important the the player be given ammo first, so the weapons code doesn't refuse 
// to deploy a better weapon that the player may pick up because he has no ammo for it.
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			CBasePlayerItem *pItem;

			// have at least one weapon in this slot
			while ( m_rgpPlayerItems[ i ] )
			{
				//ALERT ( at_console, "trying to give %s\n", STRING( m_rgpPlayerItems[ i ]->pev->classname ) );

				pItem = m_rgpPlayerItems[ i ];
				m_rgpPlayerItems[ i ] = m_rgpPlayerItems[ i ]->m_pNext;// unlink this weapon from the box

				if ( pPlayer->AddPlayerItem( pItem ) )
				{
					pItem->AttachToPlayer( pPlayer );
				}
			}
		}
	}

	EMIT_SOUND( pOther->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	SetTouch(NULL);
	UTIL_Remove(this);
}

//=========================================================
// CWeaponBox - PackWeapon: Add this weapon to the box
//=========================================================
BOOL CWeaponBox::PackWeapon( CBasePlayerItem *pWeapon )
{
	// is one of these weapons already packed in this box?
	if ( HasWeapon( pWeapon ) )
	{
		return FALSE;// box can only hold one of each weapon type
	}

	if ( pWeapon->m_pPlayer )
	{
		if ( !pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) )
		{
			// failed to unhook the weapon from the player!
			return FALSE;
		}
	}

	int iWeaponSlot = pWeapon->iItemSlot();
	
	if ( m_rgpPlayerItems[ iWeaponSlot ] )
	{
		// there's already one weapon in this slot, so link this into the slot's column
		pWeapon->m_pNext = m_rgpPlayerItems[ iWeaponSlot ];	
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
	}
	else
	{
		// first weapon we have for this slot
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
		pWeapon->m_pNext = NULL;	
	}

	pWeapon->pev->spawnflags |= SF_NORESPAWN;// never respawn
	pWeapon->pev->movetype = MOVETYPE_NONE;
	pWeapon->pev->solid = SOLID_NOT;
	pWeapon->pev->effects = EF_NODRAW;
	pWeapon->pev->modelindex = 0;
	pWeapon->pev->model = iStringNull;
	pWeapon->pev->owner = edict();
	pWeapon->SetThink( NULL );// crowbar may be trying to swing again, etc.
	pWeapon->SetTouch( NULL );
	pWeapon->m_pPlayer = NULL;

	//ALERT ( at_console, "packed %s\n", STRING(pWeapon->pev->classname) );

	return TRUE;
}

//=========================================================
// CWeaponBox - PackAmmo
//=========================================================
BOOL CWeaponBox::PackAmmo( int iszName, int iCount )
{
	int iMaxCarry;

	if ( FStringNull( iszName ) )
	{
		// error here
		ALERT ( at_console, "NULL String in PackAmmo!\n" );
		return FALSE;
	}
	
	iMaxCarry = MaxAmmoCarry( iszName );

	if ( iMaxCarry != -1 && iCount > 0 )
	{
		//ALERT ( at_console, "Packed %d rounds of %s\n", iCount, STRING(iszName) );
		GiveAmmo( iCount, (char *)STRING( iszName ), iMaxCarry );
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox - GiveAmmo
//=========================================================
int CWeaponBox::GiveAmmo( int iCount, char *szName, int iMax, int *pIndex/* = NULL*/ )
{
	int i;

	for (i = 1; i < MAX_AMMO_SLOTS && !FStringNull( m_rgiszAmmo[i] ); i++)
	{
		if (stricmp( szName, STRING( m_rgiszAmmo[i])) == 0)
		{
			if (pIndex)
				*pIndex = i;

			int iAdd = min( iCount, iMax - m_rgAmmo[i]);
			if (iCount == 0 || iAdd > 0)
			{
				m_rgAmmo[i] += iAdd;

				return i;
			}
			return -1;
		}
	}
	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex)
			*pIndex = i;

		m_rgiszAmmo[i] = MAKE_STRING( szName );
		m_rgAmmo[i] = iCount;

		return i;
	}
	ALERT( at_console, "out of named ammo slots\n");
	return i;
}

//=========================================================
// CWeaponBox::HasWeapon - is a weapon of this type already
// packed in this box?
//=========================================================
BOOL CWeaponBox::HasWeapon( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname) ))
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox::IsEmpty - is there anything in this box?
//=========================================================
BOOL CWeaponBox::IsEmpty( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			return FALSE;
		}
	}

	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// still have a bit of this type of ammo
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
//=========================================================
void CWeaponBox::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-16, -16, 0);
	pev->absmax = pev->origin + Vector(16, 16, 16); 
}


void CBasePlayerWeapon::PrintState( void )
{
	ALERT( at_console, "primary:  %f\n", m_flNextPrimaryAttack );
	ALERT( at_console, "idle   :  %f\n", m_flTimeWeaponIdle );

//	ALERT( at_console, "nextrl :  %f\n", m_flNextReload );
//	ALERT( at_console, "nextpum:  %f\n", m_flPumpTime );

//	ALERT( at_console, "m_frt  :  %f\n", m_fReloadTime );
	ALERT( at_console, "m_finre:  %i\n", m_fInReload );
//	ALERT( at_console, "m_finsr:  %i\n", m_fInSpecialReload );

	ALERT( at_console, "m_iclip:  %i\n", m_iClip );
}


TYPEDESCRIPTION	CRpg::m_SaveData[] = 
{
	DEFINE_FIELD( CRpg, m_fSpotActive, FIELD_INTEGER ),
	DEFINE_FIELD( CRpg, m_cActiveRockets, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CRpg, CBasePlayerWeapon );

TYPEDESCRIPTION	CRpgRocket::m_SaveData[] = 
{
	DEFINE_FIELD( CRpgRocket, m_flIgniteTime, FIELD_TIME ),
	DEFINE_FIELD( CRpgRocket, m_pLauncher, FIELD_CLASSPTR ),
};
IMPLEMENT_SAVERESTORE( CRpgRocket, CGrenade );

TYPEDESCRIPTION	CShotgun::m_SaveData[] = 
{
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	DEFINE_FIELD( CShotgun, m_fInSpecialReload, FIELD_INTEGER ),
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	// DEFINE_FIELD( CShotgun, m_iShell, FIELD_INTEGER ),
	DEFINE_FIELD( CShotgun, m_flPumpTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CShotgun, CBasePlayerWeapon );

TYPEDESCRIPTION	CGauss::m_SaveData[] = 
{
	DEFINE_FIELD( CGauss, m_fInAttack, FIELD_INTEGER ),
//	DEFINE_FIELD( CGauss, m_flStartCharge, FIELD_TIME ),
//	DEFINE_FIELD( CGauss, m_flPlayAftershock, FIELD_TIME ),
//	DEFINE_FIELD( CGauss, m_flNextAmmoBurn, FIELD_TIME ),
	DEFINE_FIELD( CGauss, m_fPrimaryFire, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CGauss, CBasePlayerWeapon );

TYPEDESCRIPTION	CEgon::m_SaveData[] = 
{
//	DEFINE_FIELD( CEgon, m_pBeam, FIELD_CLASSPTR ),
//	DEFINE_FIELD( CEgon, m_pNoise, FIELD_CLASSPTR ),
//	DEFINE_FIELD( CEgon, m_pSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CEgon, m_shootTime, FIELD_TIME ),
	DEFINE_FIELD( CEgon, m_fireState, FIELD_INTEGER ),
	DEFINE_FIELD( CEgon, m_fireMode, FIELD_INTEGER ),
	DEFINE_FIELD( CEgon, m_shakeTime, FIELD_TIME ),
	DEFINE_FIELD( CEgon, m_flAmmoUseTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CEgon, CBasePlayerWeapon );

TYPEDESCRIPTION	CSatchel::m_SaveData[] = 
{
	DEFINE_FIELD( CSatchel, m_chargeReady, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CSatchel, CBasePlayerWeapon );

