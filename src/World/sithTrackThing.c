#include "sithTrackThing.h"

#include "stdPlatform.h"
#include "General/stdConffile.h"
#include "General/stdMath.h"
#include "World/sithSoundClass.h"
#include "Engine/sithCollision.h"
#include "World/jkPlayer.h"
#include "Cog/sithCog.h"
#include "jk.h"

void sithTrackThing_MoveToFrame(sithThing *thing, int goalFrame, flex_t a3)
{
    if ( goalFrame < thing->trackParams.loadedFrames )
    {
        thing->trackParams.flags |= 4u;
        thing->trackParams.lerpSpeed = a3;
        thing->goalframe = goalFrame;
        sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_STARTMOVE);
        sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_MOVING);
        sithTrackThing_Arrivedidk(thing);
    }
}

void sithTrackThing_Arrivedidk(sithThing *thing)
{
    sithThingFrame *v4; // edx
    flex_d_t v8; // st7
    flex_d_t v9; // st6
    sithThingFrame *v12; // eax
    flex_d_t v15; // st7
    flex_d_t v16; // st6
    flex_t thinga; // [esp+10h] [ebp+4h]

    int goalFrame = thing->goalframe;
    if ( thing->curframe == goalFrame )
    {
        v4 = thing->trackParams.aFrames;
        rdVector_Sub3(&thing->trackParams.vel, &v4[thing->curframe].pos, &thing->position);
        v8 = rdVector_Normalize3Acc(&thing->trackParams.vel);
        if ( v8 != 0.0 )
        {
            v9 = v8 / thing->trackParams.lerpSpeed;
            thing->field_250 = 0;
            thing->trackParams.flags |= 1;
            thing->trackParams.field_1C = v9;
        }
        thinga = thing->trackParams.field_1C;
        if ( thing->trackParams.field_1C == 0.0 )
        {
            thing->trackParams.flags &= ~0x17;
            thing->trackParams.lerpSpeed = 0.0;
            thing->goalframe = 0;
            thing->field_258 = 0;
            thing->field_250 = 0;
            sithSoundClass_ThingPauseSoundclass(thing, SITH_SC_MOVING);
            sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_STOPMOVE);
            if ( (thing->thingflags & SITH_TF_CAPTURED) != 0 && (thing->thingflags & SITH_TF_INVULN) == 0 )
                sithCog_SendMessageFromThing(thing, 0, SITH_MESSAGE_ARRIVED);
            return;
        }
        goalFrame = thing->goalframe;
    }
    else
    {
        if ( thing->curframe >= 0 )
            goalFrame = thing->curframe + (goalFrame - thing->curframe >= 0 ? 1 : -1);

        // Added
        //if (goalFrame < 0)
        //    goalFrame = 0;
        //if (goalFrame >= thing->trackParams.sizeFrames)
        //    goalFrame = 0;//thing->trackParams.sizeFrames - 1;

        v12 = thing->trackParams.aFrames;
        thing->field_258 = goalFrame;
        rdVector_Sub3(&thing->trackParams.vel, &v12[goalFrame].pos, &thing->position);
        v15 = rdVector_Normalize3Acc(&thing->trackParams.vel);
        if ( v15 != 0.0 )
        {
            v16 = v15 / thing->trackParams.lerpSpeed;
            thing->field_250 = 0;
            thing->trackParams.flags |= 1;
            thing->trackParams.field_1C = v16;
        }
        thinga = thing->trackParams.field_1C;
    }
    if ( thinga < 1.0 )
        thinga = 1.0;
    sithTrackThing_PrepareForOrient(thing, &thing->trackParams.aFrames[goalFrame].rot, thinga);
}

