#include "rdPolyLine.h"

#include "Engine/rdroid.h"
#include "Engine/rdCamera.h"
#include "General/stdMath.h"
#include "General/stdString.h"
#include "Raster/rdCache.h"
#include "Engine/rdColormap.h"
#include "Primitives/rdPrimit3.h"
#include "Primitives/rdDebug.h"
#include <math.h>

static rdVector3 polylineVerts[32]; // idk the size on this
static rdVector3 rdPolyLine_FaceVerts[32];

rdPolyLine* rdPolyLine_New(char *polyline_fname, char *material_fname, char *material_fname2, flex_t length, flex_t base_rad, flex_t tip_rad, int lightmode, int texmode, int sortingmethod, flex_t extraLight)
{
    rdPolyLine* polyline;

    polyline = (rdPolyLine *)rdroid_pHS->alloc(sizeof(rdPolyLine));
    if (polyline)
    {
        memset(polyline, 0, sizeof(*polyline)); // Added: clear struct
        rdPolyLine_NewEntry(polyline, polyline_fname, material_fname, material_fname2, length, base_rad, tip_rad, lightmode, texmode, sortingmethod, extraLight);
    }
    return polyline;
}

int rdPolyLine_NewEntry(rdPolyLine *polyline, char *polyline_fname, char *material_side_fname, char *material_tip_fname, flex_t length, flex_t base_rad, flex_t tip_rad, rdGeoMode_t edgeGeometryMode, rdLightMode_t edgeLightingMode, rdTexMode_t edgeTextureMode, flex_t extraLight)
{

    rdMaterial *mat;
    int *vertexPosIdx;
    unsigned int numVertices;
    rdVector2 *extraUVTipMaybe;
    int *vertexUVIdx;
    rdVector2 *extraUVFaceMaybe;
    stdVBuffer *v22;

    // Added: memleak mitigation
    rdPolyLine_FreeEntry(polyline);

    if ( polyline_fname )
    {
        // TODO: caching?
#ifdef SITH_DEBUG_STRUCT_NAMES
        stdString_SafeStrCopy(polyline->fname, polyline_fname, 32);
#endif
    }
    polyline->length = length;
    polyline->baseRadius = base_rad;
    polyline->edgeFace.textureMode = edgeTextureMode;
    polyline->textureMode = edgeTextureMode;
    polyline->lightingMode = edgeLightingMode;
    polyline->tipRadius = tip_rad;
    polyline->edgeFace.type = 0;
    polyline->edgeFace.geometryMode = edgeGeometryMode;
    polyline->edgeFace.lightingMode = edgeLightingMode;
    polyline->geometryMode = edgeGeometryMode;
    polyline->edgeFace.extraLight = extraLight;

    polyline->edgeFace.material = rdMaterial_Load(material_side_fname, 0, 0);
    if ( !polyline->edgeFace.material )
        return 0;
    rdMaterial_EnsureDataForced(polyline->edgeFace.material); // Added: TWL
    polyline->edgeFace.numVertices = 4;
    vertexPosIdx = (int *)rdroid_pHS->alloc(sizeof(int) * polyline->edgeFace.numVertices);
    polyline->edgeFace.vertexPosIdx = vertexPosIdx;
    if ( !vertexPosIdx )
        return 0;
    numVertices = polyline->edgeFace.numVertices;
    for (int i = 0; i < numVertices; ++vertexPosIdx )
        *vertexPosIdx = i++;
    if ( polyline->edgeFace.geometryMode >= RD_GEOMODE_TEXTURED)
    {
        vertexUVIdx = (int *)rdroid_pHS->alloc(4 * numVertices);
        polyline->edgeFace.vertexUVIdx = vertexUVIdx;
        if ( !vertexUVIdx )
            return 0;
        for (int j = 0; j < polyline->edgeFace.numVertices; ++vertexUVIdx )
            *vertexUVIdx = j++;
        extraUVTipMaybe = (rdVector2 *)rdroid_pHS->alloc(sizeof(rdVector2) * polyline->edgeFace.numVertices);
        polyline->extraUVTipMaybe = extraUVTipMaybe;
        if ( !extraUVTipMaybe )
            return 0;
        // Odd quirk: This requires the material be actually loaded
        // Added: nullptr fallbacks
        v22 = NULL;
        if (polyline->edgeFace.material->texinfos[0] && polyline->edgeFace.material->texinfos[0]->texture_ptr) {
            v22 = polyline->edgeFace.material->texinfos[0]->texture_ptr->texture_struct[0];
        }
        extraUVTipMaybe[0].x = (flex_d_t)(v22 ? (unsigned int)v22->format.width : 1) - 0.01;// Added: nullptr check and fallback
        extraUVTipMaybe[0].y = 0.0;
        extraUVTipMaybe[1].x = 0.0;
        extraUVTipMaybe[1].y = 0.0;
        extraUVTipMaybe[2].x = 0.0;
        extraUVTipMaybe[2].y = (flex_d_t)(v22 ? (unsigned int)v22->format.height : 1) - 0.01;// Added: nullptr check and fallback
        extraUVTipMaybe[3].x = (flex_d_t)(v22 ? (unsigned int)v22->format.width : 1) - 0.01;// Added: nullptr check and fallback
        extraUVTipMaybe[3].y = (flex_d_t)(v22 ? (unsigned int)v22->format.height : 1) - 0.01;// Added: nullptr check and fallback
    }
    rdMaterial_OptionalFree(polyline->edgeFace.material); // Added: TWL
    polyline->tipFace.textureMode = edgeTextureMode;
    polyline->textureMode = edgeTextureMode;
    polyline->lightingMode = edgeLightingMode;
    polyline->tipFace.type = 0;
    polyline->tipFace.geometryMode = edgeGeometryMode;
    polyline->tipFace.lightingMode = edgeLightingMode;
    polyline->geometryMode = edgeGeometryMode;
    polyline->tipFace.extraLight = extraLight;
    polyline->tipFace.material = rdMaterial_Load(material_tip_fname, 0, 0);
    if ( !polyline->tipFace.material )
        return 0;
    rdMaterial_EnsureDataForced(polyline->tipFace.material); // Added: TWL
    polyline->tipFace.numVertices = 4;
    vertexPosIdx = (int *)rdroid_pHS->alloc(sizeof(int) * polyline->tipFace.numVertices);
    polyline->tipFace.vertexPosIdx = vertexPosIdx;
    if ( !vertexPosIdx )
        return 0;
    for (int k = 0; k < polyline->tipFace.numVertices; ++vertexPosIdx )
        *vertexPosIdx = k++;
    if ( polyline->tipFace.geometryMode >= RD_GEOMODE_TEXTURED)
    {
        vertexUVIdx = (int *)rdroid_pHS->alloc(sizeof(int) * polyline->tipFace.numVertices);
        polyline->tipFace.vertexUVIdx = vertexUVIdx;
        if ( !vertexUVIdx )
            return 0;
        for (int l = 0; l < polyline->tipFace.numVertices; ++vertexUVIdx )
            *vertexUVIdx = l++;
        extraUVFaceMaybe = (rdVector2 *)rdroid_pHS->alloc(sizeof(rdVector2) * polyline->tipFace.numVertices);
        polyline->extraUVFaceMaybe = extraUVFaceMaybe;
        if ( !extraUVFaceMaybe )
            return 0;
        // Odd quirk: This requires the material be actually loaded
        // Added: nullptr fallbacks
        v22 = NULL;
        if (polyline->tipFace.material->texinfos[0] && polyline->tipFace.material->texinfos[0]->texture_ptr) {
            v22 = polyline->tipFace.material->texinfos[0]->texture_ptr->texture_struct[0];
        }
        extraUVFaceMaybe[0].x = (flex_d_t)(v22 ? (unsigned int)v22->format.width : 1.0) - 0.01; // Added: nullptr check and fallback
        extraUVFaceMaybe[0].y = 0.0;
        extraUVFaceMaybe[1].x = 0.0;
        extraUVFaceMaybe[1].y = 0.0;
        extraUVFaceMaybe[2].x = 0.0;
        extraUVFaceMaybe[2].y = (flex_d_t)(v22 ? (unsigned int)v22->format.height : 1.0) - 0.01; // Added: nullptr check and fallback
        extraUVFaceMaybe[3].x = (flex_d_t)(v22 ? (unsigned int)v22->format.width : 1.0) - 0.01; // Added: nullptr check and fallback
        extraUVFaceMaybe[3].y = (flex_d_t)(v22 ? (unsigned int)v22->format.height : 1.0) - 0.01; // Added: nullptr check and fallback
    }
    rdMaterial_OptionalFree(polyline->tipFace.material); // Added: TWL
    return 1;
}

