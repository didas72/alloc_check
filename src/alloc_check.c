/**
 * @file alloc_check.c
 * 
 * @brief Dynamic memory allocation helper
 * 
 * @author Diogo Cruz Diniz
 * Contact: diogo.cruz.diniz@tecnico.ulisboa.pt
 */


//Allow the use of standard alloc, realloc and free
#define ALLOW_STANDARD_MEM
#include "alloc_check.h"

#include <stdlib.h>
#include <stdint.h>


#define DIE exit(72)
#define DIE_NULL(ptr) do { if (ptr == NULL) DIE; } while (0)


//===Required structures===
//Implements needed data structures
#define VOIDPTRARR_DEFAULT_CAP 8

typedef struct
{
	void **data;
	size_t capacity;
	size_t count;
} voidptr_array;

static voidptr_array *create_voidptr_array()
{
	voidptr_array *ret = malloc(sizeof(voidptr_array));
	DIE_NULL(ret);

	ret->data = malloc(VOIDPTRARR_DEFAULT_CAP * sizeof(void *));
	DIE_NULL(ret->data);
	ret->count = 0;
	ret->capacity = VOIDPTRARR_DEFAULT_CAP;

	return ret;
}

static void destroy_voidptr_array(voidptr_array *arr)
{
	free(arr->data);
	free(arr);
}

static void ensure_voidptr_array(voidptr_array *arr, size_t capacity)
{
	if (arr->capacity >= capacity) return;

	if (arr->capacity < VOIDPTRARR_DEFAULT_CAP) arr->capacity = VOIDPTRARR_DEFAULT_CAP;
	while (arr->capacity < capacity) arr->capacity << 1;

	void **tmp = realloc(arr->data, arr->capacity * sizeof(void *));
	DIE_NULL(tmp);

	arr->data = tmp;
}

static void trim_voidptr_array(voidptr_array *arr)
{
	if (arr->count == arr->capacity) return;

	void **tmp = realloc(arr->data, arr->count * sizeof(void *));
	DIE_NULL(tmp);

	arr->data = tmp;
	arr->capacity = arr->count;
}



enum ENTRY_TYPE
{
	ENTRY_NVAL = 0,
	ENTRY_ALLOC,
	ENTRY_REALLOC,
	ENTRY_FREE,
};

typedef struct
{
	uint64_t id;
	int type;

	void *ptr;
	size_t size;
	char *file_name;
	int line;
} memory_entry;

typedef struct
{
	uint64_t id_counter;

	voidptr_array allocs;
	voidptr_array reallocs;
	voidptr_array frees;

	voidptr_array pointers;
	voidptr_array entry_lookup;
} checker_status;



static checker_status status = { .id_counter = 0, .allocs = NULL, .reallocs = NULL, .frees = NULL, .pointers = NULL, .entry_lookup = NULL };


//Allocs create new id
//Reallocs and frees search id
//NULL id must always exist

//Report will iterate every id [through entry_lookup], determine final state,
