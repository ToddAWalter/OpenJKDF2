#include "jkGUITitle.h"

#include "General/Darray.h"
#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "General/stdStrTable.h"
#include "General/stdFileUtil.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Cog/jkCog.h"
#include "Main/jkStrings.h"
#include "Win95/stdDisplay.h"
#include "World/sithWorld.h"
#include "World/jkPlayer.h"
#include "General/stdString.h"
#include "General/stdFnames.h"
#include "General/stdMath.h"
#include "Platform/std3D.h"

static wchar_t jkGuiTitle_tmpBuffer[512];
static wchar_t jkGuiTitle_versionBuffer[64];
static flex_t jkGuiTitle_loadPercent;

static jkGuiElement jkGuiTitle_elementsLoad[6] = {
    {ELEMENT_TEXT,  0,  2,  0,  3, {250, 50, 390, 80},  1,  0,  0,  0,  0,  0, {0},  0},
    {ELEMENT_CUSTOM,  0,  0, .origExtraInt = 0xE1,  0, {330, 131, 240, 20},  1,  0,  0, jkGuiTitle_LoadBarDraw,  0,  0, {0},  0},
    {ELEMENT_TEXT,  0,  0, "GUI_LOADING",  3, {330, 152, 240, 20},  1,  0,  0,  0,  0,  0, {0},  0},
    
#ifdef QOL_IMPROVEMENTS
    { ELEMENT_CUSTOM,  0,  8,  0,  0, {310, 200, 280, 230},  1,  0,  0, jkGuiTitle_UnkDraw,  0,  0, {0},  0},
    { ELEMENT_TEXTBUTTON,  1,            2, "GUI_OK",               3, {440, 430, 200, 40}, 1, 0, NULL,                        0, 0, 0, {0}, 0},
#else
    { ELEMENT_CUSTOM,  0,  8,  0,  0, {310, 200, 280, 275},  1,  0,  0, jkGuiTitle_UnkDraw,  0,  0, {0},  0},
#endif
    {ELEMENT_END,  0,  0,  0,  0, {0},  0,  0,  0,  0,  0,  0, {0},  0}
};

