#include "rdParticle.h"

#include "Engine/rdroid.h"
#include "stdPlatform.h"
#include "General/stdConffile.h"
#include "General/stdString.h"
#include "jk.h"
#include "Raster/rdCache.h"
#include "Engine/rdClip.h"
#include "Win95/std.h"

static rdVector3 aParticleVerticesTmp[32];
static rdVector3 aParticleVertices[256];
static rdParticleLoader_t rdParticle_loader;

void rdParticle_RegisterLoader(rdParticleLoader_t loader)
{
    rdParticle_loader = loader;
}

rdParticle* rdParticle_New(int numVertices, flex_t size, rdMaterial *material, int lightingMode, int allocateVertices)
{
    rdParticle *particle;

    particle = (rdParticle*)rdroid_pHS->alloc(sizeof(rdParticle));
    if (particle)
        rdParticle_NewEntry(particle, numVertices, size, material, lightingMode, allocateVertices);

    return particle;
}

int rdParticle_NewEntry(rdParticle *particle, int numVertices, flex_t size, rdMaterial *material, int lightingMode, int allocateVertices)
{
    particle->material = material;
    particle->diameter = size;
    particle->radius = size * 0.5;
    particle->numVertices = numVertices;
    particle->lightingMode = lightingMode;
    particle->cloudRadius = 0;
    particle->hasVertices = allocateVertices;

    if (allocateVertices)
    {
        particle->vertices = (rdVector3 *)rdroid_pHS->alloc(sizeof(rdVector3) * numVertices);
        particle->vertexCel = (int *)rdroid_pHS->alloc(sizeof(int) * particle->numVertices);
        if (particle->vertices && particle->vertexCel)
        {
            _memset(particle->vertices, 0, sizeof(rdVector3) * particle->numVertices);
            _memset(particle->vertexCel, 0xFF, sizeof(int) * particle->numVertices);
            return 1;
        }
        
        return 0;
    }
    else
    {
        particle->vertices = 0;
        particle->vertexCel = 0;
        return 1;
    }

    return 0;
}

rdParticle* rdParticle_Clone(rdParticle *particle)
{
    rdParticle *clonedPart; // eax

    clonedPart = (rdParticle*)rdroid_pHS->alloc(sizeof(rdParticle));
    if (clonedPart)
    {
        rdParticle_NewEntry(clonedPart, particle->numVertices, particle->diameter, particle->material, particle->lightingMode, 1);
        _memcpy(clonedPart->vertices, particle->vertices, sizeof(rdVector3) * particle->numVertices);
        _memcpy(clonedPart->vertexCel, particle->vertexCel, sizeof(int) * particle->numVertices);
    }

    return clonedPart;
}

void rdParticle_Free(rdParticle *particle)
{
    if (!particle)
        return;

    rdParticle_FreeEntry(particle);
    
    rdroid_pHS->free(particle);
}

void rdParticle_FreeEntry(rdParticle *particle)
{
    if (particle->hasVertices)
    {
        if (!particle->vertices)
            return;
        rdroid_pHS->free(particle->vertices);
        rdroid_pHS->free(particle->vertexCel);
    }
    particle->vertices = NULL;
    particle->vertexCel = NULL;
}

rdParticle* rdParticle_Load(char *path)
{
    rdParticle *particle;

    if (rdParticle_loader)
        return (rdParticle*)rdParticle_loader(path);

    particle = (rdParticle*)rdroid_pHS->alloc(sizeof(rdParticle));
    if (!particle)
        return NULL;

    if (rdParticle_LoadEntry(path, particle))
        return particle;

    rdParticle_Free(particle);
    return NULL;
}