void sithTrackThing_Tick(sithThing *thing, flex_t deltaSeconds)
{
    flex_d_t v4; // st7
    flex_d_t v5; // st7
    flex_d_t v18; // st7
    flex_d_t v22; // st7
    flex_d_t v26; // st7
    flex_d_t v30; // st7
    flex_d_t v31; // st7
    flex_t a6; // [esp+0h] [ebp-50h]
    flex_t v41; // [esp+4h] [ebp-4Ch]
    flex_t v42; // [esp+4h] [ebp-4Ch]
    rdVector3 rotVec; // [esp+8h] [ebp-48h] BYREF
    rdVector3 a1a; // [esp+14h] [ebp-3Ch] BYREF
    rdMatrix34 rotMat; // [esp+20h] [ebp-30h] BYREF
    flex_t a3; // [esp+54h] [ebp+4h]
    flex_t a3a; // [esp+54h] [ebp+4h]
    flex_t deltaSecondsa; // [esp+58h] [ebp+8h]

    if ( deltaSeconds == 0.0 )
    {
        return;
    }
    if (!thing->trackParams.flags)
    {
        return;
    }
    if (thing->trackParams.flags & 0x80u)
    {
        return;
    }
    if ( (thing->trackParams.flags & 2) != 0 )
    {
        v4 = thing->trackParams.field_54 * deltaSeconds;
        v41 = v4;
        v5 = v4 + thing->field_24C;
        a3 = v5;
        if ( v5 > 1.0 )
        {
            a3 = 1.0;
            v41 = 1.0 - thing->field_24C;
        }
        rdVector_Scale3(&rotVec, &thing->trackParams.moveFrameDeltaAngles, a3);
        rdMatrix_Copy34(&rotMat, &thing->trackParams.moveFrameOrientation);

        // MoTS added: MoveToFrame was kinda just broken in JK?
        // MoTS uses absolute rotations, but maybe JK used deltas?
        if (Main_bMotsCompat) {
            sithThingFrame* pFrame = &thing->trackParams.aFrames[thing->curframe];
            rdVector_Add3Acc(&rotVec, &pFrame->rot);
            rdVector_NormalizeAngleAcute3(&rotVec);
            rdMatrix_BuildRotate34(&rotMat, &rotVec);
        }
        else {
            rdMatrix_PostRotate34(&rotMat, &rotVec);
        }
        
        if ( (thing->trackParams.flags & 0x10) != 0 )
        {
            rdVector_Add3(&a1a, &thing->trackParams.field_58, &rotMat.scale);
            rdVector_Sub3Acc(&a1a, &thing->position);
            if (!rdVector_IsZero3(&a1a))
            {
                a6 = stdMath_ClipPrecision(rdVector_Normalize3Acc(&a1a));
                if ( a6 != 0.0 )
                {
                    v18 = sithCollision_UpdateThingCollision(thing, &a1a, a6, RAYCAST_40 | RAYCAST_4);
                    if ( v18 < a6 )
                    {
                        rdMatrix_Copy34(&rotMat, &thing->trackParams.moveFrameOrientation);
                        a3 = v18 / a6 * v41 + thing->field_24C;
                        rdVector_Scale3(&rotVec, &thing->trackParams.moveFrameDeltaAngles, a3);
                        rdMatrix_PreRotate34(&rotMat, &rotVec);
                    }
                }
            }
        }
        thing->field_24C = a3;
        rdVector_Zero3(&thing->lookOrientation.scale);
        sithCollision_sub_4E77A0(thing, &rotMat);
        if ( thing->field_24C >= 1.0 )
        {
            if ( (thing->trackParams.flags & 0x10) == 0 )
            {
                rdMatrix_BuildRotate34(&rotMat, &thing->trackParams.orientation);
                rdVector_Zero3(&thing->lookOrientation.scale);
                sithCollision_sub_4E77A0(thing, &rotMat);
            }
            thing->trackParams.flags &= ~0x12;
        }
    }
    if ( (thing->trackParams.flags & 1) != 0 )
    {
        if ( thing->trackParams.field_1C <= (flex_d_t)deltaSeconds )
            v22 = thing->trackParams.field_1C;
        else
            v22 = deltaSeconds;
        v42 = v22;
        deltaSecondsa = stdMath_ClipPrecision(thing->trackParams.lerpSpeed * v22);
        if ( deltaSecondsa != 0.0 )
        {
            v26 = sithCollision_UpdateThingCollision(thing, &thing->trackParams.vel, deltaSecondsa, RAYCAST_40 | RAYCAST_4);
            a3a = v26;
            if ( v26 >= deltaSecondsa )
            {
                v22 = v42;
            }
            else
            {
                v30 = stdMath_ClipPrecision(a3a);
                if ( v30 <= 0.0 )
                {
                    v22 = 0.0;
                    ++thing->field_250;
                }
                else
                {
                    ++thing->field_250;
                    v22 = a3a / deltaSecondsa * v42;
                }
            }
        }
        v31 = thing->trackParams.field_1C - v22;
        thing->trackParams.field_1C = v31;
        v31 = stdMath_ClipPrecision(v31);
        if ( v31 == 0.0 )
        {
            thing->trackParams.flags &= ~1;
        }
        else if ( thing->field_250 > 2u)
        {
            sithTrackThing_BlockedIdk(thing);
        }
    }
    if ( (thing->trackParams.flags & 3) == 0 )
    {
        sithTrackThing_StoppedMoving(thing);
    }
}

