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



enum TERM_COLOR
{
	COLOR_DEFAULT = 39,
	COLOR_BLACK = 30,
	COLOR_DARK_RED= 31,
	COLOR_DARK_GREEN = 32,
	COLOR_DARK_YELLOW = 33,
	COLOR_DARK_BLUE = 34,
	COLOR_DARK_MAGENTA = 35,
	COLOR_DARK_CYAN = 36,
	COLOR_LIGHT_GRAY = 37,
	COLOR_DARK_GRAY = 90,
	COLOR_RED = 91,
	COLOR_GREEN = 92,
	COLOR_ORANGE = 93,
	COLOR_BLUE = 94,
	COLOR_MAGENTA = 95,
	COLOR_CYAN = 96,
	COLOR_WHITE = 97,
};

static void set_color(int fg, int bg, char bold)
{
	printf("\033[%d;%dm\033[%dm", bold, fg, bg + 10);
}



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
static char __format_size_buff[6+1];
static char *format_size(size_t size)
{
	char *unit = "!?";
	size_t shown_size = 99999;

	if (size > 0x20000000000000)
	{
		unit = "PB";
		shown_size = size >> 50;
	}
	else if (size > 0x80000000000)
	{
		unit = "TB";
		shown_size = size >> 40;
	}
	else if (size > 0x200000000)
	{
		unit = "GB";
		shown_size = size >> 30;
	}
	else if (size > 0x800000)
	{
		unit = "MB";
		shown_size = size >> 20;
	}
	else if (size > 0x10000)
	{
		unit = "kB";
		shown_size = size >> 10;
	}
	else
	{
		snprintf(__format_size_buff, 7, "%ldB", size);
		return __format_size_buff;
	}

	snprintf(__format_size_buff, 7, "%ld%s", shown_size, unit);
	return __format_size_buff;
}
static char __format_file_line_buff[25+1];
static char *format_file_line(char *file_name, int line)
{
	size_t file_name_len = strlen(file_name);

	if (file_name_len <= 20)
		snprintf(__format_file_line_buff, 25, "%s:%d", file_name, line);
	else
		snprintf(__format_file_line_buff, 25, "%.17s...:%d", file_name, line);

	return __format_file_line_buff;
}
#pragma GCC diagnostic pop


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
	ENTRY_MALLOC = 1,
	ENTRY_CALLOC = 2,
	ENTRY_REALLOC = 3,
	ENTRY_FREE = 4,
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

	//Each [m/c]alloc, realloc and free entries
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

char *entry_type_str(int type)
{
	if (type == 1) return "MALLOC";
	if (type == 2) return "CALLOC";
	if (type == 3) return "REALLOC";
	if (type == 4) return "FREE";
	return "";
}



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
void *checked_malloc(size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = malloc(size);

	memory_entry *entry;
	size_t id;

	if (ptr == NULL)
	{
		id = 0;
		entry = create_memory_entry(ENTRY_MALLOC, id, ptr, size, file_name, line);
	}
	else
	{
		id = status.id_counter++;
		entry = create_memory_entry(ENTRY_MALLOC, id, ptr, size, file_name, line);
		append_voidptr_array(status.pointers, ptr); //add index to pointer matching
		append_voidptr_array(status.entry_lookup, create_voidptr_array()); //create lookup for new id
	}
	append_voidptr_array(status.allocs, entry); //add to alloc list
	append_voidptr_array(status.entry_lookup->data[id], entry); //add first entry

	return ptr;
}

void *checked_calloc(size_t nitems, size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = malloc(size);

	memory_entry *entry;
	size_t id;

	if (ptr == NULL)
	{
		id = 0;
		entry = create_memory_entry(ENTRY_CALLOC, id, ptr, size, file_name, line);
	}
	else
	{
		id = status.id_counter++;
		entry = create_memory_entry(ENTRY_CALLOC, id, ptr, nitems * size, file_name, line);
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
	for (size_t i = 1, head = 0; i < status.entry_lookup->count && head < blockc; i++)
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
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No missing frees.                                                    |\n");
		return;
	}

	for (size_t i = 0; i < block_count; i++)
	{
		size_t block = block_array[i];
		voidptr_array *entries = status.entry_lookup->data[block];
		memory_entry *entry = entries->data[entries->count - 1];

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld: %-6s, has %-5ld entries:                              |\n", block, format_size(entry->size), entries->count);

		set_color(COLOR_CYAN, COLOR_DEFAULT, 0);
		for (size_t j = 0; j < entries->count; j++)
		{
			entry = entries->data[j];
			printf("| -> %-7s %6s @%-18p at %-25s   |\n", entry_type_str(entry->type), format_size(entry->size), entry->ptr, format_file_line(entry->file_name, entry->line));
		}
	}
}