static jkGuiMenu jkGuiTitle_menuLoad = {jkGuiTitle_elementsLoad, -1, 0xFF, 0xE1, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static jkGuiElement jkGuiTitle_elementsLoadStatic[6] = {
    {ELEMENT_TEXT,  0,  2, "GUI_LOADING",  3, {60, 280, 520, 30},  1,  0,  0,  0,  0,  0, {0},  0},
    {ELEMENT_CUSTOM,  0,  0, .origExtraInt = 0xE1,  0, {220, 240, 200, 20},  1,  0,  0, jkGuiTitle_LoadBarDraw,  0,  0, {0},  0},
    {ELEMENT_TEXT,  0,  1, "GUI_COPYRIGHT1",  3, {10, 420, 620, 30},  1,  0,  0,  0,  0,  0, {0},  0},
    {ELEMENT_TEXT,  0,  1, "GUI_COPYRIGHT2",  3, {10, 440, 620, 30},  1,  0,  0,  0,  0,  0, {0},  0},
    {ELEMENT_TEXT,  0,  0,  0,  3, {560, 440, 70, 30},  1,  0,  0,  0,  0,  0, {0},  0},
    {ELEMENT_END,  0,  0,  0,  0, {0},  0,  0,  0,  0,  0,  0, {0},  0}
};

static jkGuiMenu jkGuiTitle_menuLoadStatic = {jkGuiTitle_elementsLoadStatic, -1, 0xFF, 0xE3, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void jkGuiTitle_Startup()
{
    jkGui_InitMenu(&jkGuiTitle_menuLoadStatic, jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]);
    jkGui_InitMenu(&jkGuiTitle_menuLoad, jkGui_stdBitmaps[JKGUI_BM_BK_LOADING]);
#ifdef QOL_IMPROVEMENTS
    jkGuiTitle_elementsLoad[4].bIsVisible = 0;
#endif
#ifdef JKGUI_SMOL_SCREEN
    // Make sure the copyright text isn't garbled
    jkGuiTitle_elementsLoadStatic[2].rect = jkGuiTitle_elementsLoadStatic[2].rectOrig;
    jkGuiTitle_elementsLoadStatic[3].rect = jkGuiTitle_elementsLoadStatic[3].rectOrig;
    jkGuiTitle_elementsLoadStatic[4].rect = jkGuiTitle_elementsLoadStatic[4].rectOrig;
    jkGuiTitle_elementsLoadStatic[2].rect.y -= 40;
    jkGuiTitle_elementsLoadStatic[2].rect.height += 40;
    jkGuiTitle_elementsLoadStatic[2].bIsSmolDirty = 1;
    jkGuiTitle_elementsLoadStatic[3].bIsSmolDirty = 1;
    jkGuiTitle_elementsLoadStatic[4].bIsSmolDirty = 1;
    jkGui_SmolScreenFixup(&jkGuiTitle_menuLoadStatic, 0);

    // Increase the size of the map loading text area
    jkGuiTitle_elementsLoad[0].rect = jkGuiTitle_elementsLoad[0].rectOrig;
    jkGuiTitle_elementsLoad[1].rect = jkGuiTitle_elementsLoad[1].rectOrig;
    jkGuiTitle_elementsLoad[2].rect = jkGuiTitle_elementsLoad[2].rectOrig;
    jkGuiTitle_elementsLoad[3].rect = jkGuiTitle_elementsLoad[3].rectOrig;
    
    jkGuiTitle_elementsLoad[0].rect.x -= 150;
    jkGuiTitle_elementsLoad[1].rect.x -= 150;
    jkGuiTitle_elementsLoad[2].rect.x -= 150;
    jkGuiTitle_elementsLoad[3].rect.x -= 150;

    jkGuiTitle_elementsLoad[0].rect.width += 150;
    jkGuiTitle_elementsLoad[1].rect.width += 150;
    jkGuiTitle_elementsLoad[2].rect.width += 150;
    jkGuiTitle_elementsLoad[3].rect.width += 150;

    jkGuiTitle_elementsLoad[2].rect.y += 20;
    jkGuiTitle_elementsLoad[3].rect.y += 20;
    jkGuiTitle_elementsLoad[3].rect.height -= 20;
    
    jkGuiTitle_elementsLoad[0].bIsSmolDirty = 1;
    jkGuiTitle_elementsLoad[1].bIsSmolDirty = 1;
    jkGuiTitle_elementsLoad[2].bIsSmolDirty = 1;
    jkGuiTitle_elementsLoad[3].bIsSmolDirty = 1;
    jkGui_SmolScreenFixup(&jkGuiTitle_menuLoad, 0);
#endif
}

void jkGuiTitle_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
    
    // Added: clean reset
    memset(jkGuiTitle_versionBuffer, 0, sizeof(jkGuiTitle_versionBuffer));
    jkGuiTitle_loadPercent = 0;
}

char jkGuiTitle_sub_4189A0(char *a1)
{
    char *v1; // esi
    char result; // al

    v1 = a1;
    for ( result = *a1; result; ++v1 )
    {
        *v1 = _string_modify_idk(*v1);
        result = v1[1];
    }
    return result;
}

wchar_t* jkGuiTitle_quicksave_related_func1(stdStrTable *strTable, char *jkl_fname)
{
    wchar_t *retval;
    jkGuiStringEntry *texts;
    char key[64];
    char tmp[64];

    stdFnames_CopyShortName(key, 64, jkl_fname);
    jkGuiTitle_sub_4189A0(key);

    // Added: Allow openjkdf2_i8n.uni to override everything
#ifdef QOL_IMPROVEMENTS
    retval = stdStrTable_GetUniString(&jkStrings_tableExtOver, key);
    if ( !retval )
#endif

    retval = stdStrTable_GetUniString(strTable, key);
    if ( !retval )
        retval = jkStrings_GetUniStringWithFallback(key);

    texts = jkGuiTitle_aTexts;
    // Added: cleanup
    for (int i = 0; i < 20; i++)
    {
        if (texts->str) {
            std_pHS->free(texts->str);
            texts->str = NULL;
        }
    }
    _memset(jkGuiTitle_aTexts, 0, sizeof(jkGuiTitle_aTexts));

    for (int i = 0; i < 20; i++)
    {
        stdString_snprintf(tmp, 64, "%s_TEXT_%02d", key, i);

        wchar_t* pTextStr = NULL;

        // Added: Allow openjkdf2_i8n.uni to override everything
#ifdef QOL_IMPROVEMENTS
        pTextStr = stdStrTable_GetUniString(&jkStrings_tableExtOver, key);
        if ( !pTextStr )
#endif

        pTextStr = stdStrTable_GetUniString(&jkCog_strings, tmp);
        texts->str = stdString_FastWCopy(pTextStr);
        ++texts;
    }

    if (retval) {
        stdString_SafeWStrCopy(jkGuiTitle_tmpBuffer, retval, sizeof(jkGuiTitle_tmpBuffer) / sizeof(wchar_t));
        retval = jkGuiTitle_tmpBuffer;
    }

    return retval;
}