void sithTrackThing_BlockedIdk(sithThing* pThing)
{
    if ((pThing->thingflags & SITH_TF_CAPTURED) && !(pThing->thingflags & SITH_TF_INVULN))
    {
        sithCog_SendMessageFromThing(pThing, 0, SITH_MESSAGE_BLOCKED);
    }
}

void sithTrackThing_StoppedMoving(sithThing* pThing)
{
    if ( (pThing->trackParams.flags & 4) != 0 )
    {
        pThing->curframe = pThing->field_258;
        if ( pThing->curframe != pThing->goalframe )
        {
            sithTrackThing_Arrivedidk(pThing);
            return;
        }
    }
    else
    {
        pThing->curframe = pThing->field_258;
    }
    sithTrackThing_Stop(pThing);
}

void sithTrackThing_PrepareForOrient(sithThing *thing, rdVector3 *pGoalFrameRot, flex_t a3)
{
    rdVector3 out;
    rdVector3 angles;

    rdMatrix_ExtractAngles34(&thing->lookOrientation, &out);
    rdVector_Sub3(&angles, pGoalFrameRot, &out);
    angles.x = stdMath_NormalizeAngleAcute(angles.x);
    angles.y = stdMath_NormalizeAngleAcute(angles.y);
    angles.z = stdMath_NormalizeAngleAcute(angles.z);

    if (stdMath_Fabs(angles.x) < 2.5)
        angles.x = 0.0;
    if (stdMath_Fabs(angles.y) < 2.5)
        angles.y = 0.0;
    if (stdMath_Fabs(angles.z) < 2.5)
        angles.z = 0.0;

    if ( !rdVector_IsZero3(&angles) )
    {
        rdMatrix_Copy34(&thing->trackParams.moveFrameOrientation, &thing->lookOrientation);
        thing->trackParams.field_54 = 1.0 / a3;
        rdVector_Zero3(&thing->trackParams.moveFrameOrientation.scale);
        rdVector_Copy3(&thing->trackParams.moveFrameDeltaAngles, &angles);
        thing->field_24C = 0.0;
        rdVector_Copy3(&thing->trackParams.orientation, pGoalFrameRot);
        thing->trackParams.flags &= ~0x10;
        thing->trackParams.flags |= 2;
    }
}

