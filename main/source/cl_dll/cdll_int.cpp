/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include "hud.h"
#include "cl_util.h"
#include "netadr.h"
#undef INTERFACE_H
#include "../common/interface.h"
#include "vgui_SchemeManager.h"
#include <papi.h>
/*
extern "C"
{
#include "pm_shared.h"
}*/
#include "pm_shared.h"

#include <string.h>
#include "hud_servers.h"
#include "vgui_int.h"

#include "AvHHud.h"
#include "AvHUIFactory.h"
#include "AvHParticleSystemManager.h"
#include "AvHHulls.h"
#include "interface.h"
//#include "itrackeruser.h"

#ifdef _WIN32
#include "winsani_in.h"
#include <windows.h>
#include "winsani_out.h"
#endif
#include "Exports.h"
#
#include "tri.h"
#include "vgui_TeamFortressViewport.h"
#include "interface.h"

#include "APIProxy.h"
//#include "cl_dll/Exports.h"

cl_enginefunc_t gEngfuncs;

// Instead of using default Half-life HUD, use more flexible one I've added
//CHud gHUD;
//UIHud gHUD("StratHL/ui.txt", new AvHUIFactory());
AvHHud gHUD((string(getModDirectory()) + "/ui.txt").c_str(), new AvHUIFactory());

TeamFortressViewport *gViewPort = NULL;

HINTERFACEMODULE g_hTrackerModule = NULL;
//ITrackerUser *g_pTrackerUser = NULL;

CPostProcessShader g_PostProcessShader;

void InitInput (void);
void EV_HookEvents( void );
void IN_Commands( void );

