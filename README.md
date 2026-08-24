# compression study

this is a Deflate algorithm using LZ77 and Huffman codes to compress the input. 
the memory allocations are ketp at minimal, to make sure this routine runs fast enough.
the deflate part is complete, as per 08-24-26.

# main.c
---
in the src directory, inside the main.c file, you find an example regarding how I plan to use this function.
you just call the **deflate()** function, you pass the input, the input size, and it returns the size of the 'deflated input', and the deflated result.

```c
        uint8_t *df_in = NULL;
        long long r = 0;
        if((r = deflate(data,input_size,&df_in)) == -1){
            /* handle error */
            return -1;
        }
```
the caller of this function has to free the result **df_in**.
