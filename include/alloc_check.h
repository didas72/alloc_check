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


#ifdef USE_STANDARD_MEM
#include <stdlib.h>
#define CHKD_MALLOC(size) malloc(size)
#define CHKD_CALLOC(nitems, size) calloc(nitems, size)
#define CHKD_REALLOC(ptr, size) realloc(ptr, size)
#define CHKD_FREE(ptr) free(ptr);
#else
#define CHKD_MALLOC(size) checked_malloc(size, __FILE__, __LINE__)
#define CHKD_CALLOC(nitems, size) checked_calloc(nitems, size, __FILE__, __LINE__)
#define CHKD_REALLOC(ptr, size) checked_realloc(ptr, size, __FILE__, __LINE__)
#define CHKD_FREE(ptr) checked_free(ptr, __FILE__, __LINE__)
#endif


#ifndef ALLOW_STANDARD_MEM
//Poison identifiers to prevent their use
#pragma GCC poison malloc calloc realloc free
#endif


void *checked_malloc(size_t size, char *file_name, int line);
void *checked_calloc(size_t nitems, size_t size, char *file_name, int line);
void *checked_realloc(void *ptr, size_t size, char *file_name, int line);
void checked_free(void *ptr, char *file_name, int line);

void report_alloc_checks();
void cleanup_alloc_checks();


#endif
