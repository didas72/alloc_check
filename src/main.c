#include "alloc_check.h"

int main()
{
	int *arr1 = CHKD_MALLOC(4 * sizeof(int));
	CHKD_FREE(arr1);

	int *arr2 = CHKD_MALLOC(20 * sizeof(int));

	int *arr3 = CHKD_CALLOC(10, sizeof(int));
	int *tmp3 = CHKD_REALLOC(arr3, 0);
	CHKD_FREE(arr3);

	int *arr4 = CHKD_CALLOC(0, sizeof(int));
	int *tmp4 = CHKD_REALLOC(arr4, 5 * sizeof(int));
	CHKD_FREE(tmp4);

	int *arr5 = CHKD_MALLOC(sizeof(int) * (1ul << 40));

	CHKD_FREE(NULL);
	CHKD_REALLOC(NULL, 20);

	(void)arr1;
	(void)arr2;
	(void)arr3;
	(void)tmp3;
	(void)arr4;
	(void)tmp4;
	(void)arr5;

	report_alloc_checks();
	cleanup_alloc_checks();
	return 0;
}
