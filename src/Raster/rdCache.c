#include "rdCache.h"

#include "Engine/rdroid.h"
#include "Engine/rdActive.h"
#include "Platform/std3D.h"
#include "Engine/rdColormap.h"
#include "General/stdMath.h"
#include "stdPlatform.h"

#include <math.h>

#ifdef QOL_IMPROVEMENTS
static int rdCache_totalLines = 0;
static rdLine rdCache_aHWLines[1024];
int rdroid_curVertexColorMode = 0; // MOTS added
#endif

#ifdef JKM_LIGHTING
static flex_t rdCache_aRedIntensities[RDCACHE_MAX_VERTICES];
static flex_t rdCache_aGreenIntensities[RDCACHE_MAX_VERTICES];
static flex_t rdCache_aBlueIntensities[RDCACHE_MAX_VERTICES];
#endif

int rdCache_Startup()
{
    return 1;
}

void rdCache_AdvanceFrame()
{
#if defined(SDL2_RENDER) || defined(TARGET_TWL)
    rdroid_curAcceleration = 1;
#endif

#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    if ( rdroid_curAcceleration > 0 )
#endif
        std3D_StartScene();
}

void rdCache_FinishFrame()
{
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    if ( rdroid_curAcceleration > 0 )
#endif
        std3D_EndScene();
}

void rdCache_Reset()
{
    rdCache_numProcFaces = 0;
    rdCache_numUsedVertices = 0;
    rdCache_numUsedTexVertices = 0;
    rdCache_numUsedIntensities = 0;
    rdCache_ulcExtent.x = 0x7FFFFFFF;
    rdCache_ulcExtent.y = 0x7FFFFFFF;
    rdCache_lrcExtent.x = 0;
    rdCache_lrcExtent.y = 0;
}

void rdCache_ClearFrameCounters()
{
    rdCache_drawnFaces = 0;
}

rdProcEntry *rdCache_GetProcEntry()
{
    size_t idx;
    rdProcEntry *out_procEntry;

    idx = rdCache_numProcFaces;
    if ( rdCache_numProcFaces >= RDCACHE_MAX_TRIS )
    {
        rdCache_Flush();
        idx = rdCache_numProcFaces;
    }

    if ( (unsigned int)(RDCACHE_MAX_VERTICES - rdCache_numUsedVertices) < 0x20 )
        return 0;

    if ( (unsigned int)(RDCACHE_MAX_VERTICES - rdCache_numUsedTexVertices) < 0x20 )
        return 0;

    if ( (unsigned int)(RDCACHE_MAX_VERTICES - rdCache_numUsedIntensities) < 0x20 )
        return 0;

    out_procEntry = &rdCache_aProcFaces[idx];
    out_procEntry->vertices = &rdCache_aVertices[rdCache_numUsedVertices];
    out_procEntry->vertexUVs = &rdCache_aTexVertices[rdCache_numUsedTexVertices];
    out_procEntry->vertexIntensities = &rdCache_aIntensities[rdCache_numUsedIntensities];
#ifdef JKM_LIGHTING
    out_procEntry->paRedIntensities = &rdCache_aRedIntensities[rdCache_numUsedIntensities];
    out_procEntry->paGreenIntensities = &rdCache_aGreenIntensities[rdCache_numUsedIntensities];
    out_procEntry->paBlueIntensities = &rdCache_aBlueIntensities[rdCache_numUsedIntensities];
#endif
    return out_procEntry;
}

void rdCache_Flush()
{
    size_t v0; // eax
    size_t v1; // edi
    size_t v3; // edi
    rdProcEntry *face; // esi

    if (!rdCache_numProcFaces)
        return;

    if ( rdroid_curSortingMethod == 2 )
    {
#ifndef TARGET_TWL
        _qsort(rdCache_aProcFaces, rdCache_numProcFaces, sizeof(rdProcEntry), (int (__cdecl *)(const void *, const void *))rdCache_ProcFaceCompare);
#endif
    }
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    if ( rdroid_curAcceleration <= 0 )
    {
        if ( rdroid_curOcclusionMethod )
        {
            if ( rdroid_curOcclusionMethod == 1 )
            {
                rdActive_AdvanceFrame();
                rdActive_DrawScene();
            }
        }
        else if ( rdroid_curZBufferMethod )
        {

            if ( rdroid_curZBufferMethod == RD_ZBUFFER_READ_WRITE )
            {
                for (v1 = 0; v1 < rdCache_numProcFaces; v1++)
                {
                    face = &rdCache_aProcFaces[v1];
                    if ( (face->extraData & 1) != 0 )
                        rdCache_DrawFaceUser(face);
                    else
                        rdCache_DrawFaceZ(face);
                }
            }
        }
        else
        {
            for (v3 = 0; v3 < rdCache_numProcFaces; v3++)
            {
                face = &rdCache_aProcFaces[v3];
                if ( (face->extraData & 1) != 0 )
                    rdCache_DrawFaceUser(face);
                else
                    rdCache_DrawFaceN(face);
            }

        }
    }
    else
#endif
    {
        rdCache_SendFaceListToHardware();
    }
    rdCache_drawnFaces += rdCache_numProcFaces;
    rdCache_Reset();
}

#ifdef TARGET_TWL
const static flex_t res_fix_x = (1.0/512.0);
const static flex_t res_fix_y = (1.0/384.0);
#endif

#if 1

