#include "stdHashTable.h"

#include "jk.h"

#include "stdPlatform.h"
#include "General/stdLinklist.h"
#include "General/stdSingleLinklist.h"
#include <math.h>
#include <stdlib.h>

#define hashmapBucketSizes_MAX (32)

int hashmapBucketSizes[hashmapBucketSizes_MAX] = 
{
    23,
    53,
    79,
    101,
    151,
    211,
    251,
    307,
    353,
    401,
    457,
    503,
    557,
    601,
    653,
    701,
    751,
    809,
    853,
    907,
    953,
    1009,
    1103,
    1201,
    1301,
    1409,
    1511,
    1601,
    1709,
    1801,
    1901,
    1999
};

uint32_t stdHashTable_HashStringToIdx(const char *data, uint32_t numBuckets)
{
    uint32_t hash;
    uint8_t i;
    
    if (!data || !data[0]) return 0; // Added

#ifdef STDHASHTABLE_CRC32_KEYS
    // TODO: check performance on this
    hash = stdCrc32(data, strlen(data));
#else
    hash = 0;
    for ( i = *data; i; ++data )
    {
        hash = (65599 * hash) + i;
        i = (uint8_t)data[1];
    }
#endif
    return hash % numBuckets;
}

stdHashTable* stdHashTable_New(int maxEntries)
{
    stdHashTable *hashtable;
    int sizeIterIdx;
    signed int calcedPrime;
    int *sizeIter;
    int actualNumBuckets = 1999;
    signed int v7;

    hashtable = (stdHashTable *)std_pHS->alloc(sizeof(stdHashTable));
    if (!hashtable)
        return NULL;

    // Added: memset
    _memset(hashtable, 0, sizeof(*hashtable));

    // Basically every usage of stdHashTable_New assumes maxEntries is
    // exactly what it says, the maximum anticipated number of entries.
    //
    // But this constructor seems to interpret that as maxBuckets, which
    // means everything is an O(1) lookup but also that the linked lists
    // never actually get uh, linked. lol
    //
    // So this just log2's the argument to make stdHashTable smaller in RAM
    // and O(log2(n)) lookups
#ifdef STDHASHTABLE_LOG2_BUCKETS
    maxEntries = (int)log2(maxEntries) / 2;
#endif

    sizeIterIdx = 0;
    calcedPrime = maxEntries;
    sizeIter = hashmapBucketSizes;
    hashtable->numBuckets = 0;
    hashtable->buckets = 0;
    hashtable->keyHashToIndex = 0;
    while ( maxEntries >= *sizeIter )
    {
        ++sizeIter;
        ++sizeIterIdx;
        if ( sizeIter >= &hashmapBucketSizes[hashmapBucketSizes_MAX] )
        {
            actualNumBuckets = maxEntries;
            sizeIterIdx = hashmapBucketSizes_MAX-1;
            break;
        }
    }
    actualNumBuckets = hashmapBucketSizes[sizeIterIdx];

    // Calculate a prime number?
    if ( maxEntries > 1999 )
    {
        while ( 1 )
        {
            v7 = 2;
            if ( calcedPrime - 1 <= 2 )
            break;
            while ( calcedPrime % v7 )
            {
                if ( ++v7 >= calcedPrime - 1 )
                    goto loop_escape;
            }
            ++calcedPrime;
        }
loop_escape:
        actualNumBuckets = calcedPrime;
    }

    hashtable->numBuckets = actualNumBuckets;
    hashtable->buckets = (tHashLink *)std_pHS->alloc(sizeof(tHashLink) * actualNumBuckets);
    if ( hashtable->buckets )
    {
      _memset(hashtable->buckets, 0, sizeof(tHashLink) * hashtable->numBuckets);
      hashtable->keyHashToIndex = stdHashTable_HashStringToIdx;
    }
    else {
        // Added: fail more gracefully and without memleaks
        std_pHS->free(hashtable);
        return NULL;
    }
    return hashtable;
}

