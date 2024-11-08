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

#include "sus/sus.h"
#include "sus/vector.h"
#include "sus/hashtable.h"
#include "sus/hashes.h"



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

	snprintf(__format_size_buff, 7, "%ld%2s", shown_size % 10000, unit);
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



enum ENTRY_TYPE
{
	ENTRY_NVAL = 0,
	ENTRY_MALLOC = 1,
	ENTRY_CALLOC = 2,
	ENTRY_REALLOC = 3,
	ENTRY_FREE = 4,
};

typedef struct memory_entry
{
	int type;

	void *old_ptr, *new_ptr;
	size_t size;
	size_t tick;
	char *file_name;
	int line;
} memory_entry;

typedef struct checker_status
{
	//Each [m/c]alloc, realloc and free entries
	//vector_t<memory_entry*>
	vector_t *allocs;
	//vector_t<memory_entry*>
	vector_t *reallocs;
	//vector_t<memory_entry*>
	vector_t *frees;

	//Index to pointer matching
	//hashtable_t<void*, vector_t<memory_entry*>>
	hashtable_t *entry_lookup;

	size_t tick;
} checker_status;



static checker_status status = { .allocs = NULL, .reallocs = NULL, .frees = NULL, .entry_lookup = NULL, .tick = 0 };



static void init_checker()
{
	if (status.allocs != NULL) return;

	status.allocs = vector_create();
	status.reallocs = vector_create();
	status.frees = vector_create();
	status.entry_lookup = hashtable_create(hash_ptr, compare_ptr);

	//Special null pointer case
	hashtable_add(status.entry_lookup, NULL, vector_create());
}



