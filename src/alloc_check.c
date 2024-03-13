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
	while (arr->capacity < capacity) arr->capacity <<= 1;

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
	char *name = malloc(strlen(file_name) + 1);
	DIE_NULL(name);
	strcpy(name, file_name);

	entry->id = id;
	entry->type = type;
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



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
void *checked_malloc(size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = malloc(size);

	memory_entry *entry;
	size_t id;

	if (!ptr)
	{
		id = 0;
		entry = create_memory_entry(ENTRY_ALLOC, id, ptr, size, file_name, line);
	}
	else
	{
		id = status.id_counter++;
		entry = create_memory_entry(ENTRY_ALLOC, id, ptr, size, file_name, line);
		append_voidptr_array(status.pointers, ptr); //add index to pointer matching
		append_voidptr_array(status.entry_lookup, create_voidptr_array()); //create lookup for new id
	}
	append_voidptr_array(status.allocs, entry); //add to alloc list
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
#pragma GCC diagnostic pop




static void find_lost_blocks(size_t **block_array, size_t *block_count, size_t *total_size)
{
	size_t *blockv = NULL;
	size_t blockc = 0;
	size_t size = 0;

	//Perform two passes
	//One for counting and one for storing ids

	//Skip id=0 (NULL/invalid)
	for (size_t i = 1; i < status.entry_lookup->count; i++)
	{
		char freed = 0;
		size_t last_size = 0;
		voidptr_array *current_block = status.entry_lookup->data[i];

		for (size_t j = 0; j < current_block->count; j++)
		{
			memory_entry *current_entry = current_block->data[j];
			last_size = current_entry->size;

			if (current_entry->type == ENTRY_FREE)
			{
				freed = 1;
				break; //REVIEW: Break needed? should be last entry
			}
		}

		if (!freed)
		{
			blockc++;
			size += last_size;
		}
	}

	blockv = malloc(blockc * sizeof(size_t));
	DIE_NULL(blockv);

	//Skip id=0 (NULL/invalid)
	for (size_t i = 1, head = 0; i < status.entry_lookup->count; i++)
	{
		char freed = 0;
		voidptr_array *current_block = status.entry_lookup->data[i];

		for (size_t j = 0; j < current_block->count; j++)
		{
			memory_entry *current_entry = current_block->data[j];

			if (current_entry->type == ENTRY_FREE)
			{
				freed = 1;
				break; //REVIEW: Break needed? should be last entry
			}
		}

		if (!freed)
		{
			blockv[head++] = i;
		}
	}

	*block_array = blockv;
	*block_count = blockc;
	*total_size = size;
}
static void print_missing_frees(size_t *block_array, size_t block_count)
{
	if (block_count == 0)
	{
		printf("| No missing frees                                 |\n");
		return;
	}

	//TODO: Later print as missing frees (use ids to list reallocs)

	for (size_t i = 0; i < block_count; i++)
	{
		voidptr_array *entries = status.entry_lookup->data[i];
		printf("%p\n", entries);
		memory_entry *last_entry = entries->data[entries->count - 1];//FIXME: reading invalid pointers
		printf("%p\n", last_entry);
		size_t block_size = last_entry->size;

		printf("|#%05ld: %05ld%3s, %05ld entries:                  |\n", i, block_size, "B", entries->count);
	}
}



void report_alloc_checks()
{
	init_checker();

	//Calculate metrics
	size_t allocs = status.allocs->count;
	size_t reallocs = status.reallocs->count;
	size_t frees = status.frees->count;

	size_t blocks_lost, memory_lost, *lost_blocks_v;
	find_lost_blocks(&lost_blocks_v, &blocks_lost, &memory_lost);

	//TODO: Find zero-sized allocs
	//TODO: Count entries
	//TODO: Later print as zero-sized allocs
	//TODO: Find zero-sized reallocs (keep ids)
	//TODO: Count entries
	//TODO: Later print as zero-sized reallocs (use ids to show alloc and list reallocs)
	size_t zero_allocs = 0, zero_reallocs = 0;

	//TODO: Find invalid reallocs (id=0)
	//TODO: Count entries
	//TODO: Later print as invalid reallocs
	//TODO: Find invalid frees (id=0)
	//TODO: Count entries
	//TODO: Later print as invalid frees
	size_t invalid_reallocs = 0, invalid_frees = 0;

	//TODO: Find failed allocs (id=0)
	//TODO: Count entries
	//TODO: Later print as failed allocs
	//TODO: Find failed reallocs (id=0) (keep ids)
	//TODO: Count entries
	//TODO: Later print as failed reallocs (use ids to show alloc and list reallocs)
	size_t failed_allocs = 0, failed_reallocs = 0;

	//Internally 60 cols wide (62 external)
	//Minimum height is 17 rows (+2 empty)
	printf("\n\n");
	printf("+================alloc_check report================+\n");
	printf("+--Statistics--------------------------------------+\n");
	printf("|Total allocs/reallocs/frees: %05ld/%05ld/%05ld    |\n", allocs, reallocs, frees);
	printf("|Blocks lost: %05ld                                |\n", blocks_lost);
	printf("|Total memory lost: ~%05ld%3s                      |\n", memory_lost >> 10, "kB"); //TODO: Variable unit (B, kB, MB)
	printf("|Total zero-sized allocs/reallocs: %05ld/%05ld     |\n", zero_allocs, zero_reallocs);
	printf("|Total invalid reallocs/frees: %05ld/%05ld         |\n", invalid_reallocs, invalid_frees);
	printf("|Total failed allocs/reallocs: %05ld/%05ld         |\n", failed_allocs, failed_reallocs);
	printf("|Note: Failed (re)allocs may not be your fault. If |\n");
	printf("|      unchecked, will cause trouble.              |\n");
	printf("+--Missing frees-----------------------------------+\n");
	print_missing_frees(lost_blocks_v, blocks_lost);
	printf("+--Invalid operations------------------------------+\n");
	printf("|              ===NOT  IMPLEMENTED===              |\n");
	//TODO: List zero-sized allocs
	//TODO: List zero-sized reallocs
	//TODO: List invalid reallocs
	//TODO: List invalid frees
	printf("|Note: Failed frees might be caused by freeing the |\n");
	printf("|      same memory more than once.                 |\n");
	printf("|Note: Failed reallocs might occur if the returned |\n");
	printf("|      pointers are not kept (not updating ptrs).  |\n");
	printf("+--Failed (re)allocations--------------------------+\n");
	printf("|              ===NOT  IMPLEMENTED===              |\n");
	//TODO: List failed allocs
	//TODO: List failed reallocs
	printf("|Note: Failed (re)allocs may not be your fault. If |\n");
	printf("|      unchecked, will cause trouble.              |\n");
	printf("+==================================================+\n");

	//TODO: Make notes only appear if related section is not empty
}






//Allocs create new id
//Reallocs searches and updates pointer to id
//Frees searches id
//NULL id must always exist

//Report will iterate every id [through entry_lookup], determine final state,