/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int CL_DLLEXPORT HUD_GetHullBounds( int hullnumber, float *mins, float *maxs )
{
//	RecClGetHullBounds(hullnumber, mins, maxs);
	
	int iret = 0;

	switch ( hullnumber )
	{
	case 0:	// Human and level 4 alien, crouched level 5
		HULL0_MIN.CopyToArray(mins);
		HULL0_MAX.CopyToArray(maxs);
		iret = 1;
		break;

	case 1:	// Crouched human, level 2 alien, level 3, crouched level 3 and 4
		HULL1_MIN.CopyToArray(mins);
		HULL1_MAX.CopyToArray(maxs);
		iret = 1;
		break;

	case 2:	 // Point based hull
		HULL2_MIN.CopyToArray(mins);
		HULL2_MAX.CopyToArray(maxs);
		iret = 1;
		break;

	case 3: // Huge alien
		HULL3_MIN.CopyToArray(mins);
		HULL3_MAX.CopyToArray(maxs);
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	CL_DLLEXPORT HUD_ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	//RecClConnectionlessPacket(net_from, args, response_buffer, response_buffer_size);

	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void CL_DLLEXPORT HUD_PlayerMoveInit( struct playermove_s *ppmove )
{
	//RecClClientMoveInit(ppmove);

	PM_Init( ppmove );
}

char CL_DLLEXPORT HUD_PlayerMoveTexture( char *name )
{
//	RecClClientTextureType(name);

	return PM_FindTextureType( name );
}

void CL_DLLEXPORT HUD_PlayerMove( struct playermove_s *ppmove, int server )
{
	//RecClClientMove(ppmove, server);

	PM_Move( ppmove, server );
}

int CL_DLLEXPORT Initialize( cl_enginefunc_t *pEnginefuncs, int iVersion )
{
	gEngfuncs = *pEnginefuncs;

	//RecClInitialize(pEnginefuncs, iVersion);

	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	memcpy(&gEngfuncs, pEnginefuncs, sizeof(cl_enginefunc_t));

	EV_HookEvents();
	gHUD.InitExploitPrevention();

	return 1;
}


/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

int CL_DLLEXPORT HUD_VidInit( void )
{
	gHUD.InitExploitPrevention();
//	RecClHudVidInit();
	gHUD.VidInit();

	VGui_Startup();

	return 1;
}

/*
==========================
HUD_Init

  Called whenever the client connects
  to a server.  Reinitializes all 
  the hud variables.
  ==========================
*/

void CL_DLLEXPORT HUD_Init( void )
{
//	RecClHudInit();
	InitInput();
	gHUD.Init();
	Scheme_Init();
	g_PostProcessShader.Init();
	//gEngfuncs.Con_Printf("waterrenderer init %d", success);
}

/*
void UIDrawVariableBarSpriteHoles(int inSprite, int inX, int inY, float inPercentage)
{
	// Assumes that frame 0 is the empty sprite, frame 1 is full sprite
	const int kEmptyFrame = 0;
	const int kFullFrame = 1;
	
	int theSpriteWidth = SPR_Width(inSprite, kFullFrame);
	int theSpriteHeight = SPR_Height(inSprite, kFullFrame);
	
	int theColorComponent = 255;
	
	// Draw empty sprite
	SPR_Set(inSprite, theColorComponent, theColorComponent, theColorComponent);
	SPR_DrawHoles(kEmptyFrame, inX, inY, NULL);
	
	// Draw secondary level if specified, at half brightness
	int theFilledHeight = theSpriteHeight*inPercentage;
	theFilledHeight = theSpriteHeight*inPercentage;
	
	// Draw partially full sprite.  Enable scissor so it's not all drawn.
	SPR_EnableScissor(inX, inY + (theSpriteHeight - theFilledHeight), theSpriteWidth, theFilledHeight);
	
	SPR_Set(inSprite, theColorComponent, theColorComponent, theColorComponent);
	SPR_DrawHoles(kFullFrame, inX, inY, NULL);
	
	SPR_DisableScissor();
}
*/
// Demonstrates black lines around the edge of sprites (both using tri API and SPR_* calls)
// Demonstrates scissor not working properly

/*
==========================
HUD_Redraw

  called every screen frame to
  redraw the HUD.
  ===========================
*/

int CL_DLLEXPORT HUD_Redraw( float time, int intermission )
{
	g_PostProcessShader.DrawShader();

//	RecClHudRedraw(time, intermission);

	gHUD.Redraw( time, intermission );

	//Direct3DTest();

	//SoftwareTest();

	return 1;
}


/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int CL_DLLEXPORT HUD_UpdateClientData(client_data_t *pcldata, float flTime )
{
//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();

	return gHUD.UpdateClientData(pcldata, flTime );
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void CL_DLLEXPORT HUD_Reset( void )
{
//	RecClHudReset();

	gHUD.VidInit();
	AvHParticleSystemManager::Instance()->Reset();
}

/*
==========================
HUD_Frame

  Called by engine every frame that client .dll is loaded
  ==========================
*/

void CL_DLLEXPORT HUD_Frame( double time )
{
//	RecClHudFrame(time);
	
	ServersThink( time );
	
	GetClientVoiceMgr()->Frame(time);
}


/*
==========================
HUD_VoiceStatus

  Called when a player starts or stops talking.
  ==========================
*/

void CL_DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
//	RecClVoiceStatus(entindex, bTalking);
	
	GetClientVoiceMgr()->UpdateSpeakerStatus(entindex, bTalking);
}

/*
==========================
HUD_DirectorEvent

  Called when a director event message was received
  ==========================
*/

void CL_DLLEXPORT HUD_DirectorMessage( int iSize, void *pbuf )
{
//	RecClDirectorMessage(iSize, pbuf);
    gHUD.m_Spectator.DirectorMessage( iSize, pbuf );
}



cldll_func_dst_t *g_pcldstAddrs;

extern "C" void CL_DLLEXPORT F(void *pv)
{
	cldll_func_t *pcldll_func = (cldll_func_t *)pv;

	// Hack!
	g_pcldstAddrs = ((cldll_func_dst_t *)pcldll_func->pHudVidInitFunc);

	cldll_func_t cldll_func = 
	{
	Initialize,
	HUD_Init,
	HUD_VidInit,
	HUD_Redraw,
	HUD_UpdateClientData,
	HUD_Reset,
	HUD_PlayerMove,
	HUD_PlayerMoveInit,
	HUD_PlayerMoveTexture,
	IN_ActivateMouse,
	IN_DeactivateMouse,
	IN_MouseEvent,
	IN_ClearStates,
	IN_Accumulate,
	CL_CreateMove,
	CL_IsThirdPerson,
	CL_CameraOffset,
	KB_Find,
	CAM_Think,
	V_CalcRefdef,
	HUD_AddEntity,
	HUD_CreateEntities,
	HUD_DrawNormalTriangles,
	HUD_DrawTransparentTriangles,
	HUD_StudioEvent,
	HUD_PostRunCmd,
	HUD_Shutdown,
	HUD_TxferLocalOverrides,
	HUD_ProcessPlayerState,
	HUD_TxferPredictionData,
	Demo_ReadBuffer,
	HUD_ConnectionlessPacket,
	HUD_GetHullBounds,
	HUD_Frame,
	HUD_Key_Event,
	HUD_TempEntUpdate,
	HUD_GetUserEntity,
	HUD_VoiceStatus,
	HUD_DirectorMessage,
	HUD_GetStudioModelInterface,
	//HUD_ChatInputPosition,
	};

	*pcldll_func = cldll_func;
}

#include "cl_dll/IGameClientExports.h"

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	virtual const char *GetServerHostName()
	{
		/*if (gViewPortInterface)
		{
			return gViewPortInterface->GetServerName();
		}*/
		return "";
	}

	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex)
	{
		if (GetClientVoiceMgr())
			return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
		return false;
	}

	virtual void MutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, true);
		}
	}

	virtual void UnmutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, false);
		}
	}
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION);
