/**
 * @file alloc_check.c
 * 
 * @brief Dynamic memory allocation helper
 * 
 * @author Diogo Cruz Diniz
 * Contact: diogo.cruz.diniz@tecnico.ulisboa.pt
 */



//Allow the use of standard alloc, realloc and free
#define USE_STANDARD_MEM
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
	//ivector_t<memory_entry>
	ivector_t *allocs;
	//ivector_t<memory_entry>
	ivector_t *reallocs;
	//ivector_t<memory_entry>
	ivector_t *frees;

	//Index to pointer matching
	//hashtable_t<void*, ivector_t<memory_entry>*>
	hashtable_t *entry_lookup;

	size_t tick;
} checker_status;



static checker_status status = { .allocs = NULL, .reallocs = NULL, .frees = NULL, .entry_lookup = NULL, .tick = 0 };



static void init_checker()
{
	if (status.allocs != NULL) return;

	status.allocs = ivector_create(sizeof(memory_entry));
	status.reallocs = ivector_create(sizeof(memory_entry));
	status.frees = ivector_create(sizeof(memory_entry));
	status.entry_lookup = hashtable_create(hash_ptr, compare_ptr);

	//Special null pointer case
	hashtable_add(status.entry_lookup, NULL, ivector_create(sizeof(memory_entry)));
}



memory_entry create_memory_entry(int type, void *old_ptr, void *new_ptr, size_t size, char *file_name, int line)
{
	memory_entry entry = { 0 };

	entry.type = type;
	entry.old_ptr = old_ptr;
	entry.new_ptr = new_ptr;
	entry.size = size;
	entry.file_name = file_name;
	entry.line = line;
	entry.tick = ++status.tick;

	return entry;
}

char *entry_type_str(int type)
{
	if (type == 1) return "MALLOC";
	if (type == 2) return "CALLOC";
	if (type == 3) return "REALLOC";
	if (type == 4) return "FREE";
	return "???";
}



void *checked_malloc(size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = malloc(size);

	//REVIEW: Pointer reuse after free handling?
	//ivector_t<memory_entry>
	ivector_t *entry_vec = ivector_create(sizeof(memory_entry));
	memory_entry entry = create_memory_entry(ENTRY_MALLOC, NULL, ptr, size, file_name, line);
	ivector_append(entry_vec, &entry);
	hashtable_add(status.entry_lookup, ptr, entry_vec);
	ivector_append(status.allocs, &entry); //Add to alloc list

	return ptr;
}

