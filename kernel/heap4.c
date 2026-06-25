/*
 * heap4.c - Heap-4 Coalescing Block Allocator
 *
 * Based on the FreeRTOS heap_4 algorithm:
 * - First-fit allocation with block splitting
 * - Adjacent free block coalescing on free
 * - O(n) alloc, O(1) free (amortized with coalescing)
 */

#include "heap4.h"
#include "os_config.h"
#include <string.h>
#include <stdint.h>

/* ========== Internal Data ========== */

/* The heap memory pool, aligned to OS_CONFIG_HEAP_ALIGNMENT */
static uint8_t uc_heap[OS_CONFIG_HEAP_ALIGNMENT +
                       OS_CONFIG_HEAP_SIZE +
                       OS_CONFIG_HEAP_ALIGNMENT]
    __attribute__((aligned(OS_CONFIG_HEAP_ALIGNMENT)));

/* Block list head */
static os_block_header_t *px_block_list = NULL;

/* Statistics */
static uint32_t free_bytes_remaining   = 0;
static uint32_t min_free_bytes_ever    = 0;
static uint32_t heap_initialized       = 0;

/* Hook */
static os_malloc_failed_hook_t malloc_failed_hook = NULL;

/* Macro: get pointer just past the header */
#define BLOCK_DATA(hdr)  ((void*)((uint8_t*)(hdr) + sizeof(os_block_header_t)))

/* Macro: get header from data pointer */
#define DATA_BLOCK(ptr)  ((os_block_header_t*)((uint8_t*)(ptr) - sizeof(os_block_header_t)))

/* Align size up to heap alignment boundary */
#define ALIGN_UP(size)   (((size) + OS_CONFIG_HEAP_ALIGNMENT - 1) & ~(OS_CONFIG_HEAP_ALIGNMENT - 1))

/* Marker to detect corruption */
#define BLOCK_ALLOCATED_BIT   0x80000000U
#define BLOCK_SIZE_MASK       0x7FFFFFFFU

/* ========== Internal Functions ========== */

static uint32_t prv_block_get_size(os_block_header_t *block)
{
    return block->size & BLOCK_SIZE_MASK;
}

static bool prv_block_is_allocated(os_block_header_t *block)
{
    return (block->size & BLOCK_ALLOCATED_BIT) != 0;
}

static void prv_block_set_allocated(os_block_header_t *block)
{
    block->size |= BLOCK_ALLOCATED_BIT;
    block->allocated = 1;
}

static void prv_block_set_free(os_block_header_t *block)
{
    block->size &= BLOCK_SIZE_MASK;
    block->allocated = 0;
}

/*
 * Insert a block into the free list in address order.
 * This is critical for coalescing to work.
 */
static void prv_insert_block(os_block_header_t *block_to_insert)
{
    os_block_header_t *iter;
    os_block_header_t *prev = NULL;

    /* Walk the list to find the correct position (address order) */
    for (iter = px_block_list; iter != NULL; iter = iter->next) {
        if ((uint8_t*)iter > (uint8_t*)block_to_insert) {
            break;
        }
        prev = iter;
    }

    /* Insert before iter */
    block_to_insert->next = iter;
    if (prev != NULL) {
        prev->next = block_to_insert;
    } else {
        px_block_list = block_to_insert;
    }

    /* Try to coalesce with previous block */
    if (prev != NULL) {
        uint8_t *prev_end = (uint8_t*)BLOCK_DATA(prev) + prv_block_get_size(prev);
        if (prev_end == (uint8_t*)block_to_insert && !prv_block_is_allocated(prev)) {
            /* Merge with previous */
            prev->size = (prev->size & BLOCK_SIZE_MASK) +
                          sizeof(os_block_header_t) +
                          prv_block_get_size(block_to_insert);
            prev->next = block_to_insert->next;
            block_to_insert = prev; /* Update for next coalesce check */
        }
    }

    /* Try to coalesce with next block */
    if (block_to_insert->next != NULL) {
        uint8_t *block_end = (uint8_t*)BLOCK_DATA(block_to_insert) +
                              prv_block_get_size(block_to_insert);
        if (block_end == (uint8_t*)(block_to_insert->next) &&
            !prv_block_is_allocated(block_to_insert->next)) {
            /* Merge with next */
            block_to_insert->size = (block_to_insert->size & BLOCK_SIZE_MASK) +
                                     sizeof(os_block_header_t) +
                                     prv_block_get_size(block_to_insert->next);
            block_to_insert->next = block_to_insert->next->next;
        }
    }
}

/* ========== Public API ========== */