memory_entry *create_memory_entry(int type, void *old_ptr, void *new_ptr, size_t size, char *file_name, int line)
{
	memory_entry *entry = malloc(sizeof(memory_entry));
	DIE_NULL(entry);
	char *name = malloc(strlen(file_name) + 1);
	DIE_NULL(name);
	strcpy(name, file_name);

	entry->type = type;
	entry->old_ptr = old_ptr;
	entry->new_ptr = new_ptr;
	entry->size = size;
	entry->file_name = name;
	entry->line = line;
	entry->tick = ++status.tick;

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



//TODO: Review need/location
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
void *checked_malloc(size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = malloc(size);

	//REVIEW: Pointer reuse after free handling?
	vector_t *entry_vec = vector_create();
	memory_entry *entry = create_memory_entry(ENTRY_MALLOC, NULL, ptr, size, file_name, line);
	vector_append(entry_vec, entry);
	hashtable_add(status.entry_lookup, ptr, entry_vec);
	vector_append(status.allocs, entry); //add to alloc list

	return ptr;
}

void *checked_calloc(size_t nitems, size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = calloc(nitems, size);

	//REVIEW: Pointer reuse after free handling?
	//REVIEW: Move logic to separate function to reuse for malloc/calloc
	vector_t *entry_vec = vector_create();
	memory_entry *entry = create_memory_entry(ENTRY_CALLOC, NULL, ptr, size, file_name, line);
	vector_append(entry_vec, entry);
	hashtable_add(status.entry_lookup, ptr, entry_vec);
	vector_append(status.allocs, entry); //add to alloc list

	return ptr;
}

void *checked_realloc(void *ptr, size_t size, char *file_name, int line)
{
	init_checker();

	void *new_ptr = realloc(ptr, size);

	memory_entry *entry = create_memory_entry(ENTRY_REALLOC, ptr, new_ptr, size, file_name, line);

	if (!ptr)
	{
		vector_t *null_entries = hashtable_get(status.entry_lookup, NULL); //Hopefully won't ever return NULL
		vector_append(null_entries, entry);
		return new_ptr;
	}

	vector_append(status.reallocs, entry);

	vector_t *pointer_entries;
	
	if (!new_ptr) //if returned NULL, keep pointer to check for future frees
	{
		pointer_entries = hashtable_get(status.entry_lookup, ptr);
		vector_append(pointer_entries, entry);
		return new_ptr;
	}
	if (hashtable_remove(status.entry_lookup, ptr, NULL, (void**)&pointer_entries) != SUS_SUCCESS)
	{
		//How did we even get here?
		pointer_entries = vector_create();
		fprintf(stderr, "ALLOC_CHECK WARN: checked_realloc received ptr not used before. This might be a problem with your code or with the library. Please analyze the report carefully and send it to the developer if you believe this is a library problem.\n");
	}
	vector_append(pointer_entries, entry);
	hashtable_add(status.entry_lookup, new_ptr, pointer_entries);

	return new_ptr;
}

void checked_free(void *ptr, char *file_name, int line)
{
	init_checker();

	free(ptr);

	memory_entry *entry = create_memory_entry(ENTRY_FREE, ptr, NULL, 0, file_name, line);
	vector_append(status.frees, entry);

	vector_t *pointer_entries = hashtable_get(status.entry_lookup, ptr);
	vector_append(pointer_entries, entry);

	//In most cases, block won't be touched after free, so we can trim to reduce memory usage
	vector_trim(pointer_entries);
}
#pragma GCC diagnostic pop



static vector_t *find_lost_blocks(size_t *total_size)
{
	size_t size = 0;

	vector_t *blocks = vector_create();
	vector_t *entry_lists = hashtable_list_contents(status.entry_lookup);

	for (size_t i = 0; i < entry_lists->count; i++)
	{
		//Skip NULL entry list
		if (entry_lists->count)
			if (!((memory_entry*)entry_lists->data[0])->old_ptr)
				continue;

		char freed = 0;
		size_t last_size = 0;
		vector_t *current_block = entry_lists->data[i];

		for (size_t j = 0; j < current_block->count; j++)
		{
			memory_entry *current_entry = current_block->data[j];
			last_size = current_entry->size;

			if (current_entry->type == ENTRY_FREE)
			{
				freed = 1;
				break;
			}
		}

		if (!freed)
		{
			vector_append(blocks, current_block);
			size += last_size;
		}
	}

	*total_size = size;
	vector_destroy(entry_lists);

	return blocks;
}
static void print_missing_frees(vector_t *lost_blocks)
{
	if (lost_blocks->count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No missing frees.                                                    |\n");
		return;
	}

	for (size_t i = 0; i < lost_blocks->count; i++)
	{
		vector_t *entries = lost_blocks->data[i];
		memory_entry *entry = entries->data[entries->count - 1];

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld: %-6s, has %-5ld entries:                              |\n", i, format_size(entry->size), entries->count); //REVIEW: Block number

		set_color(COLOR_RED, COLOR_DEFAULT, 0);
		for (size_t j = 0; j < entries->count; j++)
		{
			entry = entries->data[j];
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
		}
	}
}

static void find_zero_re_allocs(vector_t **alloc_vector, vector_t **realloc_vector)
{
	vector_t *allocs = vector_create();
	vector_t *reallocs = vector_create();
	vector_t *entry_lists = hashtable_list_contents(status.entry_lookup);

	for (size_t i = 0; i < entry_lists->count; i++)
	{
		vector_t *current_block = entry_lists->data[i];

		for (size_t j = 0; j < current_block->count; j++)
		{
			memory_entry *current_entry = current_block->data[j];

			if ((current_entry->type == ENTRY_MALLOC || current_entry->type == ENTRY_CALLOC) && current_entry->size == 0)
			{
				vector_append(allocs, current_block);
				break;
			}
			else if (current_entry->type == ENTRY_REALLOC && current_entry->size == 0)
			{
				vector_append(reallocs, current_block);
				break;
			}
		}
	}

	vector_destroy(entry_lists);

	*alloc_vector = allocs;
	*realloc_vector = reallocs;
}
static void print_zero_allocs(vector_t *alloc_vector)
{
	if (alloc_vector->count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No zero-sized allocs.                                                |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Zero-sized allocs===                                              |\n");

	for (size_t i = 0; i < alloc_vector->count; i++)
	{
		vector_t *entries = alloc_vector->data[i];
		memory_entry *entry;

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", i, entries->count);  //REVIEW: Block number

		for (size_t j = 0; j < entries->count; j++)
		{
			entry = entries->data[j];
			if ((entry->type == ENTRY_MALLOC || entry->type == ENTRY_CALLOC) && entry->size == 0)
			{
				set_color(COLOR_RED, COLOR_DEFAULT, 0);
				printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
			}
			else
			{
				set_color(COLOR_CYAN, COLOR_DEFAULT, 0);
				printf("| -> %-7s %6s @%-18p at %-25s   |\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
			}
		}
	}
}
static void print_zero_reallocs(vector_t *realloc_vector)
{
	if (realloc_vector->count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No zero-sized reallocs.                                              |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Zero-sized reallocs===                                            |\n");

	for (size_t i = 0; i < realloc_vector->count; i++)
	{
		vector_t *entries = realloc_vector->data[i];
		memory_entry *entry;

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", i, entries->count);  //REVIEW: Block number

		for (size_t j = 0; j < entries->count; j++)
		{
			entry = entries->data[j];
			if (entry->type == ENTRY_REALLOC && entry->size == 0)
			{
				set_color(COLOR_RED, COLOR_DEFAULT, 0);
				printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->old_ptr, format_file_line(entry->file_name, entry->line));
			}
			else
			{
				set_color(COLOR_CYAN, COLOR_DEFAULT, 0);
				printf("| -> %-7s %6s @%-18p at %-25s   |\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
			}
		}
	}
}

static void find_failed_re_allocs(vector_t **failed_reallocs, size_t *failed_allocs)
{
	//REVIEW: Ignore zero-sized ops that return NULL, shown separately

	vector_t *reallocs = vector_create();
	size_t allocc = 0;

	vector_t *null_block = status.entry_lookup->data[0]; //TODO: libsus implement hashable find

	for (size_t i = 0; i < null_block->count; i++)
	{
		memory_entry *entry = null_block->data[i];

		if ((entry->type == ENTRY_MALLOC || entry->type == ENTRY_CALLOC) && entry->size != 0) allocc++;
	}

	vector_t *entry_lists = hashtable_list_contents(status.entry_lookup);

	for (size_t i = 0; i < entry_lists->count; i++)
	{
		vector_t *cur_block =  entry_lists->data[i];

		for (size_t j = 0; j < cur_block->count; j++)
		{
			memory_entry *entry = cur_block->data[j];

			if (entry->type == ENTRY_REALLOC && entry->size != 0 && entry->new_ptr == NULL)
			{
				vector_append(reallocs, cur_block);
				break;
			}
		}
	}

	vector_destroy(entry_lists);

	*failed_allocs = allocc;
	*failed_reallocs = reallocs;
}
static void print_failed_allocs(size_t failed_allocs)
{
	if (failed_allocs == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No failed allocs.                                                    |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Failed allocs===                                                  |\n");

	vector_t *null_block = status.entry_lookup->data[0]; //TODO: libsus implement hashable find

	set_color(COLOR_RED, COLOR_DEFAULT, 0);
	for (size_t i = 0; i < null_block->count; i++)
	{
		memory_entry *entry = null_block->data[i];

		if ((entry->type == ENTRY_MALLOC || entry->type == ENTRY_CALLOC) && entry->size != 0)
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
	}
}
static void print_failed_reallocs(vector_t *failed_reallocs)
{
	if (failed_reallocs->count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No failed reallocs.                                                  |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Failed reallocs===                                                |\n");

	for (size_t i = 0; i < failed_reallocs->count; i++)
	{
		vector_t *entries = failed_reallocs->data[i];
		memory_entry *entry;

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", i, entries->count);

		for (size_t j = 0; j < entries->count; j++)
		{
			entry = entries->data[j];
			if (entry->type == ENTRY_REALLOC && entry->size != 0 && entry->new_ptr == NULL)
			{
				set_color(COLOR_RED, COLOR_DEFAULT, 0);
				printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->old_ptr, format_file_line(entry->file_name, entry->line));
			}
			else
			{
				set_color(COLOR_CYAN, COLOR_DEFAULT, 0);
				printf("| -> %-7s %6s @%-18p at %-25s   |\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
			}
		}
	}
}

static void find_null_reallocs_frees(size_t *null_reallocs, size_t *null_frees)
{
	size_t reallocc = 0, freec = 0;

	voidptr_array *null_block = status.entry_lookup->data[0];

	for (size_t i = 0; i < null_block->count; i++)
	{
		memory_entry *entry = null_block->data[i];

		if (entry->type == ENTRY_FREE) freec++;
		else if (entry->type == ENTRY_REALLOC) reallocc++;
	}

	*null_reallocs = reallocc;
	*null_frees = freec;
}
static void print_null_reallocs(size_t null_reallocs)
{
	if (null_reallocs == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No NULL reallocs.                                                    |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===NULL reallocs===                                                  |\n");

	voidptr_array *null_block = status.entry_lookup->data[0];

	set_color(COLOR_RED, COLOR_DEFAULT, 0);
	for (size_t i = 0; i < null_block->count; i++)
	{
		memory_entry *entry = null_block->data[i];

		if (entry->type == ENTRY_REALLOC && entry->old_ptr == NULL)
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->old_ptr, format_file_line(entry->file_name, entry->line));
	}
}
static void print_null_frees(size_t null_frees)
{
	if (null_frees == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No NULL frees.                                                       |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===NULL frees===                                                     |\n");

	voidptr_array *null_block = status.entry_lookup->data[0];

	set_color(COLOR_RED, COLOR_DEFAULT, 0);
	for (size_t i = 0; i < null_block->count; i++)
	{
		memory_entry *entry = null_block->data[i];

		if (entry->type == ENTRY_FREE && entry->old_ptr == NULL)
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->old_ptr, format_file_line(entry->file_name, entry->line));
	}
}

static void print_all_allocs()
{
	if (status.allocs->count == 0)
	{
		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("| No (c)allocs.                                                        |\n");
		return;
	}

	for (size_t i = 0; i < status.allocs->count; i++)
	{
		memory_entry *entry = status.allocs->data[i];

		if (entry->new_ptr == NULL) 
			set_color(COLOR_RED, COLOR_DEFAULT, 0);
		else if (entry->size == 0)
			set_color(COLOR_DARK_YELLOW, COLOR_DEFAULT, 0);
		else
			set_color(COLOR_GREEN, COLOR_DEFAULT, 0);

		printf("| %4ld %-7s %6s @%-18p at %-25s |\n", i, entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
	}
}
static void print_all_reallocs()
{
	if (status.reallocs->count == 0)
	{
		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("| No reallocs.                                                         |\n");
		return;
	}
	
	for (size_t i = 0; i < status.reallocs->count; i++)
	{
		memory_entry *entry = status.reallocs->data[i];

		if (entry->old_ptr == NULL) 
			set_color(COLOR_RED, COLOR_DEFAULT, 0);
		else if (entry->size == 0 || entry->new_ptr == NULL)
			set_color(COLOR_DARK_YELLOW, COLOR_DEFAULT, 0);
		else
			set_color(COLOR_GREEN, COLOR_DEFAULT, 0);

		printf("| %4ld %-7s %6s @%-18p at %-25s |\n", i, entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
	}
}
static void print_all_frees()
{
	if (status.frees->count == 0)
	{
		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("| No frees.                                                           |\n");
		return;
	}

	for (size_t i = 0; i < status.frees->count; i++)
	{
		memory_entry *entry = status.frees->data[i];

		if (entry->old_ptr == NULL) 
			set_color(COLOR_RED, COLOR_DEFAULT, 0);
		else
			set_color(COLOR_GREEN, COLOR_DEFAULT, 0);

		printf("| %4ld %-7s @%-18p at %-25s        |\n", i, entry_type_str(entry->type), entry->old_ptr, format_file_line(entry->file_name, entry->line));
	}
}



void report_alloc_checks()
{
	init_checker();

	//Calculate metrics
	size_t allocs = status.allocs->count;
	size_t reallocs = status.reallocs->count;
	size_t frees = status.frees->count;

	size_t memory_lost;
	vector_t *lost_blocks = find_lost_blocks(&memory_lost);

	vector_t *zero_allocs, *zero_reallocs;
	find_zero_re_allocs(&zero_allocs, &zero_reallocs);
	
	size_t failed_allocs, failed_reallocs, *failed_reallocs_v;
	find_failed_re_allocs(&failed_reallocs_v, &failed_allocs, &failed_reallocs);

	size_t null_reallocs, null_frees;
	find_null_reallocs_frees(&null_reallocs, &null_frees);

	//Internally 70 cols wide (72 external)
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("\n\n");
	printf("+==========================alloc_check report==========================+\n");
	printf("+--Statistics----------------------------------------------------------+\n");
	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("|Total allocs/reallocs/frees: %-5ld/%-5ld/%-5ld                        |\n", allocs, reallocs, frees);
	printf("|Total blocks/memory lost: %-5ld/~%-6s                               |\n", lost_blocks->count, format_size(memory_lost));
	printf("|Total zero-sized allocs/reallocs: %-5ld/%-5ld                         |\n", zero_allocs->count, zero_reallocs->count);
	printf("|Total failed allocs/reallocs: %-5ld/%-5ld                             |\n", failed_allocs, failed_reallocs);
	printf("|Total NULL reallocs/frees: %-5ld/%-5ld                                |\n", null_reallocs, null_frees);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Missing frees-------------------------------------------------------+\n");
	print_missing_frees(lost_blocks);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Invalid operations--------------------------------------------------+\n");
	print_zero_allocs(zero_allocs);
	print_zero_reallocs(zero_reallocs);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Failed (re)allocations----------------------------------------------+\n");
	print_failed_allocs(failed_allocs);
	print_failed_reallocs(failed_reallocs_v, failed_reallocs);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Possible mistakes---------------------------------------------------+\n");
	print_null_reallocs(null_reallocs);
	print_null_frees(null_frees);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+======================================================================+\n");
	set_color(COLOR_DEFAULT, COLOR_DEFAULT, 0);

	vector_destroy(lost_blocks);
	vector_destroy(zero_allocs);
	vector_destroy(zero_reallocs);
	free(failed_reallocs_v);
}

void list_all_entries()
{
	init_checker();

	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("\n\n");
	printf("+========================alloc_check entry list========================+\n");
	printf("+--[C]Allocs-----------------------------------------------------------+\n");
	print_all_allocs();
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Reallocs------------------------------------------------------------+\n");
	print_all_reallocs();
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Frees---------------------------------------------------------------+\n");
	print_all_frees();
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+======================================================================+\n");
	set_color(COLOR_DEFAULT, COLOR_DEFAULT, 0);
	
}

void cleanup_alloc_checks()
{
	for (size_t i = 0; i < status.allocs->count; i++)
		destroy_memory_entry(status.allocs->data[i]);

	for (size_t i = 0; i < status.reallocs->count; i++)
		destroy_memory_entry(status.reallocs->data[i]);

	for (size_t i = 0; i < status.frees->count; i++)
		destroy_memory_entry(status.frees->data[i]);

	vector_destroy(status.allocs);
	vector_destroy(status.reallocs);
	vector_destroy(status.frees);
	hashtable_destroy(status.entry_lookup);

	status.allocs = NULL;
	status.reallocs = NULL;
	status.frees = NULL;
	status.entry_lookup = NULL;
	status.tick = 0;
}
