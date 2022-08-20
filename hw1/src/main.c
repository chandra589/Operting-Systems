#include <stdio.h>
#include <stdlib.h>

#include "mtft.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

int main(int argc, char **argv)
{
    if(validargs(argc, argv))
    {
        //debug("exit failure usage\n");
        USAGE(*argv, EXIT_FAILURE);
    }
    if(global_options & HELP_OPTION)
    {
        //debug("exit success usage with -h flag\n");
        USAGE(*argv, EXIT_SUCCESS);
    }

    //if encode option is true
    if (global_options & ENCODE_OPTION)
    {
        //debug("starting mtft_Encode\n");
        int ret = mtf_encode();
        if (ret == 0)
            return EXIT_SUCCESS;
        else
            return EXIT_FAILURE;
    }
    else if (global_options & DECODE_OPTION)
    {
        //debug("starting mtft_Decode\n");
        int ret = mtf_decode();
        if (ret == 0)
            return EXIT_SUCCESS;
        else
            return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