void os_heap_init(void)
{
    os_block_header_t *first_block;
    uint8_t *aligned_heap;

    /* Align the heap start address */
    aligned_heap = (uint8_t*)ALIGN_UP((uintptr_t)uc_heap);

    /* Initialize the first block spanning the entire heap */
    first_block = (os_block_header_t*)aligned_heap;
    first_block->next = NULL;
    first_block->size = OS_CONFIG_HEAP_SIZE - sizeof(os_block_header_t);
    first_block->allocated = 0;

    px_block_list = first_block;
    free_bytes_remaining = first_block->size;
    min_free_bytes_ever = free_bytes_remaining;
    heap_initialized = 1;
}

void* os_heap_alloc(uint32_t size)
{
    os_block_header_t *block, *new_block;
    void *result = NULL;

    if (size == 0 || !heap_initialized) {
        return NULL;
    }

    /* Align the requested size */
    size = ALIGN_UP(size);

    /* Walk the free list to find a suitable block (first-fit) */
    for (block = px_block_list; block != NULL; block = block->next) {
        if (!prv_block_is_allocated(block) && prv_block_get_size(block) >= size) {
            break;
        }
    }

    if (block == NULL) {
        /* Call malloc failed hook */
        if (malloc_failed_hook != NULL) {
            malloc_failed_hook(size);
        }
        return NULL; /* No suitable block found */
    }

    /* Check if we can split this block */
    uint32_t block_size = prv_block_get_size(block);
    if (block_size >= size + sizeof(os_block_header_t) + OS_CONFIG_HEAP_ALIGNMENT) {
        /* Split: create a new free block after the allocated portion */
        new_block = (os_block_header_t*)((uint8_t*)BLOCK_DATA(block) + size);
        new_block->size = block_size - size - sizeof(os_block_header_t);
        new_block->allocated = 0;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;

        /* Free reduced by user data + header consumed by split */
        free_bytes_remaining -= (size + sizeof(os_block_header_t));
    } else {
        /* No split: entire block consumed */
        free_bytes_remaining -= block_size;
    }

    /* Mark as allocated */
    prv_block_set_allocated(block);

    /* Update high-water mark */
    if (free_bytes_remaining < min_free_bytes_ever) {
        min_free_bytes_ever = free_bytes_remaining;
    }

    result = BLOCK_DATA(block);
    return result;
}

void* os_heap_calloc(uint32_t count, uint32_t size)
{
    uint32_t total = count * size;
    void *ptr = os_heap_alloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void os_heap_free(void *ptr)
{
    os_block_header_t *block;

    if (ptr == NULL || !heap_initialized) {
        return;
    }

    block = DATA_BLOCK(ptr);

    /* Sanity check: was it actually allocated? */
    if (!prv_block_is_allocated(block)) {
        /* Double free or corrupt pointer */
        return;
    }

    /* Mark as free */
    free_bytes_remaining += prv_block_get_size(block);
    prv_block_set_free(block);

    /* Re-insert into the free list (with coalescing) */
    /* First remove from its current position by walking from head */
    /* Actually, for heap_4 we maintain a single sorted list.
       We need to remove the block from its current position
       and re-insert it for coalescing. */
    {
        os_block_header_t *prev = NULL;
        os_block_header_t *iter;

        /* Remove block from list */
        for (iter = px_block_list; iter != NULL; iter = iter->next) {
            if (iter == block) {
                if (prev != NULL) {
                    prev->next = block->next;
                } else {
                    px_block_list = block->next;
                }
                break;
            }
            prev = iter;
        }
    }

    /* Re-insert with coalescing */
    prv_insert_block(block);
}

void* os_heap_realloc(void *ptr, uint32_t new_size)
{
    void *new_ptr;
    os_block_header_t *block;
    uint32_t old_size;

    if (ptr == NULL) {
        return os_heap_alloc(new_size);
    }
    if (new_size == 0) {
        os_heap_free(ptr);
        return NULL;
    }

    block = DATA_BLOCK(ptr);
    old_size = prv_block_get_size(block);
    new_size = ALIGN_UP(new_size);

    if (new_size <= old_size) {
        /* Shrinking - could split, but for simplicity just return same ptr */
        return ptr;
    }

    /* Growing - allocate new, copy, free old */
    new_ptr = os_heap_alloc(new_size);
    if (new_ptr != NULL) {
        memcpy(new_ptr, ptr, old_size);
        os_heap_free(ptr);
    }
    return new_ptr;
}

uint32_t os_heap_get_free_size(void)
{
    return free_bytes_remaining;
}

uint32_t os_heap_get_largest_free_block(void)
{
    os_block_header_t *block;
    uint32_t largest = 0;

    for (block = px_block_list; block != NULL; block = block->next) {
        if (!prv_block_is_allocated(block)) {
            uint32_t sz = prv_block_get_size(block);
            if (sz > largest) {
                largest = sz;
            }
        }
    }
    return largest;
}

uint32_t os_heap_get_min_free_size(void)
{
    return min_free_bytes_ever;
}

/* ========== Hook ========== */

void os_heap_set_malloc_failed_hook(os_malloc_failed_hook_t hook)
{
    malloc_failed_hook = hook;
}