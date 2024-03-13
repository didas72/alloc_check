#include "alloc_check.h"

int main()
{
	int *arr1 = CHKD_MALLOC(4 * sizeof(int));
	CHKD_FREE(arr1);

	int *arr2 = CHKD_MALLOC(20 * sizeof(int));

	report_alloc_checks();
	return 0;
}