int rdParticle_LoadEntry(char *fpath, rdParticle *pParticle)
{
    rdParticle *v4; // esi
    int v5; // ebx
    rdMaterial *v7; // eax
    int v8; // ebp
    rdVector3 *v13; // eax
    int v15; // eax
    rdVector3 *v16; // esi
    int *v17; // edi
    flex32_t fx;
    flex32_t fy;
    flex32_t fz;
    flex32_t size; // [esp+1Ch] [ebp-14h]
    flex32_t v21; // [esp+20h] [ebp-10h]
    int versMinor; // [esp+24h] [ebp-Ch]
    int versMajor; // [esp+28h] [ebp-8h]
    int v24; // [esp+2Ch] [ebp-4h]

    stdString_SafeStrCopy(pParticle->name, stdFileFromPath(fpath), 0x20);
    v5 = 0;
    pParticle->hasVertices = 1;
    if (!stdConffile_OpenRead(fpath))
        goto done;
    if (!stdConffile_ReadLine())
        goto done_close;
        
    if ( _sscanf(stdConffile_aLine, " section: %s", std_genBuffer) != 1 )
        goto done_close;
    
    if (!stdConffile_ReadLine())
        goto done_close;
        
    _sscanf(stdConffile_aLine, " par %d.%d", &versMajor, &versMinor);
    if (!stdConffile_ReadLine())
        goto done_close;

    if (_sscanf(stdConffile_aLine, " size %f", &size) != 1)
        goto done_close;

    pParticle->diameter = size; // FLEXTODO
    pParticle->radius = size * 0.5; // FLEXTODO
    if (!stdConffile_ReadLine())
        goto done_close;

    if ( _sscanf(stdConffile_aLine, " material %s", std_genBuffer) != 1 )
        goto done_close;
        
    v7 = rdMaterial_Load(std_genBuffer, 0, 0);
    pParticle->material = v7;
    if (!v7)
        goto done_close;

    v8 = v7->num_texinfo;
    if (!stdConffile_ReadLine())
        goto done_close;

    if ( _sscanf(stdConffile_aLine, " lightingmode %d", &pParticle->lightingMode) != 1 )
        goto done_close;

    if (!stdConffile_ReadLine())
        goto done_close;

    if ( _sscanf(stdConffile_aLine, " section: %s", std_genBuffer) != 1 )
        goto done_close;

    if (!stdConffile_ReadLine() )
        goto done_close;

    if ( _sscanf(stdConffile_aLine, " radius %f", &v21) != 1 )
        goto done_close;

    pParticle->cloudRadius = v21; // FLEXTODO
    if (!stdConffile_ReadLine())
        goto done_close;

    if ( _sscanf(stdConffile_aLine, " insert offset %f %f %f", &fx, &fy, &fz) != 3 )
        goto done_close;

    pParticle->insertOffset.x = fx; // FLEXTODO
    pParticle->insertOffset.y = fy; // FLEXTODO
    pParticle->insertOffset.z = fz; // FLEXTODO
    if (!stdConffile_ReadLine())
        goto done_close;

    uint32_t numVertices;
    if ( _sscanf(stdConffile_aLine, " vertices %d", &numVertices) == 1
      && numVertices <= 0x100 )
    {
        pParticle->numVertices = numVertices;
        v13 = (rdVector3 *)rdroid_pHS->alloc(sizeof(rdVector3) * numVertices);
        pParticle->vertices = v13;
        pParticle->vertexCel = (int*)rdroid_pHS->alloc(sizeof(int) * numVertices);
        v16 = pParticle->vertices;
        if ( v16 && pParticle->vertexCel)
        {
            v17 = pParticle->vertexCel;
            if ( numVertices <= 0 )
            {
LABEL_28:
                stdConffile_Close();
                return 1;
            }
            while ( stdConffile_ReadLine()
                 && _sscanf(
                        stdConffile_aLine,
                        " %d: %f %f %f %d",
                        &v24,
                        &fx,
                        &fy,
                        &fz,
                        v17) == 5
                 && *v17 < v8 )
            {
                ++v17;
                v16->x = fx; // FLEXTODO
                v16->y = fy; // FLEXTODO
                v16->z = fz; // FLEXTODO
                ++v16;
                if ( ++v5 >= numVertices )
                    goto LABEL_28;
            }
        }
    }

done_close:
    stdConffile_Close();
done:
    return 0;
}