tHashLink* stdHashTable_GetBucketTail(tHashLink *pLL)
{
#ifdef STDHASHTABLE_SINGLE_LINKLIST
    return stdSingleLinklist_GetTail(pLL);
#else
    return stdLinklist_GetTail(pLL);
#endif
}

void stdHashTable_FreeBuckets(tHashLink *a1)
{
    tHashLink *iter;
    
    // Added: nullptr check
    if (!a1) return;

    iter = a1->next;
    while ( iter )
    {
        // TODO verify possible regression, prevent double free?
        tHashLink* next_iter = iter->next;
        iter->next = NULL; // added

        //printf("Free from %p: %p\n", a1, iter);
        std_pHS->free(iter);
        
        iter = next_iter;
    }
}

void stdHashTable_Free(stdHashTable *table)
{
    int bucketIdx;
    int bucketIdx2;
    tHashLink *iter;
    tHashLink *iter_child;
    
    // Added: nullptr check
    if (!table) return;

    bucketIdx = 0;
    if ( table->numBuckets > 0 )
    {
        bucketIdx2 = 0;
        do
        {
            stdHashTable_FreeBuckets(&table->buckets[bucketIdx2]);
            table->buckets[bucketIdx2].next = NULL; // added
            ++bucketIdx;
            ++bucketIdx2;
        }
        while ( bucketIdx < table->numBuckets );
    }
    std_pHS->free(table->buckets);
    table->buckets = NULL; // added
    
    std_pHS->free(table);
}

int stdHashTable_SetKeyVal(stdHashTable *hashmap, const char *key, void *value)
{
    tHashLink *new_child; // eax
    tHashLink *v9; // ecx
    tHashLink *v10; // esi

    // ADDED
    if (!hashmap || !key)
        return 0;

    if (stdHashTable_GetKeyVal(hashmap, key)) {
#ifndef SITH_DEBUG_STRUCT_NAMES
        stdHashTable_FreeKey(hashmap, key);
#else
        return 0;
#endif
    }

    v9 = &hashmap->buckets[hashmap->keyHashToIndex(key, hashmap->numBuckets)];
    v10 = stdHashTable_GetBucketTail(v9);

    if ( v10->key )
    {
        new_child = (tHashLink *)std_pHS->alloc(sizeof(tHashLink));
        if (!new_child)
            return 0;
        //printf("Alloc to %p: %p %s\n", v9, new_child, key);

        _memset(new_child, 0, sizeof(*new_child));
#ifdef STDHASHTABLE_CRC32_KEYS
        new_child->keyCrc32 = stdCrc32(key, strlen(key));
#else
        new_child->key = key;
#endif
        new_child->value = value;
#ifdef STDHASHTABLE_SINGLE_LINKLIST
        stdSingleLinklist_InsertAfter(v10, new_child);
#else
        stdLinklist_InsertAfter(v10, new_child);
#endif
    }
    else
    {
        _memset(v9, 0, sizeof(*v9));
#ifdef STDHASHTABLE_CRC32_KEYS
        v9->keyCrc32 = stdCrc32(key, strlen(key));
#else
        v9->key = key;
#endif
        v9->value = value;

        //printf("Bin to %p: %p %s\n", v9, new_child, key);
    }
    return 1;
}

void* stdHashTable_GetKeyVal(stdHashTable *hashmap, const char *key)
{
    tHashLink *i;
    tHashLink *foundKey;

    if (!hashmap || !key) // Added: key nullptr check
        return NULL;

#ifdef STDHASHTABLE_CRC32_KEYS
    uint32_t keyCrc32 = stdCrc32(key, strlen(key));
#endif

    foundKey = 0;
    for ( i = &hashmap->buckets[hashmap->keyHashToIndex(key, hashmap->numBuckets)]; i; i = i->next )
    {
#ifdef STDHASHTABLE_CRC32_KEYS
        if (!i->keyCrc32) {
            foundKey = 0;
            break;
        }
        if (i->keyCrc32 == keyCrc32) {
            foundKey = i;
            break;
        }
#else
        const char* key_iter = (const char *)i->key;
        if ( !key_iter )
        {
            foundKey = 0;
            break;
        }
        if ( !_strcmp(key_iter, key) )
        {
            foundKey = i;
            break;
        }
#endif
    }

    if (foundKey) {
        return foundKey->value;
    }

    return 0;
}

