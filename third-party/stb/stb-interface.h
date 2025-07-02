#ifndef STB_INTERFACE
#define STB_INTERFACE

extern "C" 
{
    #include "stb_image.h"
    #include "stb_truetype.h"

    unsigned char *stbi_xload(char const *filename, int *x, int *y, int *frames); // load gif animated
}

#endif