int rdParticle_Write(char *writePath, rdParticle *particle, char *madeBy)
{
    int v3; // ebx
    unsigned int v4; // edi
    int v6; // [esp+28h] [ebp-4h]

    v3 = rdroid_pHS->fileOpen(writePath, "wt+");
    v4 = 0;
    if ( !v3 )
        return 0;
    rdroid_pHS->filePrintf(v3, "# PAR '%s' created from '%s'\n\n", particle->name, madeBy);
    rdroid_pHS->filePrintf(v3, "###############\n");
    rdroid_pHS->filePrintf(v3, "SECTION: HEADER\n\n");
    rdroid_pHS->filePrintf(v3, "PAR %d.%d\n\n", 1, 0);
    rdroid_pHS->filePrintf(v3, "SIZE %.6f\n\n", particle->diameter);
    rdroid_pHS->filePrintf(v3, "MATERIAL %s\n\n", particle->material->mat_fpath);
    rdroid_pHS->filePrintf(v3, "LIGHTINGMODE %d\n\n", particle->lightingMode);
    rdroid_pHS->filePrintf(v3, "###############\n");
    rdroid_pHS->filePrintf(v3, "SECTION: GEOMETRYDEF\n\n");
    rdroid_pHS->filePrintf(v3, "# Object radius\n");
    rdroid_pHS->filePrintf(v3, "RADIUS %10.6f\n\n", particle->cloudRadius);
    rdroid_pHS->filePrintf(v3, "# Insertion offset\n");
    rdroid_pHS->filePrintf(v3, "INSERT OFFSET %10.6f %10.6f %10.6f\n\n", particle->insertOffset.x, particle->insertOffset.y, particle->insertOffset.z);
    rdroid_pHS->filePrintf(v3, "VERTICES %d\n\n", particle->numVertices);
    rdroid_pHS->filePrintf(v3, "# num:     x:         y:         z:       cel:\n");
    if ( particle->numVertices > 0u )
    {
        v6 = 0;
        do
        {
            rdroid_pHS->filePrintf(
                v3,
                "  %3d: %10.6f %10.6f %10.6f %d\n",
                v4,
                particle->vertices[v6].x,
                particle->vertices[v6].y,
                particle->vertices[v6].z,
                particle->vertexCel[v4]);
            ++v4;
            ++v6;
        }
        while ( v4 < particle->numVertices );
    }
    rdroid_pHS->filePrintf(v3, "\n\n");
    rdroid_pHS->fileClose(v3);
    return 1;
}

