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


#ifndef USE_STANDARD_MEM
#define CHKD_MALLOC(size) malloc(size)
#define CHKD_REALLOC(ptr, size) malloc(ptr, size)
#define CHKD_FREE(ptr) free(ptr);
#else
#define CHKD_MALLOC(size) checked_malloc(size, __FILE__, __LINE__)
#define CHKD_REALLOC(ptr, size) checked_malloc(ptr, size, __FILE__, __LINE__)
#define CHKD_FREE(ptr) checked_free(ptr, __FILE__, __LINE__);
#endif


#ifndef ALLOW_STANDARD_MEM
//Poison identifiers to prevent their use
#pragma GCC poison malloc realloc free
#endif


void *checked_malloc(size_t size, char *file_name, int line);
void *checked_realloc(void *ptr, size_t size, char *file_name, int line);
void checked_free(void *ptr, char *file_name, int line);

void report_alloc_checks();
//TODO: Full history function


#endif