int stdHashTable_FreeKey(stdHashTable *hashtable, const char *key)
{
    int v2;
    tHashLink *foundKey;
    tHashLink *i;
    tHashLink *bucketTopKey;

    if (!hashtable || !key) // Added: key nullptr
        return 0;

#ifdef STDHASHTABLE_CRC32_KEYS
    uint32_t keyCrc32 = stdCrc32(key, strlen(key));
#endif

    tHashLink* beforeFoundKey = NULL; // added
    foundKey = 0;
    v2 = hashtable->keyHashToIndex(key, hashtable->numBuckets);
    for ( i = &hashtable->buckets[v2]; i; i = i->next )
    {
#ifdef STDHASHTABLE_CRC32_KEYS
        if (!i->keyCrc32) {
            break;
        }
        if (i->keyCrc32 == keyCrc32)
        {
            foundKey = i;
            break;
        }
#else
        const char* key_iter = i->key;
        if ( !key_iter )
            break;
        if ( !_strcmp(key_iter, key) )
        {
            foundKey = i;
            break;
        }
#endif
        beforeFoundKey = i;
    }

    if ( !foundKey )
        return 0;

    //stdLinklist_UnlinkChild(foundKey); // Added: Moved to prevent freeing issues
    bucketTopKey = &hashtable->buckets[v2];
    if ( bucketTopKey == foundKey )
    {
        tHashLink* pNext = foundKey->next;
        if ( pNext )
        {
#ifdef STDHASHTABLE_CRC32_KEYS
            bucketTopKey->keyCrc32 = pNext->keyCrc32;
#else
            bucketTopKey->key = pNext->key;
#endif
            bucketTopKey->value = pNext->value;

#ifdef STDHASHTABLE_SINGLE_LINKLIST
            stdSingleLinklist_InsertReplace(pNext, bucketTopKey);
#else
            stdLinklist_InsertReplace(pNext, bucketTopKey);
#endif
            std_pHS->free(pNext);
        }
        else
        {
#ifndef STDHASHTABLE_SINGLE_LINKLIST
            bucketTopKey->prev = NULL;
#endif
            bucketTopKey->next = NULL;
#ifdef STDHASHTABLE_CRC32_KEYS
            bucketTopKey->keyCrc32 = 0;
#else
            bucketTopKey->key = NULL;
#endif
            bucketTopKey->value = 0;
        }
    }
    else
    {
#ifdef STDHASHTABLE_SINGLE_LINKLIST
        stdSingleLinklist_UnlinkChild(foundKey, beforeFoundKey); // Added: Moved to prevent freeing issues
#else
        stdLinklist_UnlinkChild(foundKey); // Added: Moved to prevent freeing issues
#endif
        std_pHS->free(foundKey);
    }
    return 1;
}