void rdPolyLine_Free(rdPolyLine *polyline)
{
    if ( polyline )
    {
        rdPolyLine_FreeEntry(polyline);
        rdroid_pHS->free(polyline);
    }
}

void rdPolyLine_FreeEntry(rdPolyLine *polyline)
{
    if ( polyline->extraUVFaceMaybe )
    {
        rdroid_pHS->free(polyline->extraUVFaceMaybe);
        polyline->extraUVFaceMaybe = 0;
    }
    if ( polyline->extraUVTipMaybe )
    {
        rdroid_pHS->free(polyline->extraUVTipMaybe);
        polyline->extraUVTipMaybe = 0;
    }
    if ( polyline->tipFace.vertexPosIdx )
    {
        rdroid_pHS->free(polyline->tipFace.vertexPosIdx);
        polyline->tipFace.vertexPosIdx = 0;
    }
    if ( polyline->tipFace.vertexUVIdx )
    {
        rdroid_pHS->free(polyline->tipFace.vertexUVIdx);
        polyline->tipFace.vertexUVIdx = 0;
    }
    if ( polyline->edgeFace.vertexPosIdx )
    {
        rdroid_pHS->free(polyline->edgeFace.vertexPosIdx);
        polyline->edgeFace.vertexPosIdx = 0;
    }
    if ( polyline->edgeFace.vertexUVIdx )
    {
        rdroid_pHS->free(polyline->edgeFace.vertexUVIdx);
        polyline->edgeFace.vertexUVIdx = 0;
    }
}

