#include "alloc_check.h"

int main()
{
	int *arr1 = CHKD_MALLOC(4 * sizeof(int));
	CHKD_FREE(arr1);

	int *arr2 = CHKD_MALLOC(20 * sizeof(int));

	int *arr3 = CHKD_CALLOC(10, sizeof(int));
	CHKD_FREE(arr3);

	int *arr4 = CHKD_CALLOC(0, sizeof(int));
	int *tmp4 = CHKD_REALLOC(arr4, 5 * sizeof(int));
	CHKD_FREE(tmp4);


	report_alloc_checks();
	return 0;
}