#ifdef STDHASHTABLE_CRC32_KEYS
int stdHashTable_FreeKeyCrc32(stdHashTable *hashtable, uint32_t keyCrc32)
{
    int v2;
    tHashLink *foundKey;
    tHashLink *i;
    tHashLink *bucketTopKey;

    if (!hashtable)
        return 0;

    tHashLink* beforeFoundKey = NULL; // added
    foundKey = 0;
    //v2 = hashtable->keyHashToIndex(key, hashtable->numBuckets);
    v2 = keyCrc32 % hashtable->numBuckets;
    for ( i = &hashtable->buckets[v2]; i; i = i->next )
    {
        if (!i->keyCrc32) {
            break;
        }
        if (i->keyCrc32 == keyCrc32)
        {
            foundKey = i;
            break;
        }
        beforeFoundKey = i;
    }

    if ( !foundKey )
        return 0;

    //stdLinklist_UnlinkChild(foundKey); // Added: Moved to prevent freeing issues
    bucketTopKey = &hashtable->buckets[v2];
    if ( bucketTopKey == foundKey )
    {
        tHashLink* pNext = foundKey->next;
        if ( pNext )
        {
            bucketTopKey->keyCrc32 = pNext->keyCrc32;
            bucketTopKey->value = pNext->value;

#ifdef STDHASHTABLE_SINGLE_LINKLIST
            stdSingleLinklist_InsertReplace(pNext, bucketTopKey);
#else
            stdLinklist_InsertReplace(pNext, bucketTopKey);
#endif
            std_pHS->free(pNext);
        }
        else
        {
#ifndef STDHASHTABLE_SINGLE_LINKLIST
            bucketTopKey->prev = NULL;
#endif
            bucketTopKey->next = NULL;
            bucketTopKey->keyCrc32 = 0;
            bucketTopKey->value = 0;
        }
    }
    else
    {
#ifdef STDHASHTABLE_SINGLE_LINKLIST
        stdSingleLinklist_UnlinkChild(foundKey, beforeFoundKey); // Added: Moved to prevent freeing issues
#else
        stdLinklist_UnlinkChild(foundKey); // Added: Moved to prevent freeing issues
#endif
        std_pHS->free(foundKey);
    }
    return 1;
}
#endif

void stdHashTable_PrintDiagnostics(stdHashTable *hashtable)
{
    int maxLookups; // edi
    int bucketIdx2; // ebp
    int bucketIdx; // ebx
    int numChildren; // eax
    signed int numFilled; // [esp+14h] [ebp-Ch]
    signed int totalChildren; // [esp+18h] [ebp-8h]

    std_pHS->debugPrint("HASHTABLE Diagnostics\n");
    std_pHS->debugPrint("---------------------\n");
    maxLookups = 0;
    bucketIdx2 = 0;
    numFilled = 0;
    totalChildren = 0;
    if ( hashtable->numBuckets > 0 )
    {
        bucketIdx = 0;
        do
        {
            if ( hashtable->buckets[bucketIdx].key )
            {
                ++numFilled;
#ifdef STDHASHTABLE_SINGLE_LINKLIST
                numChildren = stdSingleLinklist_NumChildren(&hashtable->buckets[bucketIdx]);
#else
                numChildren = stdLinklist_NumChildren(&hashtable->buckets[bucketIdx]);
#endif
                totalChildren += numChildren;
                if ( numChildren > maxLookups )
                    maxLookups = numChildren;
            }
            ++bucketIdx2;
            ++bucketIdx;
        }
        while ( bucketIdx2 < hashtable->numBuckets );
    }
    std_pHS->debugPrint(" Maximum Lookups = %d\n", maxLookups);
    std_pHS->debugPrint(" Filled Indices = %d/%d (%2.2f%%)\n", numFilled, hashtable->numBuckets, (flex_t)numFilled * 100.0 / (flex_t)hashtable->numBuckets); // FLEXTODO
    std_pHS->debugPrint(" Average Lookup = %2.2f\n", (flex_t)totalChildren / (flex_t)numFilled); // FLEXTODO
    std_pHS->debugPrint(" Weighted Lookup = %2.2f\n", (flex_t)totalChildren / (flex_t)hashtable->numBuckets); // FLEXTODO
    std_pHS->debugPrint("---------------------\n");
}

void stdHashTable_Dump(stdHashTable *hashtable)
{
    int index;
    tHashLink *key_iter;

    std_pHS->debugPrint("HASHTABLE\n---------\n");
    index = 0;
    if ( hashtable->numBuckets > 0 )
    {
        do
        {
            std_pHS->debugPrint("Index: %d\t", index);
            key_iter = &hashtable->buckets[index];
            std_pHS->debugPrint("Strings:", index);
            for ( ; key_iter; key_iter = key_iter->next )
                std_pHS->debugPrint(" '%s'", key_iter->key);
            std_pHS->debugPrint("\n");
            ++index;
        }
        while ( index < hashtable->numBuckets );
    }
}
