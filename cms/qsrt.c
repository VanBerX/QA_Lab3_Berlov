#include "qsrt.h"

#define SWAP(p, q) do { int t = *(p); *(p) = *(q); *(q) = t; } while(0)

static void bubblesort_int(int *array, int left, int right)
{
    int i, j;
    if(right - left < 1)
        return;
    for(i = right; i > left; i--) {
        int cnt = 0;
        for(j = left; j < i; j++)
            if(array[j] > array[j+1]) {
                SWAP(array+j, array+j+1);
                cnt++;
            }
        if(!cnt)
            break;
    }
}

static void do_quicksort_int(int *array, int left, int right)
{
    int i, j, pivot;

    if(right - left < 15) {
        bubblesort_int(array, left, right);
        return;
    }
    pivot = array[(left+right)/2];
    i = left;
    j = right;
    for(;;) {
        while(array[i] < pivot)
            i++;
        while(array[j] > pivot)
            j--;
        if(i >= j) {
            do_quicksort_int(array, left, j);
            do_quicksort_int(array, j+1, right);
            return;
        }
        SWAP(array+i, array+j);
        i++;
        j--;
    }
}

void quicksort_int(int *array, int len)
{
    do_quicksort_int(array, 0, len-1);
}


#ifdef QSRT_TEST

#include <stdio.h>
#include <stdlib.h>

static void apr(int *a, int n)
{
    int k;
    for(k = 0; k < n; k++)
        printf("%d ", a[k]);
    printf("\n");
    fflush(stdout);
}

static void chk(int *a, int n)
{
    int count = 0;
    int k;
    for(k = 0; k < n-1; k++)
        if(a[k] > a[k+1])
            count++;
    if(count)
        printf("FAILED WITH %d MISPLACEMENTS\n", count);
}

enum { arr_sz = 1000 };

int main()
{
    int array[arr_sz];
    int i, k;

    srand(777);

#if 0
    for(i = 0; i < arr_sz; i++)
        array[i] = 10000 + rand() % 9999;
    bubblesort_int(array, 0, arr_sz-1);
    apr(array, arr_sz);
    chk(array, arr_sz);
    printf("==========================================================\n");
#endif

    for(k = 0; k < 5000; k++) {
        for(i = 0; i < arr_sz; i++)
            array[i] = 10000 + rand() % 99;
        for(i = arr_sz/2 - 20; i < arr_sz/2 + 20; i++)
            array[i] = 50000;
        quicksort_int(array, arr_sz);
        //apr(array, arr_sz);
        chk(array, arr_sz);
        //printf("====================================\n");
    }

#if 0
    for(i = 0; i < arr_sz; i++)
        array[i] = i;
    quicksort_int(array, arr_sz);
    apr(array, arr_sz);
    chk(array, arr_sz);
    printf("==========================================================\n");

    for(i = 0; i < arr_sz; i++)
        array[i] = 10000 - 10*i;
    quicksort_int(array, arr_sz);
    apr(array, arr_sz);
    chk(array, arr_sz);
    printf("==========================================================\n");

    for(i = 0; i < arr_sz; i++)
        array[i] = 13;
    quicksort_int(array, arr_sz);
    apr(array, arr_sz);
    chk(array, arr_sz);
    printf("==========================================================\n");
#endif

    return 0;
}

#endif