int rdPolyLine_Draw(rdThing *thing, rdMatrix34 *matrix)
{
    rdPolyLine *polyline;
    flex_t length;
    flex_d_t tip_left;
    flex_d_t tip_bottom;
    flex_d_t tip_right;
    flex_d_t tip_top;
    flex_t ang;
    flex_t angSin;
    flex_t angCos;
    rdVector3 vertex_out;
    rdMatrix34 out;
    rdVector3 vertex;
    rdMeshinfo idxInfo;

    polyline = thing->polyline;
    
    // This is slightly different than IDA?
    idxInfo.numVertices = 4;
    idxInfo.vertices = polylineVerts;
    idxInfo.paDynamicLight = 0;
    idxInfo.intensities = 0;

    rdMatrix_Multiply34(&out, &rdCamera_pCurCamera->view_matrix, matrix);
    vertex.x = 0.0;
    vertex.y = polyline->length;
    vertex.z = 0.0;
    rdMatrix_TransformPoint34(&vertex_out, &vertex, &out);
    tip_left = vertex_out.x - polyline->tipRadius;
    tip_bottom = vertex_out.z - polyline->tipRadius;
    tip_right = vertex_out.x + polyline->tipRadius;
    tip_top = vertex_out.z + polyline->tipRadius;

    flex_t epislon = -0.001;

    // Tip
    {
        polylineVerts[0].x = tip_left;
        polylineVerts[0].y = vertex_out.y - epislon;
        polylineVerts[0].z = tip_bottom;
        polylineVerts[1].x = tip_right;
        polylineVerts[1].y = vertex_out.y - epislon;
        polylineVerts[1].z = tip_bottom;
        polylineVerts[2].x = tip_right;
        polylineVerts[2].y = vertex_out.y - epislon;
        polylineVerts[2].z = tip_top;
        polylineVerts[3].x = tip_left;
        polylineVerts[3].y = vertex_out.y - epislon;
        polylineVerts[3].z = tip_top;
        idxInfo.vertexUVs = polyline->extraUVFaceMaybe;
        rdPolyLine_DrawFace(thing, &polyline->tipFace, polylineVerts, &idxInfo);
    }

    // Base
    {
        polylineVerts[0].x = out.scale.x - polyline->baseRadius;
        polylineVerts[0].y = out.scale.y - epislon;
        polylineVerts[0].z = out.scale.z - polyline->baseRadius;
        polylineVerts[1].x = out.scale.x + polyline->baseRadius;
        polylineVerts[1].y = out.scale.y - epislon;
        polylineVerts[1].z = out.scale.z - polyline->baseRadius;
        polylineVerts[2].x = out.scale.x + polyline->baseRadius;
        polylineVerts[2].y = out.scale.y - epislon;
        polylineVerts[2].z = out.scale.z + polyline->baseRadius;
        polylineVerts[3].x = out.scale.x - polyline->baseRadius;
        polylineVerts[3].y = out.scale.y - epislon;
        polylineVerts[3].z = out.scale.z + polyline->baseRadius;
        idxInfo.vertexUVs = polyline->extraUVFaceMaybe;
        rdPolyLine_DrawFace(thing, &polyline->tipFace, polylineVerts, &idxInfo);
    }
    

    // Blade
    {
        flex_t zdist = vertex_out.z - out.scale.z;
        flex_t xdist = vertex_out.x - out.scale.x;
        flex_t mag = stdMath_Sqrt(xdist * xdist + zdist * zdist);

        // Added: prevent div 0
        if (mag == 0)
        {
            mag = 0.000001f;
        }

        ang = stdMath_ArcSin3((-xdist) / mag);
        if ( zdist < 0.0 )
        {
            if ( xdist <= 0.0 )
                ang = -(ang - -180.0);
            else
                ang = 180.0 - ang;
        }
        stdMath_SinCos(ang, &angSin, &angCos);
        polylineVerts[0].x = (polyline->tipRadius * angCos) - (mag * angSin) + out.scale.x;
        polylineVerts[0].y = vertex_out.y;
        polylineVerts[0].z = (polyline->tipRadius * angSin) + (mag * angCos) + out.scale.z;
        
        polylineVerts[1].x = (-polyline->tipRadius * angCos) - (mag * angSin) + out.scale.x;
        polylineVerts[1].y = vertex_out.y;
        polylineVerts[1].z = (-polyline->tipRadius * angSin) + (mag * angCos) + out.scale.z;
        
        polylineVerts[2].x = (-polyline->baseRadius * angCos) - (flex_t)0.0 + out.scale.x;
        polylineVerts[2].y = out.scale.y;
        polylineVerts[2].z = (-polyline->baseRadius * angSin) + (flex_t)0.0 + out.scale.z;
        
        polylineVerts[3].x = (polyline->baseRadius * angCos) - (flex_t)0.0 + out.scale.x;
        polylineVerts[3].y = out.scale.y;
        polylineVerts[3].z = (polyline->baseRadius * angSin) + (flex_t)0.0 + out.scale.z;
        idxInfo.vertexUVs = polyline->extraUVTipMaybe;
        rdPolyLine_DrawFace(thing, &polyline->edgeFace, polylineVerts, &idxInfo);
    }
    return 1;
}

