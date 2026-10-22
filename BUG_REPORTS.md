# COMPRESSIONS gzip review

Reviewed commit `508e57a83d26bf78cae86f15dc49f4f4d1660864` (main).
The repository was not modified. Probes ran in temporary directories, using GCC,
AddressSanitizer/UndefinedBehaviorSanitizer and Python's independent gzip decoder.

## Confirmed findings

1. **P1 — Handle empty and single-symbol distance Huffman trees.**
   `src/lz77.c:904–908` builds a distance tree without handling zero or one used
   symbol. With no LZ77 matches, the zero-initialized root recurses into itself in
   `assign_depth`; compressing `abcdef` triggers an ASan-confirmed stack overflow.
   With one distance symbol, assigning depth zero omits the required distance
   code: repeating `console.log("hello");\n` produces a file that standard gzip
   rejects with `invalid distance code`. Use valid DEFLATE degenerate-tree
   handling (or an appropriate stored/fixed block) rather than serializing these
   trees unchanged. This affects ordinary small web assets, not just bad inputs.

2. **P1 — Match exact source-file suffixes and skip generated output.**
   `src/os_operations.c:257–260` uses substring matching. With `html.css.js`, it
   selects `index.html.gz`, `app.js.map`, `backup.html.old`, `htmlnotes.txt` and a
   directory named `folder.js`. This can recompress generated gzip files and
   abort or crash the batch. A directory named `folder.js` alone aborts the scan;
   a batch containing `page.html` plus an existing valid `page.html.gz` crashed
   during testing. Check the final `.html`, `.css` or `.js` suffix and ensure the
   input is an intended regular source file before invoking the callback.

3. **P1 — Return the complete gzip member length in memory mode.**
   `src/lz77.c:1191–1193` returns `comp_size`, although the returned buffer also
   contains the gzip header, original filename and eight-byte trailer. A test
   `page.html` returned 54 while the complete member occupied 82 bytes. Decoding
   the returned-length bytes fails with premature EOF; decoding all 82 succeeds.
   Return `bwritten` in memory mode. File mode already writes `bwritten` and
   returns a success status. If WSER uses the current memory return as the send
   length or Content-Length, it will truncate the response.

4. **P2 — Support empty and very small web assets.**
   `src/lz77.c:873–876` returns zero without an end-of-block stream for empty
   input and rejects input smaller than `sizeof(struct LDpair)` (six bytes in
   this build). An empty `.js` is reported successfully compressed but fails
   standard gzip decoding; one-, two- and five-byte files return -1. For example,
   a valid two-byte `{}` JavaScript input can stop the entire directory batch.
   Allocate at least one token slot and emit a valid empty DEFLATE stream.

5. **P2 — Propagate final buffered-write failures.**
   `src/os_operations.c:189–190` ignores `fclose` failure. With a zero-byte
   `RLIMIT_FSIZE` and SIGXFSZ ignored, compression returned success while the
   output gzip file had zero bytes. Small buffered writes can fail only during
   close/flush. Check the final close result and avoid treating partial files as
   successful compressed assets.

6. **P2 — Reject an empty extension list before accessing `p[-1]`.**
   `src/os_operations.c:252–255` executes the do/while even when the list is
   empty, then reads before the buffer. `browse_dir("", callback)` triggers an
   ASan stack-buffer-underflow. Handle empty input explicitly and parse tokens
   without reading before their start.

7. **P2 — Close directory streams on success and error.**
   `src/os_operations.c:246–275` never calls `closedir`, including early callback
   failure. Repeating 40 scans with a 32-descriptor limit fails despite a callback
   that opens no files. This matters if a server reuses the routine for rescans.

8. **P2 — Fix the Windows branch's variable scope/implementation.**
   `src/os_operations.c:249,267–271` declares `found` only in the POSIX branch,
   leaves the Windows branch empty, then references `found` unconditionally.
   Consequently this function cannot compile for the advertised Windows target.
   This is a source-level finding; no Windows toolchain was available to run it.

9. **P2 — Separate the directory gzip command from ZIP extraction.**
   `src/main.c:73–81` compresses the directory, then requires an unrelated ZIP
   argument and calls `test_ZIP`. Running the CLI without arguments produced a
   valid output but exited 255 with “you gotta specify a file to zip!”. This
   prevents reliable use as a standalone asset-build step. Define separate modes
   and return the selected operation's actual result.

## Passing cases and scope

Individually compressing the current easyres `index.html`, `staff.html`,
`styles.css` and `app.js` in file mode produced gzip files that decompressed
byte-for-byte correctly with Python's gzip implementation. These successes do
not cover the failure cases above. Native Windows/macOS execution and WSER's
actual HTTP negotiation/cache behavior were not tested in this review.

