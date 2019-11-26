

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define K 1024
#define M (1024 * 1024)
#define G (1024 * 1024 * 1024)


/**
 * Struct to be placed at the front and end of every memory block
 */
typedef struct meta_data {
    int free;
    size_t size;
    struct meta_data *next;
    struct meta_data *prev;
    struct meta_data *next_free;
    struct meta_data *prev_free;
} meta;

static meta *head = NULL;
static meta *tail = NULL;
static meta *free_head = NULL;
static meta *free_tail = NULL;
//static char last_alloc[10];
static int heap_s = 0;
static int heap_e = 0;
// Initializes a meta struct
void init_meta(meta *m, int f, size_t s, meta *n, meta *p) {
    m->free = f;
    m->size = s;
    m->next = n;
    m->prev = p;
//    m->next_free = NULL;
//    m->prev_free = NULL;
//    if (f) { m->next_free = free_head; free_head = m; }
//    if (f && free_tail == NULL) { free_tail = m; }
/*    m->next_free = NULL;
    m->prev_free = NULL;
    if(f) {
        if (free_head == NULL) {
            free_head = m;
            free_tail = m;
        }
        m->prev_free = free_tail;
        free_tail->next_free = m;
        free_tail = m;
    }*/
}

void clear_meta(meta *m) {
    m->free = 0;
    m->size = 0;
    m->next = NULL;
    m->prev = NULL;
    m->next_free = NULL;
    m->prev_free = NULL;
}

void print_list() {
    meta *m = head;
    fprintf(stderr, "Total List:   Head: %p     ", head);
    while (m) {
        fprintf(stderr, "%p->", m);
        m = m->next;
    }
    fprintf(stderr, "     Tail: %p\n", tail);
}

void print_free_list() {
    meta *m = free_head;
    fprintf(stderr, "Free List:   Head: %p     ", free_head);
    while (m) {
        fprintf(stderr, "%p->", m);
        m = m->next_free;
    }
    fprintf(stderr, "     Tail: %p\n", free_tail);
}

// Used for Debugging (not really lol)
int valid_ptr(void *ptr) {
    meta *m = head;
    while(m) {
        if (ptr == m + 1) return 1;
    }
    return 0;
}


/*  Probably won't need these any more
// Returns the meta pointer at the back of a memory block
// (Input MUST be the front meta pointer)
meta *back(meta *front) {
    return (meta*)(((void*)(front)) + front->size - sizeof(meta));
}
// Returns the meta pointer at the front of a memory block
// (Input MUST be the back meta pointer)
meta *front(meta *back) {
    return (meta*)(((void*)(back)) - back->size + sizeof(meta));
}
*/

/**
 * Splits the given block into one block with size s and another block with
 * size prev_size - s
 *
 * @param p
 *    Pointer to block (must be beginning meta pointer)
 * @param s
 *    Size of first split block
 */
void split_block(meta *p, size_t s) {
    //if (!p->free) return;
    if (p->size - s < 2 * sizeof(meta)) return;
    meta *block_2 = (meta*)(((void*)(p)) + s);
    init_meta(block_2, 1, p->size - s, p->next, p);
    block_2->next_free = NULL;
    block_2->prev_free = NULL;
    init_meta(p, p->free, s, block_2, p->prev);
    if (block_2->next != NULL) block_2->next->prev = block_2;
    if (p == tail) tail = block_2;
    if (free_head == NULL) { free_head = block_2; free_tail = block_2; }
    else { block_2->prev_free = free_tail; free_tail->next_free = block_2; free_tail = block_2; }
    //meta *p_back = back(p);
    //init_meta(p_back, p->free, s, block_2);
    //meta *block_2_back = back(block_2);
    //init_meta(block_2_back, 1, block_2->size, block_2->next);
}

// Finds the first available free block
meta *first_fit(size_t s) {
    meta *p = free_head;
    while(p != NULL) {
        if (p->free && ((p->size - sizeof(meta)) > s)) {
            if (p->size >= (s + 2 * sizeof(meta))) {
                split_block(p, s + sizeof(meta));
            }
            p->free = 0;
            if (p->next_free) { p->next_free->prev_free = p->prev_free; }
            if (p->prev_free) { p->prev_free->next_free = p->next_free; }
            if (free_head == p) { free_head = p->next_free; }
            if (free_tail == p) { free_tail = p->prev_free; }
            return p;
        }
        p = p->next_free;
    }
    return p;
}

//  Finds the best available free block
/*  WROTE THIS EARLY, DIDN'T UPDATE IT AFTER I CHANGED OTHER PARTS.  JUST USE FIRST_FIT.
meta *best_fit(size_t s) {
    meta *p = free_head;
    size_t best_dif = 2.5 * (size_t)G;
    meta *best = NULL;
    while (p) {
        if (p->size >= s + sizeof(meta)) {
            if (p->size - (s + sizeof(meta)) < best_dif) {
                best_dif = p->size - (s + sizeof(meta));
                best = p;
            }
        }
        p = p->next_free;
    }
    return best;
}
*/


// Determines how much memory to allocate based on the size requested
size_t sbrk_value(size_t size) {
    if (head == NULL) { return size + sizeof(meta); }
    if (head->next == NULL) { return size + sizeof(meta); }
    if (heap_s == 0) { heap_s = (int)sbrk(0); heap_e = heap_s; }
    if ( heap_e + 50 * M - heap_s < 2.5 * G && 50 * M > (size + sizeof(meta))) { heap_e += (50 * M); return (50 * M); }
    heap_e += (size + sizeof(meta));
    return size + sizeof(meta);
}

