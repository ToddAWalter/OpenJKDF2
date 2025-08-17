#ifndef _JKGUI_H
#define _JKGUI_H

#include "types.h"
#include "globals.h"
#include "Primitives/rdRect.h"

#define jkGui_InitMenu_ADDR (0x004129A0)
#define jkGui_MessageBeep_ADDR (0x00412A10)
#define jkGui_Startup_ADDR (0x00412A20)
#define jkGui_Shutdown_ADDR (0x00412B70)
#define jkGui_SetModeMenu_ADDR (0x00412C00)
#define jkGui_SetModeGame_ADDR (0x00412DF0)
#define jkGui_sub_412E20_ADDR (0x00412E20)
#define jkGui_copies_string_ADDR (0x00412EA0)
#define jkGui_sub_412EC0_ADDR (0x00412EC0)
#define jkGui_sub_412ED0_ADDR (0x00412ED0)

typedef struct jkGuiMenu jkGuiMenu;
typedef struct stdBitmap stdBitmap;
typedef struct stdFont stdFont;
typedef struct jkEpisodeLoad jkEpisodeLoad;

void jkGui_InitMenu(jkGuiMenu *menu, stdBitmap *bgBitmap);
#ifdef JKGUI_SMOL_SCREEN
void jkGui_SmolScreenFixup(jkGuiMenu *menu, BOOL bForce);
#endif
int jkGui_MessageBeep();
int jkGui_Startup();
void jkGui_Shutdown();
int jkGui_SetModeMenu(const void *palette);
void jkGui_SetModeGame();
void jkGui_sub_412E20(jkGuiMenu* menu, int a2, int a3, int a4);
void jkGui_copies_string(char* out);
char *jkGui_sub_412EC0();
wchar_t* jkGui_sub_412ED0();

void jkGui_LoadBmIdx(int idx); // Added

#endif // _JKGUI_H