int rdParticle_Draw(rdThing *thing, rdMatrix34 *matrix_4_3)
{
    rdParticle *particle; // edi
    int v3; // eax
    flex_t *v4; // ebx
    rdProcEntry *v5; // esi
    flex_d_t v6; // st6
    flex_d_t v7; // st5
    flex_d_t v8; // st7
    flex_d_t v9; // st4
    flex_d_t v10; // st3
    flex_d_t v11; // st2
    flex_d_t v12; // st1
    flex_d_t v13; // st3
    flex_d_t v14; // rt1
    flex_t v15; // ST24_4
    flex_d_t v16; // st2
    flex_d_t v17; // st7
    flex_t v18; // edx
    flex_t v19; // ST24_4
    flex_d_t v20; // st5
    flex_d_t v21; // st2
    flex_d_t v22; // st7
    rdClipFrustum *v23; // ecx
    flex_t v24; // eax
    flex_t v25; // ST24_4
    unsigned int v26; // eax
    unsigned int v27; // ebp
    int *v29; // ecx
    int v30; // ecx
    int v32; // [esp+10h] [ebp-44h]
    rdVector3 vertex_out; // [esp+18h] [ebp-3Ch]
    rdMatrix34 out; // [esp+24h] [ebp-30h]
    int v35; // [esp+58h] [ebp+4h]
    flex_t matrix_4_3a; // [esp+5Ch] [ebp+8h]

    particle = thing->particlecloud;
    rdMatrix_TransformPoint34(&vertex_out, &matrix_4_3->scale, &rdCamera_pCurCamera->view_matrix);
    if ( rdroid_curCullFlags & 2 )
        v3 = rdClip_SphereInFrustum(rdCamera_pCurCamera->pClipFrustum, &vertex_out, particle->cloudRadius);
    else
        v3 = thing->clippingIdk;
    if ( v3 != SPHERE_FULLY_OUTSIDE )
    {
        rdMatrix_Multiply34(&out, &rdCamera_pCurCamera->view_matrix, matrix_4_3);
        if ( rdroid_curRenderOptions & 2 )
            matrix_4_3a = rdCamera_pCurCamera->ambientLight;
        else
            matrix_4_3a = 0.0;
        if ( matrix_4_3a < 1.0 )
        {
            if ( matrix_4_3a > 0.0 )
                v35 = particle->lightingMode;
            else
                v35 = 1;
        }
        else
        {
            v35 = 0;
        }
        if ( v35 >= particle->lightingMode )
            v35 = particle->lightingMode;
        rdMatrix_TransformPointLst34(&out, particle->vertices, &aParticleVertices[0], particle->numVertices);
        v32 = 0;
        if ( !particle->numVertices )
            return 1;
        v4 = &aParticleVertices[0].y;
        while ( 1 )
        {
            v5 = rdCache_GetProcEntry();
            if ( !v5 )
                break;
            v6 = *(v4 - 1) - particle->radius;
            v7 = v4[1] - particle->radius;
            v8 = *(v4 - 1) + particle->radius;
            v9 = v4[1] + particle->radius;
            v10 = *v4;
            v11 = *v4;
            aParticleVerticesTmp[0].x = v6;
            v12 = v10;
            v13 = *v4;
            aParticleVerticesTmp[0].y = v12;
            v14 = v11;
            v15 = v7;
            v16 = v8;
            v17 = v15;
            v18 = v15;
            v19 = v16;
            v20 = v19;
            aParticleVerticesTmp[1].y = v14;
            v21 = v17;
            v22 = *v4;
            aParticleVerticesTmp[1].z = v21;
            v23 = rdCamera_pCurCamera->pClipFrustum;
            v24 = v19;
            v25 = v9;
            aParticleVerticesTmp[2].x = v20;
            aParticleVerticesTmp[2].y = v13;
            aParticleVerticesTmp[2].z = v9;
            aParticleVerticesTmp[3].x = v6;
            aParticleVerticesTmp[3].y = v22;
            aParticleVerticesTmp[0].z = v18;
            aParticleVerticesTmp[1].x = v24;
            aParticleVerticesTmp[3].z = v25; 
            v26 = rdClip_Face3S(v23, aParticleVerticesTmp, 4);
            v27 = v26;
            if ( v26 >= 3 )
            {
                rdCamera_pCurCamera->fnProjectLst(v5->vertices, aParticleVerticesTmp, v26);
                v5->lightingMode = v35;
                v29 = particle->vertexCel;
                v5->material = particle->material;

                // Added: Particles should always be drawn
                rdMaterial_EnsureDataForced(v5->material);

                v5->ambientLight = matrix_4_3a;
                v30 = v29[v32];
                v5->geometryMode = 3;
                v5->type = 0;
                v5->wallCel = v30;
                v5->light_flags = 0;
                rdCache_AddProcFace(0, v27, 1);
            }
            v4 += 3;
            if ( (unsigned int)++v32 >= particle->numVertices )
                return 1;
        }
    }
    return 0;
}