int rdCache_SendFaceListToHardware()
{
    int v0; // ecx
    int v1; // edx
    flex_d_t v2; // st7
    flex_d_t v3; // st6
    flex_d_t v4; // st5
    rdClipFrustum *v7; // edx
    flex_d_t invZFar; // st7
    int mipmap_level; // edi
    rdProcEntry *active_6c; // esi
    v11_struct v11; // edx
    int expected_alpha; // ecx
    int v14; // eax
    rdTexinfo *v15; // eax
    rdTexture *sith_tex_sel; // esi
    rdDDrawSurface *tex2_arr_sel; // eax
    flex_t *vert_lights_iter; // ecx
    int vert_lights_iter_cnt; // edx
    flex_d_t v21; // st7
    flex_d_t v22; // st7
    flex_d_t v23; // st7
    flex_d_t v24; // st7
    flex_d_t v25; // st7
    flex_d_t v26; // st7
    flex_d_t v27; // st7
    rdVector3 *iterating_6c_vtxs_; // esi
    int v35; // ecx
    flex_d_t v36; // st7
    flex_d_t d3dvtx_zval; // st7
    flex_d_t v38; // st6
    flex_d_t light_level; // st7
    int vertex_g; // ebx
    int vertex_r; // edi
    rdColormap *v45; // eax
    flex_d_t v47; // st7
    __int64 v48; // rax
    flex_d_t v49; // st7
    int vertex_b; // cl
    rdProcEntry *v52; // esi
    int final_vertex_color; // eax
    rdVector2 *uvs_in_pixels; // eax
    flex_d_t tex_v; // st7
    int v61; // ecx
    int lighting_maybe_2; // edx
    unsigned int v63; // edi
    int tri; // eax
    int lighting_maybe; // ebx
    size_t tri_idx; // eax
    flex_t *v70; // ecx
    int v71; // edx
    flex_d_t v72; // st7
    flex_d_t v73; // st7
    flex_d_t v75; // st7
    flex_d_t v76; // st7
    flex_d_t v78; // st7
    rdDDrawSurface *v80; // edx
    flex_d_t v87; // st7
    flex_d_t v88; // st7
    flex_d_t v89; // st6
    rdColormap *v91; // esi
    flex_d_t v92; // st7
    int v93; // eax
    int v94; // ebx
    int v95; // edx
    int v96; // edi
    int v97; // eax
    int v98; // ecx
    __int64 v100; // rax
    flex_d_t v101; // st7
    uint8_t v103; // cl
    int v104; // eax
    unsigned int v108; // edi
    int v109; // ecx
    int v110; // edx
    unsigned int v111; // edi
    int v112; // esi
    rdTri *v114; // eax
    int v115; // ebx
    unsigned int v117; // eax
    flex_t actual_width; // [esp+1Ch] [ebp-84h]
    flex_t actual_height; // [esp+20h] [ebp-80h]
    flex_t v121; // [esp+24h] [ebp-7Ch]
    flex_t green_scalar; // [esp+34h] [ebp-6Ch]
    flex_t blue_scalar; // [esp+38h] [ebp-68h]
    int rend_6c_current_idx; // [esp+3Ch] [ebp-64h]
    flex_t red_scalar; // [esp+40h] [ebp-60h]
    int v129; // [esp+44h] [ebp-5Ch]
    int v130; // [esp+48h] [ebp-58h]
    int vertex_a; // [esp+4Ch] [ebp-54h]
    int alpha_upshifta; // [esp+4Ch] [ebp-54h]
    int alpha_is_opaque; // [esp+50h] [ebp-50h]
    int tri_vert_idx; // [esp+58h] [ebp-48h]
    int flags_idk; // [esp+60h] [ebp-40h]
    rdTexinfo *v137; // [esp+64h] [ebp-3Ch]
    int iterating_6c_vtx_idx; // [esp+64h] [ebp-3Ch]
    int mipmap_related; // [esp+68h] [ebp-38h]
    rdVector3 *iterating_6c_vtxs; // [esp+68h] [ebp-38h]
    unsigned int out_width; // [esp+6Ch] [ebp-34h] BYREF
    unsigned int out_height; // [esp+74h] [ebp-2Ch] BYREF
    int flags_idk_; // [esp+78h] [ebp-28h]
    int a3; // [esp+7Ch] [ebp-24h]
    int lighting_capability; // [esp+80h] [ebp-20h]
    flex_t v148; // [esp+84h] [ebp-1Ch]
    int blue; // [esp+8Ch] [ebp-14h]
    int red_and_alpha; // [esp+98h] [ebp-8h]
    int green; // [esp+9Ch] [ebp-4h]
    int tmpiter;

    //printf("b %f %f %f ", rdroid_curColorEffects.tint.x, rdroid_curColorEffects.tint.z, rdroid_curColorEffects.tint.z);
    //printf("%d %d %d\n", rdroid_curColorEffects.filter.x, rdroid_curColorEffects.filter.z, rdroid_curColorEffects.filter.z);


    a3 = 0; // added? aaaaaaa undefined
    v0 = 0;
    v1 = 0;
    v130 = 0;
    v129 = 0;
    alpha_is_opaque = 0;
    
    switch ( rdroid_curZBufferMethod )
    {
        case RD_ZBUFFER_NOREAD_NOWRITE:
        default:
            flags_idk = 0x33;
            break;
        case RD_ZBUFFER_NOREAD_WRITE:
            flags_idk = 0x1033;
            break;
        case RD_ZBUFFER_READ_WRITE:
            flags_idk = 0x1833;
            break;
        case RD_ZBUFFER_READ_NOWRITE:
            flags_idk = 0x833;
            break;
    }
    if ( rdroid_curColorEffects.tint.x > 0.0 || rdroid_curColorEffects.tint.y > 0.0 || rdroid_curColorEffects.tint.z > 0.0 )
    {
        v2 = rdroid_curColorEffects.tint.y * 0.5;
        v3 = rdroid_curColorEffects.tint.z * 0.5;
        v0 = 1;
        v130 = 1;
        red_scalar = rdroid_curColorEffects.tint.x - (v3 + v2);
        v4 = rdroid_curColorEffects.tint.x * 0.5;
        green_scalar = rdroid_curColorEffects.tint.y - (v4 + v3);
        blue_scalar = rdroid_curColorEffects.tint.z - (v4 + v2);
    }
    if ( rdroid_curColorEffects.filter.x || rdroid_curColorEffects.filter.y || rdroid_curColorEffects.filter.z )
    {
        v1 = 1;
        v129 = 1;
    }

    if ( v0 || v1 || (rdGetVertexColorMode() == 1)) // MOTS added
    {
        flags_idk |= 0x8000;
    }

    std3D_ResetRenderList();
    rdCache_ResetRenderList();
    v7 = rdCamera_pCurCamera->pClipFrustum;
    invZFar = 1.0 / v7->zFar;
    rend_6c_current_idx = 0;
    
    for (rend_6c_current_idx = 0; rend_6c_current_idx < rdCache_numProcFaces; rend_6c_current_idx++)
    {        
        active_6c = &rdCache_aProcFaces[rend_6c_current_idx];
        mipmap_level = a3;

        flags_idk_ = flags_idk;
        if ( (rdroid_curRenderOptions & 2) != 0 )
            v148 = active_6c->ambientLight;
        else
            v148 = 0.0;

        // Added: We need to know if a face is flex_d_t-sided
        if (active_6c->type & RD_FF_DOUBLE_SIDED && active_6c->light_flags)
        {
            flags_idk_ |= 0x10000;
        }

#if defined(SDL2_RENDER) || defined(TARGET_TWL)
        d3d_maxVertices = STD3D_MAX_VERTICES;
#endif
        if ( active_6c->numVertices + rdCache_totalVerts >= d3d_maxVertices )
        {
            rdCache_DrawRenderList();
            rdCache_ResetRenderList();
        }

        v11.mipmap_related = rdroid_curGeometryMode;
        tri_vert_idx = rdCache_totalVerts;

        if ( active_6c->geometryMode < rdroid_curGeometryMode )
            v11.mipmap_related = active_6c->geometryMode;

        mipmap_related = v11.mipmap_related;
        lighting_capability = active_6c->lightingMode;

        if ( lighting_capability >= rdroid_curLightingMode )
            lighting_capability = rdroid_curLightingMode;

        if ( (active_6c->type & RD_FF_TEX_TRANSLUCENT) != 0 )
        {
            expected_alpha = 90;
            flags_idk_ |= 0x200;
            red_and_alpha = 90;
        }
        else
        {
            expected_alpha = 255;
            red_and_alpha = 255;
        }

        if ( expected_alpha != 255 && !std3D_HasModulateAlpha() && !std3D_HasAlphaFlatStippled() )
        {
            red_and_alpha = 255;
            alpha_is_opaque = 1;
        }

        tex2_arr_sel = 0;

        v11.material = active_6c->material;
        if (!v11.material)
        {
            continue;
        }

        // Added
        rdMaterial_EnsureData(v11.material);
#if defined(RDMATERIAL_LRU_LOAD_UNLOAD)
        if (!v11.material->bDataLoaded) {
            continue;
        }
#endif

        v14 = active_6c->wallCel;
        if ( v14 == -1 )
        {
            v14 = v11.material->celIdx;
            if ( v14 >= 0 )
            {
                if ( v14 > v11.material->num_texinfo - 1 )
                    v14 = v11.material->num_texinfo - 1;
            }
        }
        else if ( v14 < 0 )
        {
            v14 = 0;
        }
        else if ( v14 > v11.material->num_texinfo - 1 )
        {
            v14 = v11.material->num_texinfo - 1;
        }

        v15 = v11.material->texinfos ? v11.material->texinfos[v14] : NULL; // Added: v11.material->texinfos nullptr check
        v137 = v15;
        if ( v11.mipmap_related == 4 && (v15 && v15->header.texture_type & 8) == 0 ) // Added: v15 nullptr check
        {
            v11.mipmap_related = 3;
            mipmap_related = 3;
        }
        if ( !v15 || (v15 && (v15->header.texture_type & 8) == 0) || (v15 && !v15->texture_ptr) ) // Added: v15 nullptr check, !v15->texture_ptr
        {
            tex2_arr_sel = 0;
        }
        else
        {
            sith_tex_sel = v15->texture_ptr;

            flex_t z_min = active_6c->z_min * rdCamera_GetMipmapScalar(); // MOTS added
            
            mipmap_level = 1;
            if (sith_tex_sel->num_mipmaps == 2)
            {
                if ( z_min <= (flex_d_t)rdroid_aMipDistances.y )
                {
                    mipmap_level = 0;
                }
            }
            else if (sith_tex_sel->num_mipmaps == 3)
            {
                if ( z_min <= (flex_d_t)rdroid_aMipDistances.x )
                {
                    mipmap_level = 0;
                }
                else if ( z_min > (flex_d_t)rdroid_aMipDistances.y )
                {
                    mipmap_level = 2;
                }
            }
            else if (sith_tex_sel->num_mipmaps == 4)
            {
                if ( z_min <= (flex_d_t)rdroid_aMipDistances.x )
                {
                    mipmap_level = 0;
                }
                else if ( z_min > (flex_d_t)rdroid_aMipDistances.y )
                {
                    if ( z_min > (flex_d_t)rdroid_aMipDistances.z )
                        mipmap_level = 3;
                    else
                        mipmap_level = 2;
                }
            }
            else if (sith_tex_sel->num_mipmaps == 1)
            {
                mipmap_level = 0;
            }

            // Look for the closest mipmap that's been loaded
#ifdef TARGET_TWL
            int mipmap_level_orig = mipmap_level;
            stdVBuffer* mipmap = sith_tex_sel->texture_struct[mipmap_level];

            alpha_is_opaque = 1;
            while (!mipmap && mipmap_level < sith_tex_sel->num_mipmaps) {
                mipmap = sith_tex_sel->texture_struct[mipmap_level];
                if (mipmap) {
                    break;
                }

                mipmap_level += 1;
            }

            if (!mipmap) {
                mipmap_level = mipmap_level_orig;
                while (!mipmap && mipmap_level > 0) {
                    mipmap = sith_tex_sel->texture_struct[mipmap_level];
                    if (mipmap) {
                        break;
                    }

                    mipmap_level -= 1;
                }
            }

            if (!mipmap) {
                mipmap_level = mipmap_level_orig;
            }
#endif

            a3 = mipmap_level;

            if ( (sith_tex_sel->alpha_en & 1) != 0 && std3D_HasAlpha() )
            {
#ifdef SDL2_RENDER
                if (sith_tex_sel->has_jkgm_override)
                {
                    tex2_arr_sel = &sith_tex_sel->alphaMats[mipmap_level];
                    if (tex2_arr_sel->albedo_factor[0] != 0.0
                        || tex2_arr_sel->albedo_factor[1] != 0.0
                        || tex2_arr_sel->albedo_factor[2] != 0.0
                        || tex2_arr_sel->albedo_factor[3] != 0.0)
                    {
                        flags_idk_ |= 0x0;
                    }
                    else {
                        // If a texture is only emissive (blaster shots, etc)
                        // then we probably want to keep the existing blend modes
                        flags_idk_ |= 0x400;
                    }
                }
                else {
                    flags_idk_ |= 0x400;
                }
#else
                flags_idk_ |= 0x400;
#endif
            }

            if ( !rdMaterial_AddToTextureCache(v11.material, sith_tex_sel, mipmap_level, alpha_is_opaque, v14) )
            {
                rdCache_DrawRenderList();
                rdCache_ResetRenderList();
                if ( !rdMaterial_AddToTextureCache(v11.material, sith_tex_sel, mipmap_level, alpha_is_opaque, v14) )
                    return 0;
            }

            tex2_arr_sel = &sith_tex_sel->alphaMats[mipmap_level];

            if ( alpha_is_opaque )
                tex2_arr_sel = &sith_tex_sel->opaqueMats[mipmap_level];

            // Added: nullptr check and fallback
            if (sith_tex_sel && sith_tex_sel->texture_struct[mipmap_level]) {
                std3D_GetValidDimension(
                    sith_tex_sel->texture_struct[mipmap_level]->format.width,
                    sith_tex_sel->texture_struct[mipmap_level]->format.height,
                    &out_width,
                    &out_height);
            }
            else {
                out_width = 8;
                out_height = 8;
            }
            
            v11.mipmap_related = mipmap_related;
            actual_width = (flex_t)(out_width << mipmap_level); // FLEXTODO
            actual_height = (flex_t)(out_height << mipmap_level); // FLEXTODO
        }

#ifdef TARGET_TWL
        // We need to know if a triangle is sky texture (affine texture)
        if (active_6c->textureMode == RD_TEXTUREMODE_AFFINE) {
            flags_idk_ |= 0x20000;
        }
#endif

        if ( v11.mipmap_related != 3 )
        {
            v26 = v148;
            if ( v11.mipmap_related != 4 )
                continue;
            if ( lighting_capability == 1 )
            {
                v27 = stdMath_Clamp(active_6c->extralight, 0.0, 1.0);

                if ( v27 > v148 )
                {
                    v26 = stdMath_Clamp(active_6c->extralight, 0.0, 1.0);
                }
                else
                {
                    v26 = v148;
                }

                active_6c->light_level_static = v26 * 255.0;
            }
            else if ( lighting_capability == 2 )
            {
                v24 = active_6c->extralight + active_6c->light_level_static;
                v25 = stdMath_Clamp(v24, 0.0, 1.0);
                v26 = stdMath_Clamp(v25, v148, 1.0);

                active_6c->light_level_static = v26 * 255.0;
            }
            else if ( lighting_capability == 3 && active_6c->numVertices )
            {
                // MOTS added
#ifdef JKM_LIGHTING
                if (rdGetVertexColorMode() == 1) {
                    flex_t* iterRedIntense = active_6c->paRedIntensities;
                    flex_t* iterGreenIntense = active_6c->paGreenIntensities;
                    flex_t* iterBlueIntense = active_6c->paBlueIntensities;

                    vert_lights_iter_cnt = active_6c->numVertices;
                    do
                    {
                        *iterRedIntense = stdMath_Clamp(stdMath_Clamp(*iterRedIntense + active_6c->extralight, 0.0, 1.0), v148, 1.0) * 255.0;
                        *iterGreenIntense = stdMath_Clamp(stdMath_Clamp(*iterGreenIntense + active_6c->extralight, 0.0, 1.0), v148, 1.0) * 255.0;
                        *iterBlueIntense = stdMath_Clamp(stdMath_Clamp(*iterBlueIntense + active_6c->extralight, 0.0, 1.0), v148, 1.0) * 255.0;

                        ++iterRedIntense;
                        ++iterGreenIntense;
                        ++iterBlueIntense;
                        --vert_lights_iter_cnt;
                    }
                    while ( vert_lights_iter_cnt );
                }
                else
#endif
                {
                    vert_lights_iter = active_6c->vertexIntensities;
                    vert_lights_iter_cnt = active_6c->numVertices;
                    do
                    {
                        v21 = *vert_lights_iter + active_6c->extralight;

                        v22 = stdMath_Clamp(v21, 0.0, 1.0);
                        v23 = stdMath_Clamp(v22, v148, 1.0);

                        *vert_lights_iter = v23 * 255.0;
                        ++vert_lights_iter;
                        --vert_lights_iter_cnt;
                    }
                    while ( vert_lights_iter_cnt );
                }

                //active_6c->light_level_static = v26 * 255.0;
            }

            iterating_6c_vtxs = active_6c->vertices;
            vertex_a = red_and_alpha << 8;

            for (int vtx_idx = 0; vtx_idx < active_6c->numVertices; vtx_idx++)
            {
#ifndef TARGET_TWL
                rdCache_aHWVertices[rdCache_totalVerts].x = (iterating_6c_vtxs[vtx_idx].x); // Added: The original game rounded to ints here (with ceilf?)
                rdCache_aHWVertices[rdCache_totalVerts].y = (active_6c->vertices[vtx_idx].y); // Added: The original game rounded to ints here (with ceilf?)
#endif
                iterating_6c_vtxs_ = active_6c->vertices;

                // DSi prefers z vertices directly
#ifdef TARGET_TWL
                v36 = iterating_6c_vtxs_[vtx_idx].z;
                iterating_6c_vtxs = iterating_6c_vtxs_;
                d3dvtx_zval = iterating_6c_vtxs_[vtx_idx].z;
                v38 = (flex_t)1.0 - d3dvtx_zval;
#else
                v36 = iterating_6c_vtxs_[vtx_idx].z;
                iterating_6c_vtxs = iterating_6c_vtxs_;
                if ( v36 == 0.0 )
                    d3dvtx_zval = 0.0;
                else
                    d3dvtx_zval = 1.0 / iterating_6c_vtxs_[vtx_idx].z;
                v38 = d3dvtx_zval * invZFar;
#endif
                if ( rdCache_dword_865258 != 16 )
                    v38 = 1.0 - v38;
#ifdef TARGET_TWL
                rdCache_aHWVertices[rdCache_totalVerts].x = (active_6c->vertices[vtx_idx].x * res_fix_x) * v38; // Added: The original game rounded to ints here (with ceilf?)
                rdCache_aHWVertices[rdCache_totalVerts].y = (active_6c->vertices[vtx_idx].y * res_fix_y) * v38; // Added: The original game rounded to ints here (with ceilf?)
                v38 = stdMath_Clamp(v38, 0.0, SITHCAMERA_ZFAR);
#endif
                rdCache_aHWVertices[rdCache_totalVerts].z = v38;

                // Don't waste time with this on DSi
#ifdef TARGET_TWL
                rdCache_aHWVertices[rdCache_totalVerts].nx = 0.0;
#else
                rdCache_aHWVertices[rdCache_totalVerts].nx = d3dvtx_zval / 32.0;
#endif
                rdCache_aHWVertices[rdCache_totalVerts].nz = 0.0;
                if ( lighting_capability == 0 )
                {
                    vertex_b = 255;
                    vertex_g = 255;
                    blue = 255;
                    green = 255;
                    vertex_r = 255;
#ifdef SDL2_RENDER
                    rdCache_aHWVertices[rdCache_totalVerts].lightLevel = 1.0;
#endif

                    /*vertex_r = 0;
                    vertex_g = 0;
                    green = 0;
                    vertex_b = 255;
                    blue = 255;*/
                }
                else
                {
                    // MOTS added
                    if (rdGetVertexColorMode() != 1 || lighting_capability != 3)
                    {
                        if ( lighting_capability == 3 )
                            light_level = active_6c->vertexIntensities[vtx_idx];
                        else
                            light_level = active_6c->light_level_static;
#ifdef SDL2_RENDER
                        rdCache_aHWVertices[rdCache_totalVerts].lightLevel = light_level / 255.0;
#endif
                        

                        vertex_b = (__int64)light_level;
                        vertex_g = vertex_b;
                        vertex_r = vertex_b;
                        blue = vertex_b;
                        green = vertex_b;

                        /*vertex_r = 255;
                    vertex_g = 0;
                    green = 0;
                    vertex_b = 255;
                    blue = 255;*/
                    }
#ifdef JKM_LIGHTING
                    else
                    {
                        //printf("%f\n", active_6c->paRedIntensities[vtx_idx]);
                        light_level = 1.0;
                        flex_d_t intRed = active_6c->paRedIntensities[vtx_idx];
                        flex_d_t intGreen = active_6c->paGreenIntensities[vtx_idx];
                        flex_d_t intBlue = active_6c->paBlueIntensities[vtx_idx];

                        // Added for SDL2
#ifdef SDL2_RENDER
                        flex_d_t luma = (0.2126 * intRed) + (0.7152 * intGreen) + (0.0722 * intBlue);
                        light_level = luma;

                        rdCache_aHWVertices[rdCache_totalVerts].lightLevel = luma / 255.0;
#endif

                        vertex_b = (int)intBlue;
                        vertex_g = (int)intGreen;
                        vertex_r = (int)intRed;
                        blue = vertex_b;
                        green = vertex_g;

                        /*vertex_r = 0;
                    vertex_g = 255;
                    green = 255;
                    vertex_b = 0;
                    blue = 0;*/

                    }
#endif
                }

                red_and_alpha = vertex_r;
                v45 = active_6c->colormap;
                if ( v45 != rdColormap_pIdentityMap )
                {
                    v47 = v45->tint.y * (flex_d_t)green;
                    vertex_r = (uint8_t)(__int64)(v45->tint.x * (flex_d_t)red_and_alpha);
                    red_and_alpha = vertex_r;
                    v48 = (__int64)v47;
                    v49 = v45->tint.z * (flex_d_t)blue;
                    vertex_g = (uint8_t)v48;
                    green = (uint8_t)v48;
                    vertex_b = (uint8_t)(__int64)v49;
                    flags_idk_ |= 0x8000;
                    blue = vertex_b;
                }
                if ( v129 )
                {
                    if ( !rdroid_curColorEffects.filter.x )
                    {
                        vertex_r = 0;
                        red_and_alpha = 0;
                    }
                    if ( !rdroid_curColorEffects.filter.y )
                    {
                        vertex_g = 0;
                        green = 0;
                    }
                    if ( !rdroid_curColorEffects.filter.z )
                    {
                        vertex_b = 0;
                        blue = 0;
                    }
                }
                if ( v130 )
                {
                    vertex_r += (__int64)((flex_d_t)red_and_alpha * red_scalar);
                    red_and_alpha = vertex_r;
                    vertex_g += (__int64)((flex_d_t)green * green_scalar);
                    green = vertex_g;
                    vertex_b += (__int64)((flex_d_t)blue * blue_scalar);
                    blue = vertex_b;
                }
                if ( rdroid_curColorEffects.fade < 1.0 )
                {
                    vertex_r = (__int64)((flex_d_t)red_and_alpha * rdroid_curColorEffects.fade);
                    vertex_g = (__int64)((flex_d_t)green * rdroid_curColorEffects.fade);
                    vertex_b = (__int64)((flex_d_t)blue * rdroid_curColorEffects.fade);
                }

                vertex_r = stdMath_ClampInt(vertex_r, 0, 255);
                vertex_g = stdMath_ClampInt(vertex_g, 0, 255);
                vertex_b = stdMath_ClampInt(vertex_b, 0, 255);

                v52 = active_6c;
                final_vertex_color = vertex_b | (((uint8_t)vertex_g | ((vertex_a | (uint8_t)vertex_r) << 8)) << 8);
                
                // For some reason, ny holds the vertex color.
                rdCache_aHWVertices[rdCache_totalVerts].color = final_vertex_color;
                uvs_in_pixels = v52->vertexUVs;

                // DSi wants UVs in pixels
#if defined(TARGET_TWL) && defined(EXPERIMENTAL_FIXED_POINT)
                rdCache_aHWVertices[rdCache_totalVerts].tu = uvs_in_pixels[vtx_idx].x >> mipmap_level;
                rdCache_aHWVertices[rdCache_totalVerts].tv = uvs_in_pixels[vtx_idx].y >> mipmap_level;
#else
                rdCache_aHWVertices[rdCache_totalVerts].tu = uvs_in_pixels[vtx_idx].x / actual_width;
                rdCache_aHWVertices[rdCache_totalVerts].tv = uvs_in_pixels[vtx_idx].y / actual_height;
#endif
                
                ++rdCache_totalVerts;
            }
#ifdef QOL_IMPROVEMENTS
            if ( active_6c->numVertices <= 2 )
            {
                tri_idx = rdCache_totalLines;
                rdCache_aHWLines[tri_idx].v2 = tri_vert_idx;
                rdCache_aHWLines[tri_idx].v1 = tri_vert_idx + 1;
                rdCache_aHWLines[tri_idx].flags = flags_idk_;
                
                rdCache_aHWVertices[rdCache_aHWLines[tri_idx].v1].color = active_6c->extraData;
                rdCache_aHWVertices[rdCache_aHWLines[tri_idx].v2].color = active_6c->extraData;

                rdCache_aHWVertices[rdCache_aHWLines[tri_idx].v1].nz = 0.0;
                rdCache_aHWVertices[rdCache_aHWLines[tri_idx].v2].nz = 0.0;
                
                rdCache_totalLines++;
            }
            else 
#endif
            if ( active_6c->numVertices <= 3 )
            {
                tri_idx = rdCache_totalNormalTris;
                rdCache_aHWNormalTris[tri_idx].v3 = tri_vert_idx;
                rdCache_aHWNormalTris[tri_idx].v2 = tri_vert_idx + 1;
                rdCache_aHWNormalTris[tri_idx].v1 = tri_vert_idx + 2;
                rdCache_aHWNormalTris[tri_idx].flags = flags_idk_;
                rdCache_aHWNormalTris[tri_idx].texture = tex2_arr_sel;
                rdCache_totalNormalTris++;
            }
            else
            {
                v61 = active_6c->numVertices - 2;
                v63 = active_6c->numVertices - 1;
                lighting_maybe_2 = 0;
                lighting_capability = 1;
                for (int pushed_tris = 0; pushed_tris <= v61; pushed_tris++)
                {
                    lighting_maybe = lighting_capability;
                    rdCache_aHWNormalTris[rdCache_totalNormalTris+pushed_tris].v3 = tri_vert_idx + lighting_maybe_2;
                    rdCache_aHWNormalTris[rdCache_totalNormalTris+pushed_tris].v2 = tri_vert_idx + lighting_maybe;
                    rdCache_aHWNormalTris[rdCache_totalNormalTris+pushed_tris].v1 = tri_vert_idx + v63;
                    rdCache_aHWNormalTris[rdCache_totalNormalTris+pushed_tris].flags = flags_idk_;
                    rdCache_aHWNormalTris[rdCache_totalNormalTris+pushed_tris].texture = tex2_arr_sel;
                    if ( (pushed_tris & 1) != 0 )
                    {
                        lighting_maybe_2 = v63--;
                    }
                    else
                    {
                        lighting_maybe_2 = lighting_maybe;
                        lighting_capability = lighting_maybe + 1;
                    }
                }
                rdCache_totalNormalTris += v61;
            }
            continue;
        }

solid_tri:
        if ( lighting_capability == 1 )
        {
            v78 = stdMath_Clamp(stdMath_Clamp(active_6c->extralight, 0.0, 1.0), v148, 1.0);
            active_6c->light_level_static = v78 * 63.0;
        }
        else if ( lighting_capability == 2 )
        {
            v75 = active_6c->extralight + active_6c->light_level_static;

            v76 = stdMath_Clamp(stdMath_Clamp(v75, 0.0, 1.0), v148, 1.0);
            active_6c->light_level_static = v76 * 63.0;
        }
        else if ( lighting_capability == 3 && active_6c->numVertices )
        {
            // MOTS added
#ifdef JKM_LIGHTING
            if (rdGetVertexColorMode() == 1) {
                flex_t* iterRedIntense = active_6c->paRedIntensities;
                flex_t* iterGreenIntense = active_6c->paGreenIntensities;
                flex_t* iterBlueIntense = active_6c->paBlueIntensities;

                v71 = active_6c->numVertices;
                do
                {
                    v73 = stdMath_Clamp(stdMath_Clamp(*iterRedIntense + active_6c->extralight, 0.0, 1.0), v148, 1.0);
                    *iterRedIntense = v73 * 63.0;

                    v73 = stdMath_Clamp(stdMath_Clamp(*iterGreenIntense + active_6c->extralight, 0.0, 1.0), v148, 1.0);
                    *iterGreenIntense = v73 * 63.0;

                    v73 = stdMath_Clamp(stdMath_Clamp(*iterBlueIntense + active_6c->extralight, 0.0, 1.0), v148, 1.0);
                    *iterBlueIntense = v73 * 63.0;

                    ++iterRedIntense;
                    ++iterGreenIntense;
                    ++iterBlueIntense;
                    --v71;
                }
                while ( v71 );
            }
            else
#endif
            {
                v70 = active_6c->vertexIntensities;
                v71 = active_6c->numVertices;
                do
                {
                    v72 = *v70 + active_6c->extralight;

                    v73 = stdMath_Clamp(stdMath_Clamp(v72, 0.0, 1.0), v148, 1.0);

                    *v70 = v73 * 63.0;
                    ++v70;
                    --v71;
                }
                while ( v71 );
            }
            goto LABEL_232;
        }
        else
        {
            goto LABEL_232;
        }

LABEL_232:
        tmpiter = 0;
        alpha_upshifta = red_and_alpha << 8;
        for (int vtx_idx = 0; vtx_idx < active_6c->numVertices; vtx_idx++)
        {
#ifndef TARGET_TWL
            rdCache_aHWVertices[rdCache_totalVerts].x = (active_6c->vertices[tmpiter].x);  // Added: The original game rounded to ints here (with ceilf?)
            rdCache_aHWVertices[rdCache_totalVerts].y = (active_6c->vertices[tmpiter].y);  // Added: The original game rounded to ints here (with ceilf?)
#endif
            v87 = active_6c->vertices[tmpiter].z;

            // DSi prefers z vertices directly
#ifdef TARGET_TWL
            v88 = 1.0 - active_6c->vertices[tmpiter].z;
            v89 = v88;
#else
            if ( v87 == 0.0 )
                v88 = 0.0;
            else
                v88 = 1.0 / active_6c->vertices[tmpiter].z;
            v89 = v88 * invZFar;
#endif
            if ( rdCache_dword_865258 != 16 )
                v89 = 1.0 - v89;
#ifdef TARGET_TWL
            rdCache_aHWVertices[rdCache_totalVerts].x = ((active_6c->vertices[tmpiter].x * res_fix_x) * v89);  // Added: The original game rounded to ints here (with ceilf?)
            rdCache_aHWVertices[rdCache_totalVerts].y = ((active_6c->vertices[tmpiter].y * res_fix_y) * v89);  // Added: The original game rounded to ints here (with ceilf?)
            v89 = stdMath_Clamp(v89, 0.0, SITHCAMERA_ZFAR);
#endif
            rdCache_aHWVertices[rdCache_totalVerts].z = v89;

            // Don't waste time with this on DSi
#ifdef TARGET_TWL
            rdCache_aHWVertices[rdCache_totalVerts].nx = 0.0;
#else
            rdCache_aHWVertices[rdCache_totalVerts].nx = v88 / 32.0;
#endif
            rdCache_aHWVertices[rdCache_totalVerts].nz = 0.0;

            // Added: nullptr check and fallback
            if (!(rdColormap *)active_6c->colormap || !v137) {
                v93 = 0xFF;
                v94 = 0xFF;
                v95 = 0xFF;
                v96 = 0xFF;
                red_and_alpha = v96;
                green = v94;
                blue = v95;
                v91 = (rdColormap *)active_6c->colormap;
                goto skip_colormap_deref;
            }

            if ( lighting_capability == 0) // Added: v137 nullptr check
            {
                v91 = (rdColormap *)active_6c->colormap;
                v97 = v137->header.field_4;
                v94 = (uint8_t)v91->colors[v97].g;
                v98 = (uint8_t)v91->colors[v97].b;
                v96 = (uint8_t)v91->colors[v97].r;
                red_and_alpha = v96;
                green = v94;
                blue = v98;
#ifdef SDL2_RENDER
                rdCache_aHWVertices[rdCache_totalVerts].lightLevel = 1.0;
#endif
            }
            else
            {
                // MOTS added
                if (rdGetVertexColorMode() != 1 || lighting_capability != 3)
                {
                    v91 = (rdColormap *)active_6c->colormap;
                    if ( lighting_capability == 3 )
                        v92 = active_6c->vertexIntensities[vtx_idx];
                    else
                        v92 = active_6c->light_level_static;
#ifdef SDL2_RENDER
                    rdCache_aHWVertices[rdCache_totalVerts].lightLevel = v92 / 255.0;
#endif
                    v93 = *((uint8_t *)v91->lightlevel + 256 * ((__int64)v92 & 0x3F) + v137->header.field_4);
                    v94 = (uint8_t)v91->colors[v93].g;
                    v95 = (uint8_t)v91->colors[v93].b;
                    v96 = (uint8_t)v91->colors[v93].r;
                    red_and_alpha = v96;
                    green = v94;
                    blue = v95;
                }
#ifdef JKM_LIGHTING
                else
                {
                    v91 = (rdColormap *)active_6c->colormap;
                    uint8_t baseLight = *((uint8_t *)v91->lightlevel + v137->header.field_4);
                    flex_d_t intRed = active_6c->paRedIntensities[vtx_idx] * 255.0 - 0.5;
                    flex_d_t intGreen = active_6c->paGreenIntensities[vtx_idx] * 255.0 - 0.5;
                    flex_d_t intBlue = active_6c->paBlueIntensities[vtx_idx] * 255.0 - 0.5;


                    intRed += (flex_t)v91->colors[baseLight].r; // FLEXTODO
                    intGreen += (flex_t)v91->colors[baseLight].r; // FLEXTODO
                    intBlue += (flex_t)v91->colors[baseLight].r; // FLEXTODO

                    v94 = (uint8_t)stdMath_Clamp(intGreen, 0.0, 255.0);
                    v95 = (uint8_t)stdMath_Clamp(intBlue, 0.0, 255.0);
                    v96 = (uint8_t)stdMath_Clamp(intRed, 0.0, 255.0);
                    red_and_alpha = v96;
                    green = v94;
                    blue = v95;
                }
#endif
            }
skip_colormap_deref:
            if ( v91 && v91 != rdColormap_pIdentityMap ) // Added: nullptr check
            {
                v96 = (uint8_t)(__int64)(v91->tint.x * (flex_d_t)red_and_alpha);
                red_and_alpha = v96;
                v100 = (__int64)(v91->tint.y * (flex_d_t)green);
                v101 = v91->tint.z * (flex_d_t)blue;
                v94 = (uint8_t)v100;
                green = (uint8_t)v100;
                blue = (uint8_t)(__int64)v101;
            }
            flags_idk_ |= 0x8000;
            if ( v129 )
            {
                if ( !(rdroid_curColorEffects.filter.x) )
                {
                    v96 = 0;
                    red_and_alpha = 0;
                }
                if ( !(rdroid_curColorEffects.filter.y) )
                {
                    v94 = 0;
                    green = 0;
                }
                if ( !(rdroid_curColorEffects.filter.z) )
                    blue = 0;
            }
            if ( v130 )
            {
                v96 += (__int64)((flex_d_t)red_and_alpha * red_scalar);
                red_and_alpha = v96;
                v94 += (__int64)((flex_d_t)green * green_scalar);
                green = v94;
                blue += (__int64)((flex_d_t)blue * blue_scalar);
            }
            if ( rdroid_curColorEffects.fade < 1.0 )
            {
                v96 = (__int64)((flex_d_t)red_and_alpha * rdroid_curColorEffects.fade);
                v94 = (__int64)((flex_d_t)green * rdroid_curColorEffects.fade);
                blue = (__int64)((flex_d_t)blue * rdroid_curColorEffects.fade);
            }
            if ( v96 < 0 )
            {
                v96 = (v96 & ~0xFF) | 0;
            }
            else if ( v96 > 255 )
            {
                v96 = (v96 & ~0xFF) | 0xff;
            }
            if ( v94 < 0 )
            {
                v94 = (v94 & ~0xFF) | 0;
            }
            else if ( v94 > 255 )
            {
                v94 = (v94 & ~0xFF) | 0xff;
            }
            v103 = blue;
            if ( blue < 0 )
            {
                v103 = 0;
            }
            else if ( blue > 255 )
            {
                v103 = -1;
            }
            v104 = v103 | (((uint8_t)v94 | ((alpha_upshifta | (uint8_t)v96) << 8)) << 8);
            rdCache_aHWVertices[rdCache_totalVerts].color = v104;
            rdCache_aHWVertices[rdCache_totalVerts].tu = 0.0;
            rdCache_aHWVertices[rdCache_totalVerts].tv = 0.0;
            rdCache_totalVerts++;
            tmpiter++;
        }
        v108 = active_6c->numVertices;
#ifdef QOL_IMPROVEMENTS
        if ( v108 <= 2 )
        {
            v117 = rdCache_totalLines;
            rdCache_aHWLines[v117].v2 = tri_vert_idx;
            rdCache_aHWLines[v117].v1 = tri_vert_idx + 1;
            rdCache_aHWLines[v117].flags = flags_idk_;
            
            rdCache_aHWVertices[rdCache_aHWLines[v117].v1].color = active_6c->extraData;
            rdCache_aHWVertices[rdCache_aHWLines[v117].v2].color = active_6c->extraData;
            
            rdCache_totalLines++;
        }
        else 
#endif
        if ( v108 <= 3 )
        {
            v117 = rdCache_totalSolidTris;
            rdCache_aHWSolidTris[v117].v3 = tri_vert_idx;
            rdCache_aHWSolidTris[v117].v2 = tri_vert_idx + 1;
            rdCache_aHWSolidTris[v117].v1 = tri_vert_idx + 2;
            rdCache_aHWSolidTris[v117].flags = flags_idk_;
            rdCache_aHWSolidTris[v117].texture = 0;
            rdCache_totalSolidTris++;
        }
        else
        {
            v109 = v108 - 2;
            v110 = 0;
            v111 = v108 - 1;
            lighting_capability = 1;
            for (v112 = 0; v112 < v109; v112++)
            {
                v115 = lighting_capability;
                rdCache_aHWSolidTris[rdCache_totalSolidTris+v112].v3 = tri_vert_idx + v110;
                rdCache_aHWSolidTris[rdCache_totalSolidTris+v112].v2 = tri_vert_idx + v115;
                rdCache_aHWSolidTris[rdCache_totalSolidTris+v112].v1 = tri_vert_idx + v111;
                rdCache_aHWSolidTris[rdCache_totalSolidTris+v112].flags = flags_idk_;
                rdCache_aHWSolidTris[rdCache_totalSolidTris+v112].texture = 0;
                if ( (v112 & 1) != 0 )
                {
                    v110 = v111--;
                }
                else
                {
                    v110 = v115;
                    lighting_capability = v115 + 1;
                }
            }
            rdCache_totalSolidTris += v109;
        }
    }

    rdCache_DrawRenderList();
    //rdCache_ResetRenderList(); // Added
    return 1;
}


