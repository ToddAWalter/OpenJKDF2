#ifndef _STDFONT_H
#define _STDFONT_H

#include "types.h"
#include "Primitives/rdRect.h"
#include "General/stdBitmap.h"

#define stdFont_New_ADDR (0x004340E0)
#define stdFont_Load_ADDR (0x00434160)
#define stdFont_Write_ADDR (0x00434480)
#define stdFont_Free_ADDR (0x00434620)
#define stdFont_Draw1_ADDR (0x00434670)
#define stdFont_Draw2_ADDR (0x00434800)
#define stdFont_Draw3_ADDR (0x004349D0)
#define stdFont_Draw4_ADDR (0x00434D70)
#define stdFont_sub_434EC0_ADDR (0x00434EC0)
#define stdFont_DrawAscii_ADDR (0x00434FE0)
#define stdFont_sub_435190_ADDR (0x00435190)
#define stdFont_sub_4352C0_ADDR (0x004352C0)
#define stdFont_sub_435570_ADDR (0x00435570)
#define stdFont_sub_4355B0_ADDR (0x004355B0)
#define stdFont_sub_4355F0_ADDR (0x004355F0)
#define stdFont_sub_4356B0_ADDR (0x004356B0)
#define stdFont_sub_4357C0_ADDR (0x004357C0)
#define stdFont_sub_435810_ADDR (0x00435810)
#define stdFont_sub_4358D0_ADDR (0x004358D0)
#define stdFont_sub_435950_ADDR (0x00435950)

stdFont* stdFont_Load(char *fpath, int a2, int a3);
MATH_FUNC unsigned int stdFont_Draw1(stdVBuffer *vbuf, stdFont *font, unsigned int blit_x, int blit_y, int a5, const wchar_t *a6, int alpha_maybe);
MATH_FUNC void stdFont_Draw2(stdVBuffer *a1, stdFont *a2, unsigned int a3, int a4, rdRect *a5, const wchar_t *a6, int a7);
MATH_FUNC void stdFont_Draw3(stdVBuffer *paintSurface, stdFont *font, int a3, rdRect *a4, int a5, const wchar_t *a6, int a7);
MATH_FUNC int stdFont_Draw4(stdVBuffer *a1, stdFont *font, int xPos, int yPos, int a5, int a6, int a7, const wchar_t *text, int alpha_maybe);
const wchar_t* stdFont_sub_4352C0(const wchar_t *a1, stdFont *a2, int a3, rdRect *a4, int *a5);
int stdFont_sub_4357C0(stdFont *a1, const wchar_t *a2, rdRect *a4);
int stdFont_sub_435810(stdFont *a1, const wchar_t *a2, int a3);
int stdFont_sub_434EC0(stdVBuffer *a1, stdFont *a2, int a3, int a4, int a5, int32_t *a6, const wchar_t *a7, int a8);
void stdFont_Free(stdFont *font);
MATH_FUNC uint32_t stdFont_DrawAscii(stdVBuffer *a1, stdFont *a2, unsigned int blit_x, int blit_y, int x_max, char *str, int alpha_maybe);
int stdFont_sub_4355B0(stdFont *font, uint16_t a2);

MATH_FUNC uint32_t stdFont_DrawAsciiGPU(stdFont *a2, unsigned int blit_x, int blit_y, int x_max, const char *str, int alpha_maybe, flex_t scale);
MATH_FUNC uint32_t stdFont_DrawAsciiWidth(stdFont *a2, unsigned int blit_x, int blit_y, int x_max, const char *str, int alpha_maybe, flex_t scale);
MATH_FUNC int stdFont_Draw4GPU(stdFont *font, int xPos, int yPos, int a5, int a6, int a7, const wchar_t *text, int alpha_maybe, flex_t scale);
MATH_FUNC unsigned int stdFont_Draw1GPU(stdFont *font, unsigned int blit_x, int blit_y, int a5, const wchar_t *a6, int alpha_maybe, flex_t scale);
MATH_FUNC unsigned int stdFont_Draw1Width(stdFont *font, unsigned int blit_x, int blit_y, int a5, const wchar_t *a6, int alpha_maybe, flex_t scale);

MATH_FUNC unsigned int stdFont_DrawMultilineCenteredGPU(stdFont *font, unsigned int blit_x, int blit_y, int a5, const wchar_t *a6, int alpha_maybe, flex_t scale);
MATH_FUNC unsigned int stdFont_DrawMultilineCenteredHeight(stdFont *font, unsigned int blit_x, int blit_y, int a5, const wchar_t *a6, int alpha_maybe, flex_t scale);
//static int (*stdFont_DrawAscii)(stdVBuffer *a1, stdFont *a2, unsigned int blit_x, int blit_y, int x_max, char *str, int alpha_maybe) = (void*)stdFont_DrawAscii_ADDR;

//static int (*stdFont_Draw4)(stdVBuffer *a1, stdFont *font, int xPos, int yPos, int a5, int a6, int a7, wchar_t *text, int alpha_maybe) = (void*)stdFont_Draw4_ADDR;
//static void (*stdFont_Free)(stdFont *font) = (void*)stdFont_Free_ADDR;
//static stdFont* (*stdFont_Load)(char *fpath, int a2, int a3) = (void*)stdFont_Load_ADDR;
//static void (*stdFont_Draw3)(stdVBuffer *a1, stdFont* a2, int a3, rdRect *a4, int a5, wchar_t *a6, int a7) = (void*)stdFont_Draw3_ADDR;
//static int (*stdFont_sub_434EC0)(stdVBuffer* a1, int a2, int a3, int a4, int a5, int a6, wchar_t *a7, int a8) = (void*)stdFont_sub_434EC0_ADDR;
//static int (*stdFont_sub_4355B0)(stdFont* a1, uint16_t a2) = (void*)stdFont_sub_4355B0_ADDR;
//static int (*stdFont_sub_435810)(stdFont* a1, wchar_t *a2, int a3) = (void*)stdFont_sub_435810_ADDR;
//static uint32_t (*stdFont_Draw1)(stdVBuffer *vbuf, stdFont *font, unsigned int blit_x, int blit_y, int a5, wchar_t *a6, int alpha_maybe) = (void*)stdFont_Draw1_ADDR;
//static int (*stdFont_sub_4357C0)(stdFont *a1, wchar_t *text, rdRect *rect) = (void*)stdFont_sub_4357C0_ADDR;
//static void (*stdFont_Draw2)(stdVBuffer *a1, stdFont *a2, unsigned int a3, int a4, rdRect *a5, wchar_t *a6, int a7) = (void*)stdFont_Draw2_ADDR;

// Added: helper
static inline int32_t stdFont_GetHeight(stdFont* pFont) {
    // Added: nullptr checks
    if (!pFont || !pFont->pBitmap || !pFont->pBitmap->mipSurfaces || !pFont->pBitmap->mipSurfaces[0]) return 0;

    return pFont->pBitmap->mipSurfaces[0]->format.height;
}

#endif // _STDFONT_H
