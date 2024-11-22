/**
 * @file alloc_check.h
 * 
 * @brief Dynamic memory allocation helper
 * 
 * @author Diogo Cruz Diniz
 * Contact: diogo.cruz.diniz@tecnico.ulisboa.pt
 */

/**
 * Notes:
 * alloc_check will call exit(72) in case any of it's internal systems fails to execute correctly
 */

#ifndef ALLOC_CHECK_H
#define ALLOC_CHECK_H


#include <stddef.h>
#include <stdlib.h>


#ifdef USE_STANDARD_MEM
#define ALLOC_CHECK_SETUP() do {} while(0)
#else
#define malloc(size) checked_malloc(size, __FILE__, __LINE__)
#define calloc(nitems, size) checked_calloc(nitems, size, __FILE__, __LINE__)
#define realloc(ptr, size) checked_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) checked_free(ptr, __FILE__, __LINE__)
#define ALLOC_CHECK_SETUP() do { atexit(&cleanup_alloc_checks); atexit(&report_alloc_checks); } while (0)
#endif

void *checked_malloc(size_t size, char *file_name, int line);
void *checked_calloc(size_t nitems, size_t size, char *file_name, int line);
void *checked_realloc(void *ptr, size_t size, char *file_name, int line);
void checked_free(void *ptr, char *file_name, int line);

void report_alloc_checks();
void list_all_entries();
void cleanup_alloc_checks();


#endif
