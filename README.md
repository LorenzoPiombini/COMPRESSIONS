# compression study

this is a Deflate algorithm using LZ77 and Huffman codes to compress data. 
the memory allocations are ketp at minimal, to make sure this routine runs fast enough.
the deflate part is complete, as per 08-24-26.

this program work on Linux, Windows and MacOS as is, just need to be compiled, ofcourse.

# API 
I am still developing this, this list of endpoints is not complete.

```c
long long deflate(uint8_t *input, uint64_t input_size,uint8_t **deflate_input);
long long inflate_GZIP(uint8_t *file_content, uint64_t file_size, uint8_t **inflated_outup, uint64_t *inflated_outup_size);
long long unZIP(uint8_t *file_content, uint64_t file_size);
int Gzip_file(char *file_name, uint8_t *stream, uint64_t stream_size);

```

here an example for **deflate()**, *data* is the uncompressed stream, *input_size* is the size of the stream, *df_in* will be the compressed result.
the function return -1 if it fails, or the size of the *deflated stream* if it is succesful.

```c
        uint8_t *df_in = NULL;
        long long r = 0;
        if((r = deflate(data,input_size,&df_in)) == -1){
            /* handle error */
            return -1;
        }
```
the caller has to free the result **df_in**.




# Useful documentation

| Document / Topic | Category | Link |
| :--- | :--- | :--- |
| **RFC 1951** | Deflate specification 1.3 | [Read RFC](https://datatracker.ietf.org/doc/html/rfc1951)) |
| **LZ77 Algorithm** | Compression Algorithm | [Read Wiki Article](https://en.wikipedia.org/wiki/LZ77_and_LZ78) |
| **GZIP** | File Format & Software | [Read Wiki Article](https://en.wikipedia.org/wiki/Gzip) |
| **.ZIP** | Archive File Format | [Read Wiki Article](https://en.wikipedia.org/wiki/ZIP_(file_format)) |