void jkGuiTitle_UnkDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int a4)
{
    int v4; // esi
    jkGuiStringEntry *v5; // ecx
    wchar_t *v6; // ebx
    int32_t result; // eax
    int v8; // ecx
    int v9; // edi
    int v10; // edx
    int v11; // edx
    int32_t v12; // edi
    stdFont **v13; // edx
    int v14; // esi
    rdRect a4a; // [esp+10h] [ebp-10h] BYREF
    jkGuiStringEntry *v16; // [esp+30h] [ebp+10h]

    if ( a4 )
        jkGuiRend_CopyVBuffer(menu, &element->rect);
    v4 = element->rect.y;
    v5 = jkGuiTitle_aTexts;
    v16 = jkGuiTitle_aTexts;
    
    for (int i = 0; i < 20; i++)
    {
        v6 = v5->str;
        result = 0;
        if ( v5->str )
        {
            if ( *v6 == '^' )
            {
#ifndef JKGUI_SMOL_SCREEN
                result = 2;
#endif
                ++v6;
            }

            v8 = element->rect.width;
            v9 = element->rect.y;
            a4a.x = element->rect.x;
            v10 = element->rect.height;
            a4a.width = v8;
            v11 = v9 + v10 - v4;
            v12 = result;
            a4a.height = v11;
            v13 = menu->fonts;
            a4a.y = v4;
            stdFont_Draw3(vbuf, v13[result], v4, &a4a, 1, v6, 1);
            v14 = stdFont_sub_4357C0(menu->fonts[v12], v6, &a4a) + v4;
            result = stdFont_GetHeight(menu->fonts[v12]);
            v4 = ((uint32_t )(3 * result) >> 2) + v14;
            v5 = v16;
        }
        v16 = ++v5;
    }
}

void jkGuiTitle_LoadBarDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, int bForceRedraw)
{
    rdRect tmp;

    if (!g_app_suspended) {
        return;
    }

    element->selectedTextEntry = stdMath_ClampInt(element->selectedTextEntry, 0, 100);
    if (bForceRedraw) {
        jkGuiRend_CopyVBuffer(menu, &element->rect);
    }
#ifdef JKGUI_SMOL_SCREEN
    tmp = element->rect;
    tmp.x -= 1;
    tmp.y -= 1;
    tmp.width += 2;
    tmp.height += 2;
    stdDisplay_VBufferFill(vbuf, 0xE4, &tmp);
#endif
    jkGuiRend_DrawRect(vbuf, &element->rect, menu->fillColor);
    tmp.x = element->rect.x + 3;
    tmp.y = element->rect.y + 3;
    tmp.width = element->rect.width;
    tmp.height = element->rect.height - 6;
    tmp.width = (element->selectedTextEntry * (element->rect.width - 6)) / 100;
    if (tmp.width > 0 && tmp.height > 0) {
        stdDisplay_VBufferFill(vbuf, element->extraInt, &tmp);
    }
}

void jkGuiTitle_WorldLoadCallback(flex_t percentage)
{
    flex_d_t v1; // st7

    if ( jkGuiTitle_loadPercent != (__int64)percentage )
    {
        jkGuiTitle_loadPercent = (__int64)percentage;
        if ( jkGuiTitle_whichLoading == 1 )
        {
            v1 = (percentage - 60.0) * 0.05 * 100.0;
            if ( v1 <= 5.0 )
                v1 = 5.0;
            jkGuiTitle_elementsLoadStatic[1].selectedTextEntry = (__int64)v1;
            jkGuiRend_UpdateAndDrawClickable(&jkGuiTitle_elementsLoadStatic[1], &jkGuiTitle_menuLoadStatic, 1);
        }
        else
        {
            jkGuiTitle_elementsLoad[1].selectedTextEntry = (__int64)percentage;
            jkGuiRend_UpdateAndDrawClickable(&jkGuiTitle_elementsLoad[1], &jkGuiTitle_menuLoad, 1);
        }
#if defined(SDL2_RENDER) || defined(TARGET_TWL)
#if defined(PLATFORM_POSIX) && !defined(TARGET_TWL)
    static uint64_t lastRefresh = 0;
    // Only update loading bar at 30fps, so that we don't waste time
    // during vsync.
    if (Linux_TimeUs() - lastRefresh < 32*1000) {
        return;
    }

    lastRefresh = Linux_TimeUs();
#endif
    stdDisplay_DDrawGdiSurfaceFlip();
#endif
    }
}