void *checked_calloc(size_t nitems, size_t size, char *file_name, int line)
{
	init_checker();

	void *ptr = calloc(nitems, size);

	//REVIEW: Pointer reuse after free handling?
	//REVIEW: Move logic to separate function to reuse for malloc/calloc
	//ivector_t<memory_entry>
	ivector_t *entry_vec = ivector_create(sizeof(memory_entry));
	memory_entry entry = create_memory_entry(ENTRY_CALLOC, NULL, ptr, size, file_name, line);
	ivector_append(entry_vec, &entry);
	hashtable_add(status.entry_lookup, ptr, entry_vec);
	ivector_append(status.allocs, &entry); //Add to alloc list

	return ptr;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
void *checked_realloc(void *ptr, size_t size, char *file_name, int line)
{
	init_checker();

	void *new_ptr = realloc(ptr, size);
	
	memory_entry entry = create_memory_entry(ENTRY_REALLOC, ptr, new_ptr, size, file_name, line);

	if (!ptr)
	{
		ivector_t *null_entries = hashtable_get(status.entry_lookup, NULL); //Hopefully won't ever return NULL
		ivector_append(null_entries, &entry);
		return new_ptr;
	}

	ivector_append(status.reallocs, &entry);

	//ivector_t<memory_entry>
	ivector_t *pointer_entries;
	
	if (!new_ptr) //if returned NULL, keep pointer to check for future frees
	{
		pointer_entries = hashtable_get(status.entry_lookup, ptr);
		ivector_append(pointer_entries, &entry);
		return new_ptr;
	}
	if (hashtable_remove(status.entry_lookup, ptr, NULL, (void**)&pointer_entries) != SUS_SUCCESS)
	{
		//How did we even get here?
		pointer_entries = ivector_create(sizeof(memory_entry));
		fprintf(stderr, "ALLOC_CHECK WARN: checked_realloc received ptr not used before. This might be a problem with your code or with the library. Please analyze the report carefully and send it to the developer if you believe this is a library problem.\n");
	}
	ivector_append(pointer_entries, &entry);
	hashtable_add(status.entry_lookup, new_ptr, pointer_entries);

	return new_ptr;
}

void checked_free(void *ptr, char *file_name, int line)
{
	init_checker();

	free(ptr);

	memory_entry entry = create_memory_entry(ENTRY_FREE, ptr, NULL, 0, file_name, line);
	ivector_append(status.frees, &entry);

	//REVIEW: Free on bad pointer?
	ivector_t *pointer_entries = hashtable_get(status.entry_lookup, ptr);
	ivector_append(pointer_entries, &entry);

	//In most cases, block won't be touched after free, so we can trim to reduce memory usage
	ivector_trim(pointer_entries);
}
#pragma GCC diagnostic pop



/// @brief Find all lost (not freed) blocks
/// @param total_size Pointer to store total lost memory size
/// @return ivector_t<ivector_t<memory_entry>*> containing lost blocks
static ivector_t *find_lost_blocks(size_t *total_size)
{
	size_t size = 0;

	//ivector_t<ivector_t<memory_entry>*>
	ivector_t *blocks = ivector_create(sizeof(ivector_t*));
	//ivector_t<ivector_t<memory_entry>*>
	ivector_t *entry_lists = hashtable_list_contents(status.entry_lookup);

	size_t block_count = ivector_get_count(entry_lists);
	for (size_t i = 0; i < block_count; i++)
	{
		char freed = 0;
		size_t last_size = 0;
		//ivector_t<memory_entry>
		ivector_t *current_block;
		ivector_fetch(entry_lists, i, &current_block);
		size_t entry_count = ivector_get_count(current_block);

		//Skip NULL/empty entry lists
		if (entry_count == 0)
			continue;

		memory_entry *first_entry = ivector_get(current_block, 0);
		if (first_entry->new_ptr == NULL)
			continue;

		for (size_t j = 0; j < entry_count; j++)
		{
			memory_entry *current_entry = ivector_get(current_block, j);
			last_size = current_entry->size;

			if (current_entry->type == ENTRY_FREE)
			{
				freed = 1;
				break;
			}
		}

		if (!freed)
		{
			ivector_append(blocks, &current_block);
			size += last_size;
		}
	}

	*total_size = size;
	ivector_destroy(entry_lists);

	return blocks;
}
/// @brief Prints table for missing frees
/// @param lost_blocks ivector_t<ivector_t<memory_entry>*> containing lost blocks, returned by find_lost_blocks
static void print_missing_frees(ivector_t *lost_blocks)
{
	size_t block_count = ivector_get_count(lost_blocks);
	if (block_count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No missing frees.                                                    |\n");
		return;
	}

	for (size_t i = 0; i < block_count; i++)
	{
		//ivector_t<memory_entry>
		ivector_t *entries;
		ivector_fetch(lost_blocks, i, &entries);
		size_t entry_count = ivector_get_count(entries);
		if (entry_count == 0) continue;
		memory_entry *entry = ivector_get(entries, entry_count - 1);

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld: %-6s, has %-5ld entries:                              |\n", i, format_size(entry->size), entry_count); //REVIEW: Block number

		set_color(COLOR_RED, COLOR_DEFAULT, 0);
		for (size_t j = 0; j < entry_count; j++)
		{
			entry = ivector_get(entries, j);
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
		}
	}
}

/// @brief Finds zero-sized alloc/reallocs
/// @param alloc_vector Pointer to store the ivector_t<ivector_t<memory_entry>*> with zero-sized m/callocs
/// @param realloc_vector Pointer to store the ivector_t<ivector_t<memory_entry>*> with zero-sized reallocs
static void find_zero_re_allocs(ivector_t **alloc_vector, ivector_t **realloc_vector)
{
	//ivector_t<ivector_t<memory_entry>*>
	ivector_t *allocs = ivector_create(sizeof(ivector_t*));
	//ivector_t<ivector_t<memory_entry>*>
	ivector_t *reallocs = ivector_create(sizeof(ivector_t*));
	//ivector_t<ivector_t<memory_entry>*>
	ivector_t *entry_lists = hashtable_list_contents(status.entry_lookup);

	size_t block_count = ivector_get_count(entry_lists);
	for (size_t i = 0; i < block_count; i++)
	{
		//ivector_t<memory_entry>
		ivector_t *current_block;
		ivector_fetch(entry_lists, i, &current_block);

		size_t entry_count = ivector_get_count(current_block);
		for (size_t j = 0; j < entry_count; j++)
		{
			memory_entry *current_entry = ivector_get(current_block, j);

			if ((current_entry->type == ENTRY_MALLOC || current_entry->type == ENTRY_CALLOC) && current_entry->size == 0)
			{
				ivector_append(allocs, &current_block);
				break;
			}
			else if (current_entry->type == ENTRY_REALLOC && current_entry->size == 0)
			{
				ivector_append(reallocs, &current_block);
				break;
			}
		}
	}

	ivector_destroy(entry_lists);

	*alloc_vector = allocs;
	*realloc_vector = reallocs;
}
/// @brief Prints table for zero-sized allocs
/// @param alloc_vector ivector_t<ivector_t<memory_entry>*> containing zero-sized m/callocs, returned by find_zero_re_allocs
static void print_zero_allocs(ivector_t *alloc_vector)
{
	size_t block_count = ivector_get_count(alloc_vector);
	if (block_count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No zero-sized allocs.                                                |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Zero-sized allocs===                                              |\n");

	for (size_t i = 0; i < block_count; i++)
	{
		//ivector_t<memory_entry>
		ivector_t *entries;
		ivector_fetch(alloc_vector, i, &entries);
		size_t entry_count = ivector_get_count(entries);

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", i, entry_count);  //REVIEW: Block number

		for (size_t j = 0; j < entry_count; j++)
		{
			memory_entry *entry = ivector_get(entries, j);
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
/// @brief Prints tablefor zero-sized reallocs
/// @param alloc_vector ivector_t<ivector_t<memory_entry>*> containing zero-sized reallocs, returned by find_zero_re_allocs
static void print_zero_reallocs(ivector_t *realloc_vector)
{
	size_t block_count = ivector_get_count(realloc_vector);
	if (block_count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No zero-sized reallocs.                                              |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Zero-sized reallocs===                                            |\n");

	for (size_t i = 0; i < block_count; i++)
	{
		//ivector_t<memory_entry>
		ivector_t *entries;
		ivector_fetch(realloc_vector, i, &entries);
		size_t entry_count = ivector_get_count(entries);

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", i, entry_count);  //REVIEW: Block number

		for (size_t j = 0; j < entry_count; j++)
		{
			memory_entry *entry = ivector_get(entries, j);
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

/// @brief Finds failed m/callocs and reallocs
/// @param failed_alloc_c Pointer to store number of failed m/callocs
/// @param failed_reallocs Pointer to store ivector_t<ivector_t<memory_entry>*> containing failed reallocs
static void find_failed_re_allocs(size_t *failed_alloc_c, ivector_t **failed_reallocs)
{
	//REVIEW: Ignore zero-sized ops that return NULL, shown separately

	//ivector_t<ivector_t<memory_entry>*>
	ivector_t *reallocs = ivector_create(sizeof(ivector_t*));
	size_t alloc_c = 0;

	//ivector_t<memory_entry>
	ivector_t *null_block = hashtable_get(status.entry_lookup, NULL);

	size_t null_entry_count = ivector_get_count(null_block);
	for (size_t i = 0; i < null_entry_count; i++)
	{
		memory_entry *entry = ivector_get(null_block, i);

		if ((entry->type == ENTRY_MALLOC || entry->type == ENTRY_CALLOC) && entry->size != 0) alloc_c++;
	}

	//ivector_t<ivector_t<memory_entry>*>
	ivector_t *entry_lists = hashtable_list_contents(status.entry_lookup); //FIXME: Hashtables return vector, not ivector

	size_t block_count = ivector_get_count(entry_lists);
	for (size_t i = 0; i < block_count; i++)
	{
		//ivector_t<memory_entry>
		ivector_t *cur_block;
		ivector_fetch(entry_lists, i, &cur_block);

		size_t entry_count = ivector_get_count(cur_block);
		for (size_t j = 0; j < entry_count; j++)
		{
			memory_entry *entry = ivector_get(cur_block, j);

			if (entry->type == ENTRY_REALLOC && entry->size != 0 && entry->new_ptr == NULL)
			{
				ivector_append(reallocs, &cur_block);
				break;
			}
		}
	}

	ivector_destroy(entry_lists);

	*failed_alloc_c = alloc_c;
	*failed_reallocs = reallocs;
}
/// @brief Prints table for failed m/callocs
/// @param failed_alloc_c The number of failed m/callocs
static void print_failed_allocs(size_t failed_alloc_c)
{
	if (failed_alloc_c == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No failed allocs.                                                    |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Failed allocs===                                                  |\n");

	//ivector_t<memory_entry>
	ivector_t *failed_allocs = hashtable_get(status.entry_lookup, NULL);

	set_color(COLOR_RED, COLOR_DEFAULT, 0);
	size_t entry_count = ivector_get_count(failed_allocs);
	for (size_t i = 0; i < entry_count; i++)
	{
		memory_entry *entry = ivector_get(failed_allocs, i);

		if ((entry->type == ENTRY_MALLOC || entry->type == ENTRY_CALLOC) && entry->size != 0)
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->new_ptr, format_file_line(entry->file_name, entry->line));
	}
}
/// @brief Prints table for failed reallocs
/// @param failed_reallocs ivector_t<ivector_t<memory_entry>*> containing failed reallocs
static void print_failed_reallocs(ivector_t *failed_reallocs)
{
	size_t block_count = ivector_get_count(failed_reallocs);
	if (block_count == 0)
	{
		set_color(COLOR_GREEN, COLOR_DEFAULT, 0);
		printf("| No failed reallocs.                                                  |\n");
		return;
	}

	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("| ===Failed reallocs===                                                |\n");

	for (size_t i = 0; i < block_count; i++)
	{
		//ivector_t<memory_entry>
		ivector_t *entries;
		ivector_fetch(failed_reallocs, i, &entries);
		size_t entry_count = ivector_get_count(entries);

		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("|Block #%-5ld has %-5ld entries:                                       |\n", i, entry_count);

		for (size_t j = 0; j < entry_count; j++)
		{
			memory_entry *entry = ivector_get(entries, j);
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

/// @brief Finds reallocs and frees of NULL. While allowed, might be unintended
/// @param null_reallocs Pointer to store number of NULL reallocs
/// @param null_frees Pointer to store number of NULL frees
static void find_null_reallocs_frees(size_t *null_reallocs, size_t *null_frees)
{
	size_t reallocc = 0, freec = 0;

	//ivector_t<memory_entry>
	ivector_t *null_block = hashtable_get(status.entry_lookup, NULL);

	size_t entry_count = ivector_get_count(null_block);
	for (size_t i = 0; i < entry_count; i++)
	{
		memory_entry *entry = ivector_get(null_block, i);

		if (entry->type == ENTRY_FREE) freec++;
		else if (entry->type == ENTRY_REALLOC) reallocc++;
	}

	*null_reallocs = reallocc;
	*null_frees = freec;
}
/// @brief Prints table for NULL reallocs
/// @param null_reallocs The number of NULL reallocs
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

	ivector_t *null_block = hashtable_get(status.entry_lookup, NULL);

	set_color(COLOR_RED, COLOR_DEFAULT, 0);
	size_t block_count = ivector_get_count(null_block);
	for (size_t i = 0; i < block_count; i++)
	{
		memory_entry *entry = ivector_get(null_block, i);

		if (entry->type == ENTRY_REALLOC && entry->old_ptr == NULL)
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->old_ptr, format_file_line(entry->file_name, entry->line));
	}
}
/// @brief Prints table for NULL frees
/// @param null_frees The number of NULL frees
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

	ivector_t *null_block = hashtable_get(status.entry_lookup, NULL);

	set_color(COLOR_RED, COLOR_DEFAULT, 0);
	size_t block_count = ivector_get_count(null_block);
	for (size_t i = 0; i < block_count; i++)
	{
		memory_entry *entry = ivector_get(null_block, i);

		if (entry->type == ENTRY_FREE && entry->old_ptr == NULL)
			printf("|>>> %-7s %6s @%-18p at %-25s<<<|\n", entry_type_str(entry->type), format_size(entry->size), entry->old_ptr, format_file_line(entry->file_name, entry->line));
	}
}

static void print_all_allocs()
{
	size_t alloc_count = ivector_get_count(status.allocs);
	if (alloc_count == 0)
	{
		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("| No (c)allocs.                                                        |\n");
		return;
	}

	for (size_t i = 0; i < alloc_count; i++)
	{
		memory_entry *entry = ivector_get(status.allocs, i);

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
	size_t realloc_count = ivector_get_count(status.reallocs);
	if (realloc_count == 0)
	{
		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("| No reallocs.                                                         |\n");
		return;
	}
	
	for (size_t i = 0; i < realloc_count; i++)
	{
		memory_entry *entry = ivector_get(status.reallocs, i);

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
	size_t free_count = ivector_get_count(status.frees);
	if (free_count == 0)
	{
		set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
		printf("| No frees.                                                           |\n");
		return;
	}

	for (size_t i = 0; i < free_count; i++)
	{
		memory_entry *entry = ivector_get(status.frees, i);

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
	size_t allocs = ivector_get_count(status.allocs);
	size_t reallocs = ivector_get_count(status.reallocs);
	size_t frees = ivector_get_count(status.frees);

	size_t memory_lost;
	ivector_t *lost_blocks = find_lost_blocks(&memory_lost);

	ivector_t *zero_allocs, *zero_reallocs;
	find_zero_re_allocs(&zero_allocs, &zero_reallocs);
	
	size_t failed_alloc_c;
	ivector_t *failed_reallocs;
	find_failed_re_allocs(&failed_alloc_c, &failed_reallocs);

	size_t null_reallocs, null_frees;
	find_null_reallocs_frees(&null_reallocs, &null_frees);

	//Internally 70 cols wide (72 external)
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("\n\n");
	printf("+==========================alloc_check report==========================+\n");
	printf("+--Statistics----------------------------------------------------------+\n");
	set_color(COLOR_WHITE, COLOR_DEFAULT, 0);
	printf("|Total allocs/reallocs/frees: %-5ld/%-5ld/%-5ld                        |\n", allocs, reallocs, frees);
	printf("|Total blocks/memory lost: %-5ld/~%-6s                               |\n", ivector_get_count(lost_blocks), format_size(memory_lost));
	printf("|Total zero-sized allocs/reallocs: %-5ld/%-5ld                         |\n", ivector_get_count(zero_allocs), ivector_get_count(zero_reallocs));
	printf("|Total failed allocs/reallocs: %-5ld/%-5ld                             |\n", failed_alloc_c, ivector_get_count(failed_reallocs));
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
	print_failed_allocs(failed_alloc_c);
	print_failed_reallocs(failed_reallocs);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+--Possible mistakes---------------------------------------------------+\n");
	print_null_reallocs(null_reallocs);
	print_null_frees(null_frees);
	set_color(COLOR_ORANGE, COLOR_DEFAULT, 0);
	printf("+======================================================================+\n");
	set_color(COLOR_DEFAULT, COLOR_DEFAULT, 0);

	ivector_destroy(lost_blocks);
	ivector_destroy(zero_allocs);
	ivector_destroy(zero_reallocs);
	ivector_destroy(failed_reallocs);
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
	ivector_destroy(status.allocs);
	ivector_destroy(status.reallocs);
	ivector_destroy(status.frees);

	hashtable_destroy_free(status.entry_lookup, NULL, ivector_destroy);

	status.allocs = NULL;
	status.reallocs = NULL;
	status.frees = NULL;
	status.entry_lookup = NULL;
	status.tick = 0;
}