#endif

void rdCache_ResetRenderList()
{
    std3D_ResetRenderList();
    rdCache_totalNormalTris = 0;
    rdCache_totalSolidTris = 0;
    rdCache_totalVerts = 0;
#ifdef QOL_IMPROVEMENTS
    rdCache_totalLines = 0;
#endif
}

void rdCache_DrawRenderList()
{
    if ( rdCache_totalVerts )
    {
        if ( !std3D_AddRenderListVertices(rdCache_aHWVertices, rdCache_totalVerts) )
        {
            std3D_DrawRenderList();
            std3D_AddRenderListVertices(rdCache_aHWVertices, rdCache_totalVerts);
        }
        std3D_RenderListVerticesFinish();
#ifndef TARGET_TWL
        if ( rdroid_curZBufferMethod == RD_ZBUFFER_READ_WRITE )
            _qsort(rdCache_aHWNormalTris, rdCache_totalNormalTris, sizeof(rdTri), rdCache_TriCompare);
#endif
        if ( rdCache_totalSolidTris )
            std3D_AddRenderListTris(rdCache_aHWSolidTris, rdCache_totalSolidTris);
        if ( rdCache_totalNormalTris )
            std3D_AddRenderListTris(rdCache_aHWNormalTris, rdCache_totalNormalTris);
#ifdef QOL_IMPROVEMENTS
#ifdef SDL2_RENDER
        if ( rdCache_totalLines )
            std3D_AddRenderListLines(rdCache_aHWLines, rdCache_totalLines);
#endif
#endif

        std3D_DrawRenderList();
    }
}