int sithTrackThing_LoadPathParams(stdConffileArg *arg, sithThing *thing, int param)
{
    switch (param)
    {
        case THINGPARAM_FRAME:
        {
            flex32_t tmpPosx, tmpPosy, tmpPosz;
            flex32_t tmpRotx, tmpRoty, tmpRotz;

            if ( thing->trackParams.loadedFrames < thing->trackParams.sizeFrames )
            {
                if ( _sscanf(arg->value, "(%f/%f/%f:%f/%f/%f)", &tmpPosx, &tmpPosy, &tmpPosz, &tmpRotx, &tmpRoty, &tmpRotz) != 6 )
                    return 0;
                sithThingFrame* pFrame = &thing->trackParams.aFrames[thing->trackParams.loadedFrames++];
                pFrame->pos.x = tmpPosx; // FLEXTODO
                pFrame->pos.y = tmpPosy; // FLEXTODO
                pFrame->pos.z = tmpPosz; // FLEXTODO
                pFrame->rot.x = tmpRotx; // FLEXTODO
                pFrame->rot.y = tmpRoty; // FLEXTODO
                pFrame->rot.z = tmpRotz; // FLEXTODO
                //rdVector_Copy3(&pFrame->pos, &tmpPos);
                //rdVector_Copy3(&pFrame->rot, &tmpRot);
            }
            else {
                // Added: print
                stdPlatform_Printf("OpenJKDF2: THINGPARAM_FRAME `%s` was ignored because thing->trackParams.loadedFrames (%d) >= thing->trackParams.sizeFrames (%d)\n", arg->value, thing->trackParams.loadedFrames < thing->trackParams.sizeFrames);
            }
            return 1;
        }

        case THINGPARAM_NUMFRAMES:
        {
            if ( thing->trackParams.sizeFrames )
                return 0;

            int numFrames = _atoi(arg->value);
            if ( numFrames < 1 ) {
                // Added: print
                stdPlatform_Printf("OpenJKDF2: THINGPARAM_NUMFRAMES (0x%x, %s) < 1??\n", numFrames, arg->value);
                return 0;
            }

            size_t alloc_sz = sizeof(sithThingFrame) * numFrames;
            thing->trackParams.aFrames = (sithThingFrame*)pSithHS->alloc(alloc_sz);
            if ( thing->trackParams.aFrames )
            {
                _memset(thing->trackParams.aFrames, 0, alloc_sz);
                thing->trackParams.sizeFrames = numFrames;
                thing->trackParams.loadedFrames = 0;
                return 1;
            }
            return 0;
        }

        default:
            return 0;
    }
}

void sithTrackThing_Stop(sithThing *thing)
{
    thing->trackParams.flags &= ~0x17u;
    thing->trackParams.lerpSpeed = 0.0;
    thing->goalframe = 0;
    thing->field_258 = 0;
    thing->field_250 = 0;
    sithSoundClass_ThingPauseSoundclass(thing, SITH_SC_MOVING);
    sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_STOPMOVE);
    if ( (thing->thingflags & SITH_TF_CAPTURED) != 0 && (thing->thingflags & SITH_TF_INVULN) == 0 )
        sithCog_SendMessageFromThing(thing, 0, SITH_MESSAGE_ARRIVED);
}

void sithTrackThing_idkpathmove(sithThing *thing, sithThing *thing2, rdVector3 *a3)
{
    sithThingFrame *v9; // esi
    rdVector3 a1a; // [esp+10h] [ebp-Ch] BYREF

    thing->trackParams.aFrames = (sithThingFrame *)pSithHS->alloc(sizeof(sithThingFrame) * thing2->trackParams.sizeFrames);
    _memcpy(thing->trackParams.aFrames, thing2->trackParams.aFrames, sizeof(sithThingFrame) * thing2->trackParams.sizeFrames);
    thing->trackParams.sizeFrames = thing2->trackParams.sizeFrames;
    thing->trackParams.loadedFrames = thing2->trackParams.loadedFrames;
    for (uint32_t v7 = 0; v7 < thing->trackParams.loadedFrames; v7++)
    {
        v9 = &thing->trackParams.aFrames[v7];
        rdVector_Rotate3(&a1a, a3, &v9->rot);
        rdVector_Add3Acc(&v9->pos, &a1a);
    }
}