// Returns the first memory block in the list with size >= s
// Splits the block if size >= 2 * s + 4 * sizeof(meta)
// If there is none, it will call sbrk and create a block
// Returns NULL if sbrk fails
meta *get_block(size_t s) {
    //meta *p = head;
    meta *p = first_fit(s);
    if (p) return p;
    if (tail) {
        if (tail->free) {
            p = (meta*)sbrk(s + sizeof(meta) - tail->size);
            if ((void*)p == (void*)-1) return NULL;
            tail->free = 0;
            if (tail->next_free) { tail->next_free->prev_free = tail->prev_free; }
            if (tail->prev_free) { tail->prev_free->next_free = tail->next_free; }
            if (free_head == tail) { free_head = tail->next_free; }
            if (free_tail == tail) { free_tail = tail->prev_free; }
            tail->next_free = NULL;
            tail->prev_free = NULL;
            tail->size = s + sizeof(meta);
            return tail;
        }
    }
    size_t sbrk_size = sbrk_value(s);    //2 * sizeof(meta) + 2 * s;
    p = (meta*)sbrk(sbrk_size);
    if ((void*)p == (void*)-1) return NULL;
    init_meta(p, 0, sbrk_size, NULL, tail);
    p->next_free = NULL;
    p->prev_free = NULL;
    if (head == NULL) { head = p; tail = p; }
    else { p->prev = tail; tail->next = p; tail = p; }
    if (sbrk_size > s + 2 * (sizeof(meta))) { split_block(p, s + sizeof(meta)); }
    //init_meta(back(p), 0, sbrk_size, NULL);
    //split_block(p, s + sizeof(meta));
    return p;
}

// Coalesce input p and block left of it if able.  Return address of start
// of new block
meta *coalesce_l(meta *p) {
    if (p == NULL || p == head) return p;
    if ((p->prev)->free) {
        meta *ret = p->prev;
        if (p->next != NULL) p->next->prev = ret;
        if (p == tail) tail = ret;
        init_meta(ret, 1, ret->size + p->size, p->next, ret->prev);
        if (p->next_free) { p->next_free->prev_free = p->prev_free; }
        if (p->prev_free) { p->prev_free->next_free = p->next_free; }
        if (p == free_head) { free_head = p->next_free; }
        if (p == free_tail) { free_tail = p->prev_free; }
        clear_meta(p);
        //init_meta(back(p), 1, front(p - 1)->size + p->size, p->next);
        return ret;
    }
    return p;
}

// Coalesce input p and block right of it if able.  Return address of start
// of new block
meta *coalesce_r(meta *p) {
    if (p == NULL || p->next == NULL) return p;
    if ((p->next)->free) {
        //init_meta(back(back(p) + 1), 1, p->size + (back(p) + 1)->size, (back(p) + 1)->next);
        if (p->next->next != NULL) p->next->next->prev = p;
        if (tail == p->next) tail = p;
        meta *temp = p->next;
        init_meta(p, 1, p->size + (p->next)->size, (p->next)->next, p->prev);
        if (temp->next_free) { temp->next_free->prev_free = temp->prev_free; }
        if (temp->prev_free) { temp->prev_free->next_free = temp->next_free; }
        if (temp == free_head) { free_head = temp->next_free; }
        if (temp == free_tail) { free_tail = temp->prev_free; }
        clear_meta(temp);
    }
    return p;
}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    // implement calloc!
    if (num == 0 || size == 0) return NULL;
    meta *p = get_block(num * size);
    if (p == NULL) return NULL;
    memset(p + 1, 0, p->size - sizeof(meta));
    //memcpy(last_alloc, "calloc", 7);
    //print_list();
    //print_free_list();
    return (void*)(p + 1);
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size) {
    // implement malloc!
    if (size == 0) return NULL;
    meta *p = get_block(size);
    if (p == NULL) return NULL;
    //memcpy(last_alloc, "malloc", 7);
    //print_list();
    //print_free_list();
    return (void*)(p + 1);
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    // implement free!
    if (ptr == NULL) return;
    meta *p = ((meta*)(ptr)) - 1;
    p->free = 1;
    if (free_head == NULL) { free_head = p; free_tail = p; }
    else { p->prev_free = free_tail; p->next_free = NULL; free_tail->next_free = p; free_tail = p; }
    p = coalesce_l(p);
    p = coalesce_r(p);
    //print_list();
    //print_free_list();
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size) {
    // implement realloc!
    if (ptr == NULL) {
        //memcpy(last_alloc, "realloc1", 9);
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        //memcpy(last_alloc, "realloc2", 9);
        return NULL;
    }
    meta *p = ((meta*)(ptr)) - 1; //ptr - sizeof(meta);
    int dif = (int)size - ((int)p->size - (int)sizeof(meta));
    if (dif == 0) {
        //memcpy(last_alloc, "realloc3", 9);
        return ptr;
    }
    if (dif < 0 && dif > (int)(-1 * (int)sizeof(meta) - 1)) {
        //memcpy(last_alloc, "realloc4", 9);
        return ptr;
    }
    if (dif < 0) {
        split_block(p, size + sizeof(meta));
        coalesce_r(p->next);
        //memcpy(last_alloc, "realloc5", 9);
        return ptr;
    }
    else { // TODO: make better
        meta *newp = get_block(size + sizeof(meta));
        void *ret = newp + 1;
        ret = memmove(ret, ptr, p->size - sizeof(meta));
        free(ptr);
        //memcpy(last_alloc, "realloc6", 8);
        return ret;
    }
}