static void find_zero_re_allocs(size_t **alloc_array, size_t **realloc_array, size_t *zero_alloc_count, size_t *zero_realloc_count)
{
	size_t *allocv = NULL, *reallocv = NULL;
	size_t allocc = 0, reallocc = 0;

	//Perform two passes
	//One for counting and one for storing ids

	//Skip NULL, count NULL zero-sized allocs as failed
	for (size_t i = 1; i < status.entry_lookup->count; i++)
	{
		voidptr_array *current_block = status.entry_lookup->data[i];

		for (size_t j = 0; j < current_block->count; j++)
		{
			memory_entry *current_entry = current_block->data[j];

			if ((current_entry->type == ENTRY_MALLOC || current_entry->type == ENTRY_CALLOC) && current_entry->size == 0)
			{
				allocc++;
				break;
				//If alloc returned NULL, has no more entries
				//Otherwise, they are needed but id will be kept
			}
			else if (current_entry->type == ENTRY_REALLOC && current_entry->size == 0)
			{
				reallocc++;
				break;
				//If realloc returned NULL, has no more entries
				//Otherwise, they are needed but id will be kept
			}
		}
	}

	allocv = malloc(allocc * sizeof(size_t));
	DIE_NULL(allocv);
	reallocv = malloc(reallocc * sizeof(size_t));
	DIE_NULL(reallocv);

	//Skip NULL, count NULL zero-sized allocs as failed
	for (size_t i = 1, ahead = 0, rhead = 0; i < status.entry_lookup->count && (ahead < allocc || rhead < reallocc); i++)
	{
		voidptr_array *current_block = status.entry_lookup->data[i];

		for (size_t j = 0; j < current_block->count; j++)
		{
			memory_entry *current_entry = current_block->data[j];

			if ((current_entry->type == ENTRY_MALLOC || current_entry->type == ENTRY_CALLOC) && current_entry->size == 0)
			{
				allocv[ahead++] = i;
				break;
			}
			else if (current_entry->type == ENTRY_REALLOC && current_entry->size == 0)
			{
				reallocv[rhead++] = i;
				break;
			}
		}
	}

	*alloc_array = allocv;
	*realloc_array = reallocv;
	*zero_alloc_count = allocc;
	*zero_realloc_count = reallocc;
}
static void print_zero_allocs(size_t *block_array, size_t zero_alloc_count)
{
	if (zero_alloc_count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No zero-sized allocs.                                                |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Zero-sized allocs===                                              |\n");

	for (size_t i = 0; i < zero_alloc_count; i++)
	{
		size_t block = block_array[i];
		voidptr_array *entries = status.entry_lookup->data[block];
		memory_entry *entry;

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", block, entries->count);

		for (size_t j = 0; j < entries->count; j++)
		{
			entry = entries->data[j];
			if ((entry->type == ENTRY_MALLOC || entry->type == ENTRY_CALLOC) && entry->size == 0)
			{
				set_color(COLOR_RED, COLOR_DEFAULT, 0);
				printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->ptr, format_file_line(entry->file_name, entry->line));
			}
			else
			{
				set_color(COLOR_CYAN, COLOR_DEFAULT, 0);
				printf("| -> %-7s %6s @%-18p at %-25s   |\n", entry_type_str(entry->type), format_size(entry->size), entry->ptr, format_file_line(entry->file_name, entry->line));
			}
		}
	}
}
static void print_zero_reallocs(size_t *block_array, size_t zero_realloc_count)
{
	if (zero_realloc_count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No zero-sized reallocs.                                              |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Zero-sized reallocs===                                            |\n");

	for (size_t i = 0; i < zero_realloc_count; i++)
	{
		size_t block = block_array[i];
		voidptr_array *entries = status.entry_lookup->data[block];
		memory_entry *entry;

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", block, entries->count);

		for (size_t j = 0; j < entries->count; j++)
		{
			entry = entries->data[j];
			if (entry->type == ENTRY_REALLOC && entry->size == 0)
			{
				set_color(COLOR_RED, COLOR_DEFAULT, 0);
				printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->ptr, format_file_line(entry->file_name, entry->line));
			}
			else
			{
				set_color(COLOR_CYAN, COLOR_DEFAULT, 0);
				printf("| -> %-7s %6s @%-18p at %-25s   |\n", entry_type_str(entry->type), format_size(entry->size), entry->ptr, format_file_line(entry->file_name, entry->line));
			}
		}
	}
}