// MOTS altered: Added some string to the printf
void jkGuiTitle_ShowLoadingStatic()
{
    wchar_t *guiVersionStr; // eax
    int verMajor; // [esp-Ch] [ebp-2Ch]
    int verMinor; // [esp-8h] [ebp-28h]
    int verRevision; // [esp-4h] [ebp-24h]
    const wchar_t* verMotsStr;
    //wchar_t v4[16]; // [esp+0h] [ebp-20h] BYREF
    // Added: removed undefined behavior, used to use the stack.....

    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]->palette);
    jkGuiTitle_whichLoading = 1;
    sithWorld_SetLoadPercentCallback(jkGuiTitle_WorldLoadCallback);
    verRevision = jkGuiTitle_verRevision;
    verMinor = jkGuiTitle_verMinor;
    verMajor = jkGuiTitle_verMajor;
    verMotsStr = L""; // TODO?
    guiVersionStr = jkStrings_GetUniStringWithFallback("GUI_VERSION");
    jk_snwprintf(jkGuiTitle_versionBuffer, (sizeof(jkGuiTitle_versionBuffer) / sizeof(wchar_t))-1, guiVersionStr, verMajor, verMinor, verRevision, verMotsStr);
    jkGuiTitle_elementsLoadStatic[4].wstr = jkGuiTitle_versionBuffer;
    jkGuiTitle_elementsLoadStatic[1].selectedTextEntry = 0;
    jkGuiRend_gui_sets_handler_framebufs(&jkGuiTitle_menuLoadStatic);
}

void jkGuiTitle_ShowLoading(char *a1, wchar_t *a2)
{
    wchar_t *v4; // ebx
    int v6; // edi
    char key[64]; // [esp+Ch] [ebp-80h] BYREF
    char v8[64]; // [esp+4Ch] [ebp-40h] BYREF

#ifdef QOL_IMPROVEMENTS
    jkGuiTitle_elementsLoad[4].bIsVisible = 0;
#endif

    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]->palette);
    jkGuiTitle_whichLoading = 2;
    jkGuiRend_SetCursorVisible(0);
    sithWorld_SetLoadPercentCallback(jkGuiTitle_WorldLoadCallback);
    jkGuiTitle_elementsLoad[1].selectedTextEntry = 0;

    v4 = jkGui_sub_412ED0();

    jkGuiTitle_elementsLoad[0].wstr = a2;
    if ( !a2 )
        jkGuiTitle_elementsLoad[0].wstr = v4;
    jkGuiRend_gui_sets_handler_framebufs(&jkGuiTitle_menuLoad);
}

void jkGuiTitle_LoadingFinalize()
{
#ifdef QOL_IMPROVEMENTS
    int shouldSkip = jkPlayer_bFastMissionText || sithNet_isMulti || !sithWorld_pCurrentWorld;
    if ( jkGuiTitle_whichLoading != 1)
    {
        int selected = -1;

        jkGuiTitle_elementsLoad[4].bIsVisible = 1;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiTitle_menuLoad, &jkGuiTitle_elementsLoad[4]);
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiTitle_menuLoad, &jkGuiTitle_elementsLoad[4]);

        while (1)
        {
            if (shouldSkip) break;
            int selected = jkGuiRend_DisplayAndReturnClicked(&jkGuiTitle_menuLoad);

#if defined(SDL2_RENDER) || defined(TARGET_TWL)
#if defined(PLATFORM_POSIX) && !defined(TARGET_TWL)
            static uint64_t lastRefresh = 0;
            // Only update loading bar at 30fps, so that we don't waste time
            // during vsync.
            if (Linux_TimeUs() - lastRefresh < 32*1000) {
                return;
            }

            lastRefresh = Linux_TimeUs();
#endif
            stdDisplay_DDrawGdiSurfaceFlip();
#endif

            break;
        }

        jkGuiTitle_elementsLoad[4].bIsVisible = 0;
    }
#endif

#ifdef SDL2_RENDER
    //std3D_PurgeTextureCache();
#endif
    jkGui_SetModeGame();
    sithWorld_SetLoadPercentCallback(0);
    jkGuiRend_sub_50FDB0();
}