void rdPolyLine_DrawFace(rdThing *thing, rdFace *face, rdVector3 *unused, rdMeshinfo *idxInfo)
{
    rdProcEntry *procEntry;
    rdMeshinfo mesh_out;
    flex_t staticLight;

    procEntry = rdCache_GetProcEntry();
    if (!procEntry)
        return;

    mesh_out.vertices = rdPolyLine_FaceVerts;
    mesh_out.verticesOrig = procEntry->vertices;
    mesh_out.vertexUVs = procEntry->vertexUVs;
    mesh_out.paDynamicLight = procEntry->vertexIntensities;
    
    idxInfo->numVertices = face->numVertices;
    idxInfo->vertexPosIdx = face->vertexPosIdx;
    idxInfo->vertexUVIdx = face->vertexUVIdx;
    
    rdGeoMode_t curGeometryMode_ = rdroid_curGeometryMode;
    rdLightMode_t curLightingMode_ = rdroid_curLightingMode;
    rdTexMode_t curTextureMode_ = rdroid_curTextureMode;

    if ( curGeometryMode_ >= face->geometryMode )
        curGeometryMode_ = face->geometryMode;
    if ( curGeometryMode_ >= thing->curGeoMode )
    {
        procEntry->geometryMode = thing->curGeoMode;
    }    
    else if ( rdroid_curGeometryMode >= face->geometryMode )
    {
        procEntry->geometryMode = face->geometryMode;
    }
    else
    {
        procEntry->geometryMode = rdroid_curGeometryMode;
    }
    
    procEntry->geometryMode = procEntry->geometryMode;
    if ( rdroid_curRenderOptions & 2 && rdCamera_pCurCamera->ambientLight >= 1.0 )
    {
        procEntry->lightingMode = RD_LIGHTMODE_FULLYLIT;
    }
    else
    {
        if ( curLightingMode_ >= face->lightingMode )
            curLightingMode_ = face->lightingMode;
        if ( curLightingMode_ >= thing->curLightMode )
        {
            face->lightingMode = thing->curLightMode;
        }
        else if ( rdroid_curLightingMode < face->lightingMode )
        {
            face->lightingMode = rdroid_curLightingMode;
        }
        procEntry->lightingMode = face->lightingMode;
    }

    if ( curTextureMode_ >= face->textureMode )
        curTextureMode_ = face->textureMode;
    
    procEntry->textureMode = thing->curTexMode;
    if ( curTextureMode_ < procEntry->textureMode )
    {
        if ( curTextureMode_ >= face->textureMode )
            procEntry->textureMode = face->textureMode;
        else
            procEntry->textureMode = rdroid_curTextureMode;
    }

    rdPrimit3_ClipFace(rdCamera_pCurCamera->pClipFrustum, procEntry->geometryMode, procEntry->lightingMode, procEntry->textureMode, idxInfo, &mesh_out, &face->clipIdk);
    if ( mesh_out.numVertices < 3 )
        return;

    rdCamera_pCurCamera->fnProjectLst(mesh_out.verticesOrig, mesh_out.vertices, mesh_out.numVertices);

    if ( rdroid_curRenderOptions & 2 )
        procEntry->ambientLight = rdCamera_pCurCamera->ambientLight;
    else
        procEntry->ambientLight = 0.0;

    if ( procEntry->lightingMode )
    {
        if ( procEntry->ambientLight < 1.0 )
        {
            if ( procEntry->lightingMode == 2 )
            {
                if ( procEntry->light_level_static < 1.0 || rdColormap_pCurMap != rdColormap_pIdentityMap )
                {
                    if ( procEntry->light_level_static <= 0.0 )
                        procEntry->lightingMode = 1;
                }
                else
                {
                    procEntry->lightingMode = 0;
                }
            }
            else if ( procEntry->lightingMode == 3 )
            {
                int i;
                staticLight = *procEntry->vertexIntensities;
                for (i = 1; i < mesh_out.numVertices; i++)
                {
                    if ( procEntry->vertexIntensities[i] != staticLight )
                        break;
                }
                if ( i == mesh_out.numVertices )
                {
                    if ( staticLight == 1.0 )
                    {
                        if ( rdColormap_pCurMap == rdColormap_pIdentityMap )
                        {
                            procEntry->lightingMode = 0;
                        }
                        else
                        {
                            procEntry->lightingMode = 2;
                            procEntry->light_level_static = 1.0;
                        }
                    }
                    else if ( staticLight == 0.0 )
                    {
                        procEntry->lightingMode = 1;
                        procEntry->light_level_static = 0.0;
                    }
                    else
                    {
                        procEntry->lightingMode = 2;
                        procEntry->light_level_static = staticLight;
                    }
                }
            }
        }
        else if ( rdColormap_pCurMap == rdColormap_pIdentityMap )
        {
            procEntry->lightingMode = 0;
        }
        else
        {
            procEntry->lightingMode = 2;
            procEntry->light_level_static = 1.0;
        }
    }
    
    int procFaceFlags = 1;
    if ( procEntry->geometryMode >= 4 )
        procFaceFlags = 3;
    if ( procEntry->lightingMode >= 3 )
        procFaceFlags |= 4u;

    procEntry->light_flags = 0;
    procEntry->wallCel = thing->wallCel;
    procEntry->type = face->type;
    procEntry->extralight = face->extraLight;
    procEntry->material = face->material;
    rdCache_AddProcFace(0, mesh_out.numVertices, procFaceFlags);
}