static void find_invalid_reallocs_frees(size_t *invalid_reallocs, size_t *invalid_frees)
{
	size_t reallocs = 0;
	size_t frees = 0;
	voidptr_array *null_block = status.entry_lookup->data[0];

	for (size_t i = 0; i < null_block->count; i++)
	{
		memory_entry *entry = null_block->data[i];

		if (entry->type == ENTRY_REALLOC) reallocs++;
		else if (entry->type == ENTRY_FREE) frees++;
	}

	*invalid_reallocs = reallocs;
	*invalid_frees = frees;
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

	size_t zero_allocs, zero_reallocs, *zero_allocs_v, *zero_reallocs_v;
	find_zero_re_allocs(&zero_allocs_v, &zero_reallocs_v, &zero_allocs, &zero_reallocs);
	//TODO: Later print as zero-sized reallocs (use ids to show ops if returned pointer is not NULL)

	//TODO: Find invalid reallocs (id=0)
	//TODO: Count entries
	//TODO: Later print as invalid reallocs
	//TODO: Find invalid frees (id=0)
	//TODO: Count entries
	//TODO: Later print as invalid frees
	size_t invalid_reallocs, invalid_frees;
	find_invalid_reallocs_frees(&invalid_reallocs, &invalid_frees);

	//TODO: Find failed allocs (id=0)
	//TODO: Count entries
	//TODO: Later print as failed allocs
	//TODO: Find failed reallocs (id=0) (keep ids)
	//TODO: Count entries
	//TODO: Later print as failed reallocs (use ids to show alloc and list reallocs)
	//REMINDER: Ignore zero-sized ops that return NULL, shown separately
	//REMINDER: Note NULL frees are technically valid
	size_t failed_allocs = 0, failed_reallocs = 0;

	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	//Internally 70 cols wide (72 external)
	printf("\n\n");
	printf("+=========================alloc_check report===========================+\n");
	printf("+--Statistics----------------------------------------------------------+\n");
	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("|Total allocs/reallocs/frees: %-5ld/%-5ld/%-5ld                        |\n", allocs, reallocs, frees);
	printf("|Total blocks/memory lost: %-5ld/~%-6s                               |\n", blocks_lost, format_size(memory_lost));
	printf("|Total zero-sized allocs/reallocs: %-5ld/%-5ld                         |\n", zero_allocs, zero_reallocs);
	printf("|Total invalid reallocs/frees: %-5ld/%-5ld                             |\n", invalid_reallocs, invalid_frees);
	printf("|Total failed allocs/reallocs: %-5ld/%-5ld                             |\n", failed_allocs, failed_reallocs);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Missing frees-------------------------------------------------------+\n");
	print_missing_frees(lost_blocks_v, blocks_lost);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Invalid operations--------------------------------------------------+\n");
	print_zero_allocs(zero_allocs_v, zero_allocs);
	print_zero_reallocs(zero_reallocs_v, zero_reallocs);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("|                        ===NOT  IMPLEMENTED===                        |\n");
	//TODO: List invalid reallocs
	//TODO: List invalid frees
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Failed (re)allocations----------------------------------------------+\n");
	printf("|                        ===NOT  IMPLEMENTED===                        |\n");
	//TODO: List failed allocs
	//TODO: List failed reallocs
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+======================================================================+\n");
	set_color(COLOR_DEFAULT, COLOR_DEFAULT, 0);

	free(lost_blocks_v);
	free(zero_allocs_v);
	free(zero_reallocs_v);
}
