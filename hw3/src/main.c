#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    double* ptr = sf_malloc(sizeof(double));

    *ptr = 320320320e-320;

    printf("%f\n", *ptr);

    sf_show_heap();

    char* ptr1 = sf_malloc(126*sizeof(char));

    sf_show_heap();

    int* ptr2 = sf_malloc(sizeof(int));

    *ptr2 = 5;

    sf_show_heap();

    float* ptr3 = sf_malloc(sizeof(float));

    *ptr3 = 4.5;

    sf_show_heap();

    sf_free(ptr2);

    sf_show_heap();

    ptr1 = sf_realloc(ptr1, 10);

    sf_show_heap();


    return EXIT_SUCCESS;
}