void sithTrackThing_RotatePivot(sithThing *thing, rdVector3 *a2, rdVector3 *a3, flex_t a4)
{
    thing->trackParams.flags |= 0x12u;
    sithSoundClass_ThingPlaySoundclass4(thing, 3u);
    sithSoundClass_ThingPlaySoundclass4(thing, 5u);
    rdVector_Copy3(&thing->trackParams.field_58, a2);
    thing->curframe = -1;
    rdMatrix_Copy34(&thing->trackParams.moveFrameOrientation, &thing->lookOrientation);
    rdVector_Sub3(&thing->trackParams.moveFrameOrientation.scale, &thing->position, &thing->trackParams.field_58);
    rdVector_Copy3(&thing->trackParams.moveFrameDeltaAngles, a3);
    thing->field_24C = 0.0;
    thing->field_250 = 0;
    thing->trackParams.field_54 = 1.0 / a4;
}

void sithTrackThing_Rotate(sithThing *trackThing, rdVector3 *rot)
{
    flex_t largestAnglePercentage;
    flex_t tmp;

    largestAnglePercentage = 0.0;
    if ( rot->x != 0.0 )
    {
        tmp = 360.0 / rot->x;
        if ( tmp > 0.0 )
        {
            largestAnglePercentage = tmp;
        }
    }
    if ( rot->y != 0.0 )
    {
        tmp = 360.0 / rot->y;
        if ( tmp > largestAnglePercentage )
        {
            largestAnglePercentage = tmp;
        }
    }
    if ( rot->z != 0.0 )
    {
        tmp = 360.0 / rot->z;
        if ( tmp > largestAnglePercentage )
        {
            largestAnglePercentage = tmp;
        }
    }
    if ( largestAnglePercentage != 0.0 )
    {
        trackThing->trackParams.flags |= 0x42u;
        rdMatrix_Copy34(&trackThing->trackParams.moveFrameOrientation, &trackThing->lookOrientation);
        rdVector_Scale3(&trackThing->trackParams.moveFrameDeltaAngles, rot, largestAnglePercentage);
        trackThing->trackParams.field_54 = 1.0 / largestAnglePercentage;
        rdVector_Zero3(&trackThing->trackParams.moveFrameOrientation.scale);
        trackThing->field_24C = 0.0;
        trackThing->field_250 = 0;
        trackThing->curframe = -1;
    }
}

void sithTrackThing_SkipToFrame(sithThing *trackThing, uint32_t goalframeNum, flex_t a3)
{
    sithThingFrame *goalFrame; // eax
    flex_t v5; // st7

    if ( goalframeNum < trackThing->trackParams.loadedFrames )
    {
        trackThing->goalframe = goalframeNum;
        trackThing->trackParams.flags &= ~0x4;
        trackThing->trackParams.lerpSpeed = a3;
        sithSoundClass_ThingPlaySoundclass4(trackThing, SITH_SC_STARTMOVE);
        sithSoundClass_ThingPlaySoundclass4(trackThing, SITH_SC_MOVING);

        goalFrame = &trackThing->trackParams.aFrames[trackThing->goalframe];

        rdVector_Sub3(&trackThing->trackParams.vel, &goalFrame->pos, &trackThing->position);
        
        v5 = rdVector_Normalize3Acc(&trackThing->trackParams.vel);
        if ( v5 != 0.0 )
        {
            trackThing->field_250 = 0;
            trackThing->trackParams.flags |= 1;
            trackThing->trackParams.field_1C = v5 / trackThing->trackParams.lerpSpeed;
        }
    }
}

int sithTrackThing_PathMovePause(sithThing *trackThing)
{
    if ( (trackThing->trackParams.flags & 3) == 0 )
        return 0;

    sithSoundClass_ThingPauseSoundclass(trackThing, SITH_SC_MOVING);

    trackThing->trackParams.flags |= 0x80;
    return 1;
}

int sithTrackThing_PathMoveResume(sithThing *trackThing)
{
    if (!(trackThing->trackParams.flags & 0x80))
        return 0;

    sithSoundClass_ThingPlaySoundclass4(trackThing, SITH_SC_MOVING);

    trackThing->trackParams.flags &= ~0x80;
    return 1;
}