int rdCache_TriCompare(const void* a_, const void* b_)
{
    const rdTri* a = (const rdTri*)a_;
    const rdTri* b = (const rdTri*)b_;

    rdDDrawSurface *tex_b;
    rdDDrawSurface *tex_a;

    tex_b = b->texture;
    tex_a = a->texture;

    if ( tex_a->is_16bit == tex_b->is_16bit )
        return tex_a - tex_b;
    else
        return tex_a->is_16bit != 0 ? 1 : -1;
}

int rdCache_ProcFaceCompare(rdProcEntry *a, rdProcEntry *b)
{
    if ( a->z_min == b->z_min )
        return 0;

    if ( a->z_min >= b->z_min )
        return -1;

    return 1;
}

// MOTS altered
int rdCache_AddProcFace(int a1, unsigned int num_vertices, char flags)
{
    int v6; // edx
    size_t current_rend_6c_idx; // eax
    rdProcEntry *procFace; // esi
    int v9; // ecx
    rdVector3 *v10; // edx
    flex_d_t y_min_related; // ebx
    flex_d_t v12; // st7
    flex_d_t y_max_related; // [esp+Ch] [ebp-18h]
    flex_t v27; // [esp+10h] [ebp-14h]
    flex_t z_max; // [esp+14h] [ebp-10h]
    flex_t z_min; // [esp+18h] [ebp-Ch]
    flex_t y_max; // [esp+1Ch] [ebp-8h]
    flex_t y_min; // [esp+20h] [ebp-4h]
    flex_t x_min; // [esp+2Ch] [ebp+8h]
    flex_t extdataa; // [esp+2Ch] [ebp+8h]
    flex_t extdatab; // [esp+2Ch] [ebp+8h]
    flex_t extdatac; // [esp+2Ch] [ebp+8h]
    flex_t x_max; // [esp+30h] [ebp+Ch]

    if ( rdCache_numProcFaces >= RDCACHE_MAX_TRIS )
        return 0;
    v6 = rdroid_curProcFaceUserData;
    current_rend_6c_idx = rdCache_numProcFaces;
    x_min = 3.4e38;
    x_max = -3.4e38;
    y_min = 3.4e38;
    procFace = &rdCache_aProcFaces[rdCache_numProcFaces];
    y_max = -3.4e38;
    z_min = 3.4e38;
    z_max = -3.4e38;
    procFace->extraData = a1;
    v9 = 0;
    procFace->numVertices = num_vertices;
    procFace->vertexColorMode = v6;
    
    // Added (MOTS also added? lol)
    y_min_related = 3.4e38;
    y_max_related = 3.4e38;
    
    if ( num_vertices )
    {
        v10 = rdCache_aProcFaces[current_rend_6c_idx].vertices;

        do
        {
            v12 = v10->x;
            if ( v12 < x_min )
                x_min = v10->x;
            if ( v12 > x_max )
                x_max = v10->x;

            if ( v10->y < y_min )
            {
                y_min = v10->y;
                y_min_related = (flex_t)v9; // FLEXTODO
            }
            if ( v10->y > y_max )
            {
                y_max = v10->y;
                y_max_related = (flex_t)v9; // FLEXTODO
            }
            if ( v10->z < z_min )
                z_min = v10->z;
            if ( v10->z > z_max )
                z_max = v10->z;
            v9++; // There used to be some weird undefined behavior here with y_min_related
            ++v10;
        }
        while ( v9 < num_vertices );
    }

    procFace->x_min = (int32_t)stdMath_Ceil(x_min);
    procFace->x_max = (int32_t)stdMath_Ceil(x_max);
    procFace->y_min = (int32_t)stdMath_Ceil(y_min);
    procFace->y_max = (int32_t)stdMath_Ceil(y_max);
    procFace->z_min = z_min;
    procFace->z_max = z_max;
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
    if ( procFace->x_min >= (unsigned int)procFace->x_max )
        return 0;
    if ( procFace->y_min >= (unsigned int)procFace->y_max )
        return 0;
#endif
    procFace->y_min_related = (int)y_min_related;
    procFace->y_max_related = (int)y_max_related;
    if ( (flags & 1) != 0 )
        rdCache_numUsedVertices += num_vertices;
    if ( (flags & 2) != 0 )
        rdCache_numUsedTexVertices += num_vertices;
    if ( (flags & 4) != 0 )
        rdCache_numUsedIntensities += num_vertices;
    procFace->colormap = rdColormap_pCurMap;
    ++rdCache_numProcFaces;
    if ( procFace->x_min < (unsigned int)rdCache_ulcExtent.x )
        rdCache_ulcExtent.x = procFace->x_min;
    if ( procFace->x_max > (unsigned int)rdCache_lrcExtent.x )
        rdCache_lrcExtent.x = procFace->x_max;
    if ( procFace->y_min < (unsigned int)rdCache_ulcExtent.y )
        rdCache_ulcExtent.y = procFace->y_min;
    if ( procFace->y_max > (unsigned int)rdCache_lrcExtent.y )
        rdCache_lrcExtent.y = procFace->y_max;
    return 1;
}
