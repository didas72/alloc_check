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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define DIE do { fprintf(stderr, "alloc_check encountered a fatal error.\n"); exit(72); } while (0)
#define DIE_NULL(ptr) do { if (ptr == NULL) DIE; } while (0)


//===Required structures===
//Implements needed data structures
#define VOIDPTRARR_DEFAULT_CAP 4

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

static void append_voidptr_array(voidptr_array *arr, void *data)
{
	ensure_voidptr_array(arr, arr->capacity + 1);
	arr->data[arr->count++] = data;
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
	size_t id;
	int type;

	void *ptr;
	size_t size;
	char *file_name;
	int line;
} memory_entry;

typedef struct
{
	size_t id_counter;

	//Each alloc, realloc and free entries
	voidptr_array *allocs;
	voidptr_array *reallocs;
	voidptr_array *frees;

	//Index to pointer matching
	voidptr_array *pointers;
	//Entries per index (List<List<entry>>)
	voidptr_array *entry_lookup;
} checker_status;



static checker_status status = { .id_counter = 0, .allocs = NULL, .reallocs = NULL, .frees = NULL, .pointers = NULL, .entry_lookup = NULL };



static void init_checker()
{
	if (status.allocs != NULL) return;

	status.allocs = create_voidptr_array();
	status.reallocs = create_voidptr_array();
	status.frees = create_voidptr_array();
	status.pointers = create_voidptr_array();
	status.entry_lookup = create_voidptr_array();

	//Special null pointer case
	append_voidptr_array(status.pointers, NULL);
	append_voidptr_array(status.entry_lookup, create_voidptr_array());
	status.id_counter = 1;
}

static size_t find_id(void *ptr)
{
	for (size_t i = 0; i < status.pointers->count; i++)
	{
		if (status.pointers->data[i] == ptr)
			return i;
	}

	return 0; //Both NULL and unlisted will be 0
}



memory_entry *create_memory_entry(int type, size_t id, void *ptr, size_t size, char *file_name, int line)
{
	memory_entry *entry = malloc(sizeof(memory_entry));
	DIE_NULL(entry);
	char *name = malloc(strlen(file_name));
	DIE_NULL(name);
	strcpy(name, file_name);

	entry->id = id;
	entry->type = ENTRY_ALLOC;
	entry->ptr = ptr;
	entry->size = size;
	entry->file_name = name;
	entry->line = line;

	return entry;
}

void destroy_memory_entry(memory_entry *entry)
{
	free(entry->file_name);
	free(entry);
}



void *checked_malloc(size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = malloc(size);

	memory_entry *entry = create_memory_entry(ENTRY_ALLOC, status.id_counter++, ptr, size, file_name, line);
	size_t id = entry->id;
	append_voidptr_array(status.allocs, entry); //add to alloc list
	append_voidptr_array(status.pointers, ptr); //add index to pointer matching
	append_voidptr_array(status.entry_lookup, create_voidptr_array()); //create lookup for new id
	append_voidptr_array(status.entry_lookup->data[id], entry); //add first entry

	return ptr;
}

void *checked_realloc(void *ptr, size_t size, char *file_name, int line)
{
	init_checker();

	void *new_ptr = realloc(ptr, size);

	size_t id = find_id(ptr);
	memory_entry *entry = create_memory_entry(ENTRY_REALLOC, id, new_ptr, size, file_name, line);
	append_voidptr_array(status.reallocs, entry);

	//update id to pointer matching, if not null or unlisted
	if (id != 0)
		status.pointers->data[id] = new_ptr;
	append_voidptr_array(status.entry_lookup->data[id], entry);

	return new_ptr;
}

void checked_free(void *ptr, char *file_name, int line)
{
	init_checker();

	free(ptr);

	size_t id = find_id(ptr);
	memory_entry *entry = create_memory_entry(ENTRY_FREE, id, ptr, 0, file_name, line);
	append_voidptr_array(status.frees, entry);
	append_voidptr_array(status.entry_lookup->data[id], entry);
}



void report_alloc_checks()
{
	//Calculate metrics
	size_t allocs = status.allocs->count, reallocs = status.reallocs->count, frees = status.frees->count;
	size_t blocksLost, memoryLost;

	printf("\n\n");
	printf("+========alloc_check report========+\n");
	printf("+--Statistics----------------------+\n");
	printf("|Total allocs/reallocs/frees:      |\n");
	printf("| => % 5ld/% 5ld/% 5ld             |\n", allocs, reallocs, frees);
	printf("|Blocks lost: % 5ld                |\n", blocksLost);
	printf("|Memory lost: % 5ld                |\n", memoryLost);
	printf("+==================================+\n");
}

//Allocs create new id
//Reallocs searches and updates pointer to id
//Frees searches id
//NULL id must always exist

//Report will iterate every id [through entry_lookup], determine final state,
