# COMPRESSIONS — current issues

Reviewed commit: `91ae12f07ed94101ff0c7de26005823668b378f6`
(`fix(): wrong size in memmode F_Gzip`).

This file intentionally lists only open findings. Resolved findings have been
removed. The primary scope is the proposed WSER asset pipeline:

```text
source .html/.css/.js -> browse_dir() -> .gz sidecar -> WSER
```

The current Linux behavior was checked with GCC, ASan/UBSan, temporary
fixtures, GNU gzip, and byte-for-byte decompression comparisons. Windows and
macOS were not tested. P1 means fix before running this pipeline automatically
in production. P2 is important correctness or portability work. P3 is lower
priority cleanup or documentation.

## WSER precompressed-asset path

### 1 — P1: Skipping a source can leave a stale `.gz` sidecar

**Location:** `src/lz77.c:1142–1152`, `src/os_operations.c:276–284`.

`F_Gzip()` returns success without touching the existing sidecar when the
source is at or below the 1,024-byte limit. HTML can also become smaller than
the limit after minimization and take the same path.

This was reproduced by compressing a large `page.html`, replacing the source
with `{}`, and running the scan again. `page.html.gz` still contained the old
page. If WSER selects that sidecar, gzip-capable and identity clients can
receive different versions of the resource.

**Required behavior:** Either generate a fresh sidecar for every selected
source, or safely remove/invalidate the old sidecar when the source is skipped.
Deleted sources also need sidecar cleanup, normally through a build manifest.

**Regression check:** Build a sidecar, shrink the source below and exactly to
the configured threshold, rebuild, and verify that there is either a fresh
matching sidecar or no sidecar.

### 2 — P1: Output replacement follows symlinks and is not atomic

**Location:** `src/os_operations.c:174–190` (`write_file`).

The destination is opened directly with `fopen(path, "wb")`. An existing
sidecar symlink is followed, so its target is overwritten. This was reproduced
with `page.html.gz -> unrelated.txt`. The function also ignores the result of
`fclose()`, so a final buffered-write failure can be reported as success after
the old destination has already been truncated.

**Required behavior:** Write to an exclusively created temporary regular file
in the destination directory, reject unsafe file types, check all write and
close results, and atomically rename the completed file over the sidecar.
Optionally sync the file and directory when crash durability is required.

**Regression check:** Inject a final write/close failure and verify that the old
sidecar survives. Place a symlink at the destination and verify that its target
is unchanged.

### 3 — P1: The scanner accepts FIFOs, symlinks, and other non-regular sources

**Location:** `src/os_operations.c:139–171`, `src/os_operations.c:242–294`.

The scanner excludes only entries reported as `DT_DIR`. A matching FIFO is
passed to `F_Gzip()` and blocks in `fopen()`; the test process remained blocked
until a two-second timeout. Matching symlinks are followed and can point outside
the intended asset directory. `d_type` can also be `DT_UNKNOWN`, so it is not a
complete type check.

**Required behavior:** Open entries relative to the directory descriptor,
reject symlinks according to an explicit policy, and verify with `fstatat()` or
`fstat()` that each opened source is a regular file. Do not process FIFOs,
sockets, devices, or directories.

**Regression check:** Verify that ordinary assets are processed while FIFOs,
sockets, directories, and disallowed symlinks are skipped without blocking.
Include a `DT_UNKNOWN` path.

### 4 — P2: Skipped files are counted as successfully compressed

**Location:** `src/os_operations.c:276–299`, `src/lz77.c:1142–1152`.

`found` is incremented before the callback runs. `F_Gzip()` returns `0` both
when a disk sidecar is written and when the source is skipped by the size
limit. A directory containing only a two-byte `page.html` therefore prints
`1 files compressed` even though no sidecar was created. The 1,024-byte policy
is also hardcoded in `browse_dir()`.

**Required behavior:** Give the callback distinct `COMPRESSED`, `SKIPPED`, and
`ERROR` results. Count only completed outputs, report skips separately, and make
the threshold configurable. This result contract should also drive the stale
sidecar handling in issue 1.

### 5 — P2: The asset CLI always scans the current directory

**Location:** `src/main.c:55–86`, `src/os_operations.c:248`.

`main()` ignores its arguments and always runs `browse_dir()` against `.` with
the hardcoded extension set `html.css.js`. This is not yet the explicit
preparation command needed by WSER deployments.

**Required behavior:** Provide a mode such as:

```text
compressions assets DIRECTORY
```

Validate arguments before modifying files, expose the extension and size
policies through deliberate options or configuration, and return nonzero when
any required output fails.

### 6 — P2: Windows directory compression silently does nothing

**Location:** `src/os_operations.c:295–297`, `README.md:7`.

The Windows branch of `browse_dir()` is empty and then reports success. This
conflicts with the README claim that the program works on Windows as-is.

**Required behavior:** Implement Windows enumeration with equivalent suffix,
type, and close/error handling, or return an explicit unsupported-operation
error and narrow the portability claim.

### 7 — P2: HTML minimization passes `long long` storage as `size_t *`

**Location:** `src/lz77.c:1138–1149`.

`size` is a `long long`, but its address is cast to `size_t *` for
`HTML_minimize()`. Accessing the object through that incompatible type violates
C aliasing rules and assumes the types have the same size and representation.

**Required behavior:** Range-check the file length, copy it into a real
`size_t` variable, pass that variable to the minimizer, and convert back only
when the API requires it.

### 8 — P3: The README documents an obsolete public API

**Location:** `README.md:9–32`, `include/lz77.h:93–99`.

The README still shows `F_Gzip(char *file_name)`, while the implementation takes
five parameters. It also does not document mode values, the skip result,
ownership of the returned memory buffer, or that the return value in memory
mode is the complete gzip length.

**Required behavior:** Document the exact current prototypes and the ownership,
length, skip, and error contracts before publishing a library package.

## Other current library safety issues

### 9 — P1 when using decompression: Allocation failure checks and cleanup are invalid

**Location:** `src/lz77.c:1227–1235` (`inflate_GZIP`).

After allocating `*inflated_outup`, the code checks `inflated_outup` instead of
`*inflated_outup`. If allocation fails, execution can continue to `memset()` a
null pointer. If the later input allocation fails, `free(inflated_outup)` frees
the address of the caller's pointer rather than the allocated output buffer.
GCC diagnoses the latter as freeing a non-heap object.

**Required behavior:** Check `*inflated_outup`, free `*inflated_outup` on later
failure, set it to `NULL`, and leave output size zero on every failure path.
Exercise allocation failures in regression tests.

### 10 — P1 when using the list API: List nodes are under-allocated

**Location:** `src/list.c:9–33`.

`List_push()` and `List_list()` allocate `sizeof(T *)` or `sizeof(T)`, where
`T` is already a pointer typedef. They then write a complete `struct List_T`,
which is larger than the allocation on normal 64-bit targets. GCC reports both
under-allocations. The asset path does not currently call this API, but any
caller can trigger a heap buffer overflow.

**Required behavior:** Allocate and clear `sizeof *p` in `List_push()` and
`sizeof **p` in `List_list()`. Add push/list/pop tests under ASan.

### 11 — P3: `HTML_clear_white_space()` has an incorrect return type

**Location:** `src/buffer.c:264–342`.

The function is declared to return `int` but reaches the end without returning
a value. Its current caller ignores the result.

**Required behavior:** Change it to `void`, or return and check a meaningful
status consistently.
