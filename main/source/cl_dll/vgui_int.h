//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_INT_H
#define VGUI_INT_H

extern "C"
{
void VGui_Startup();
void VGui_Shutdown();
void* VGui_GetPanel();

//Only safe to call from inside subclass of Panel::paintBackground
void VGui_ViewportPaintBackground(int extents[4]);
}


#endif