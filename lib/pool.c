// TODO:
//  - implement destructor logic for pool
//  - implement `pool_clear()`
//  - implement pool and sparce-set mashup
//

#ifndef __HCH_POOL_H
#define __HCH_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define INDEX_INVALID ((size_t)-1)

typedef size_t           Index;
typedef unsigned char bitmask8;

typedef enum {
	PoolState_allocated = 1,
} PoolState;

typedef struct {
    size_t      typesize;
    char*       type;

    void*       data;
    Index*      free_indexes;
    
    void        (*destructor) (void*);
    size_t      capacity;
    size_t      count;
    size_t      free_count;
    size_t      max_size;
} Pool;

#ifndef POOL_MALLOC
#   define POOL_MALLOC(S) malloc(S)
#endif

#ifndef POOL_FREE
#   define POOL_FREE(P) free(P)
#endif

#ifndef POOL_REALLOC
#   define POOL_REALLOC(P,S) realloc(P,S)
#endif

// i can cause segmentation fault on assert, 
// so that i can force GDB to capture the state in 
// which program(library) have failed.
#ifdef DEBUG_SEGFAULT_ON_ASSERT
#   define FAULT_TRIGGER \
        *((int*)0) = 1 
#endif


#ifndef FAULT_TRIGGER
#define FAULT_TRIGGER // does nothing 
#endif

// i use custom assert, because i want to print
// value that caused the failure, sometimes.
#ifndef hch_assert
#define hch_assert(COND,...) \
    do { if (!(COND)) { \
        fprintf(stderr,"Assertion at [%s]: ",__func__); \
        fprintf(stderr,__VA_ARGS__); \
        fprintf(stderr,"\n"); \
        FAULT_TRIGGER;      \
        exit(1);            \
    }} while(0)
#endif

#ifndef IGNORE_RETURN
#   define IGNORE_RETURN (void)
#endif

#define __POOL_typestring(T) #T

#ifndef POOL_DEFAULT_CAPACITY
#   define POOL_DEFAULT_CAPACITY 32
#endif

#define POOL_ITEM_POINTER(p,index) \
    p->data + index * (p->typesize + sizeof(bitmask8));



//      //
/* API  */
//      //

#define     pool_new(T)                         __pool_init(NULL, POOL_DEFAULT_CAPACITY, sizeof(T), __POOL_typestring(T))
#define     pool_init(P,T,S)                    IGNORE_RETURN __pool_init(P, S, sizeof(T), __POOL_typestring(T))
void        pool_resize(Pool* p, size_t newsize);
Index       pool_reserve(Pool* p);
void        pool_release(Pool* p, Index i);
void*       pool_refer(Pool* p, Index i);


Pool __pool_init(Pool* self, size_t capacity, size_t typesize, char* type) {
    const size_t data_sz_bytes = capacity * ( sizeof(bitmask8) + typesize );
    const size_t idxs_sz_bytes = capacity * sizeof(Index);

    Pool new = {
        .type = type,
        .typesize = typesize,
        .capacity = capacity,
        .count = 0,
        .free_count = 0,
        .max_size = 0,

        .data = POOL_MALLOC(data_sz_bytes),
        .free_indexes = POOL_MALLOC(idxs_sz_bytes),
    };

    memset(new.data,0,data_sz_bytes);
    memset(new.free_indexes,0,idxs_sz_bytes);

    if (!self) {
        return new;
    } 

    memcpy(self, &new, sizeof(new));
    return *self;
}

void pool_free(Pool* p) {
    free(p->data);
    free(p->free_indexes);
    memset(p,0,sizeof(*p));
}

void* __pool_force_refer(Pool* p, Index i) {
    hch_assert(p, "Expected to have valid pointer got NULL");
    hch_assert(p->data, "Expected to have valid data pointer initilized");
    return POOL_ITEM_POINTER(p,i);
}

void* pool_refer(Pool* p, Index i) {
    void* ptr = __pool_force_refer(p,i);
    bitmask8 state = *((bitmask8*)ptr);
    void* payload = ptr + sizeof(bitmask8);
    return (state & PoolState_allocated) ? payload : NULL;
}

void pool_resize(Pool* p, size_t newsize) {
    const size_t newsize_bytes = newsize * (sizeof(bitmask8) + p->typesize);
    const size_t newsize_indexes_bytes = newsize * sizeof(Index);

    if (p->capacity == newsize) 
        return;
    else if (p->capacity > newsize) {
        // TODO:
        // impl destructor
    } 
 
    p->capacity     = newsize;
    p->data         = POOL_REALLOC(p->data,         newsize_bytes           );
    p->free_indexes = POOL_REALLOC(p->free_indexes, newsize_indexes_bytes   );
}

#define pool_append(P, VAR) \
    __pool_append(P, &(VAR), sizeof(VAR))

Index __pool_append(Pool* p, void* data, size_t typesize) {
    Index i = pool_reserve(p);
    if (i == INDEX_INVALID) 
        return INDEX_INVALID;

    hch_assert(typesize == p->typesize, 
            "Expected to have a type that equal or less than pool typesize");
    memcpy(pool_refer(p,i),data,typesize);
    return i;
}

Index pool_reserve(Pool* p) {
    Index index = INDEX_INVALID;
    if (p->free_count > 0) {
        index = p->free_indexes[p->free_count-1];
        p->free_count--;
    } else {
        index = p->count;
    }
    //debug("Reserved id: %u\n",ptr);
    hch_assert(index != INDEX_INVALID, "Failed to reserve entity");
    hch_assert(p->count < p->capacity, "Attempt to buffer overflow");

    bitmask8* state = __pool_force_refer(p, index);
    *state |= PoolState_allocated;

    p->count++;
    if (p->count > p->max_size) {
        p->max_size = p->count;
    }
    return index;
}

void  pool_release(Pool* p, Index i) {
    if (p->count==0)
        return;

    hch_assert(i < p->capacity, "Attempt to access Out of Bounds");
    bitmask8* state = POOL_ITEM_POINTER(p,i);

    if (!(*state & PoolState_allocated))
        return;
 
    p->free_indexes[p->free_count] = i;
    p->free_count++;

    *state ^= PoolState_allocated;
    p->count--;
}


void pool_iter(Pool* p, void (*item_proc) (void* item, void* ret), void* ret) {
    void* item = NULL;
    for(size_t i = 0; i < p->max_size; i++) {
        item = pool_refer(p,i);
        if (item) item_proc(item, ret);
    }
}

#endif //__HCH_POOL_H


#ifdef POOL_ACT_AS_EXAMPLE
int main(void) {
    Pool p = pool_new(int);   
    pool_resize(&p,2);

    int data = 20;
    Index i1 = pool_append(&p, data);


    data = 40;
    Index i2 = pool_append(&p, data);

    int* d1 = pool_refer(&p,i1);
    int* d2 = pool_refer(&p,i2);
 
    printf("%lu : %i\n", i1, *d1);
    printf("%lu : %i\n", i2, *d2);
    
    pool_release(&p, i1);
    pool_release(&p, i2);
    pool_free(&p);
}
#endif
