# COMPRESSIONS — current issues

Reviewed commit: `f82e39a033a9801bbbda8f063a12dc1952edc1f6`
(`fix(): tmp file mode`).

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

### 1 — P1: The scanner accepts FIFOs, symlinks, and other non-regular sources

**Location:** `src/os_operations.c:141–173`, `src/os_operations.c:291–355`.

The scanner excludes only entries reported as `DT_DIR`. A matching FIFO is
passed to `F_Gzip()` and blocks in `fopen()`; the test process remained blocked
until a two-second timeout. Matching source symlinks are followed and can point
outside the intended asset directory. `d_type` can also be `DT_UNKNOWN`, so it
is not a complete type check.

**Required behavior:** Open entries relative to the directory descriptor,
reject symlinks according to an explicit policy, and verify with `fstatat()` or
`fstat()` that each opened source is a regular file. Do not process FIFOs,
sockets, devices, or directories.

**Regression check:** Verify that ordinary assets are processed while FIFOs,
sockets, directories, and disallowed symlinks are skipped without blocking.
Include a `DT_UNKNOWN` path.

### 2 — P3: Temporary-output recovery and I/O checks remain fragile

**Location:** `src/os_operations.c:183–233` (`write_file`).

Successful replacement is now atomic and does not follow a destination
symlink. The previous sidecar also survives an injected write failure. However,
the fixed `./tmp.packed` name can remain after `SIGKILL`, a crash, or power loss;
the next single-process invocation then fails because it opens the file with
`O_EXCL`. A zero-byte result from a later `write()` makes the retry loop repeat
without progress, and `fsync()` failure is inferred from `errno` instead of its
return value.

**Required behavior:** Create a safely recoverable temporary file in the
destination directory, treat a positive-length zero-byte write as an error,
and check `fsync(fd) == -1` directly.

**Regression check:** Leave a stale temporary file and verify that the next
normal invocation can recover safely. Inject a zero-byte retry and an `fsync()`
failure and verify that each returns an error without replacing the old
sidecar.

### 3 — P2: The asset CLI always scans the current directory

**Location:** `src/main.c:55–86`, `src/os_operations.c:291`.

`main()` ignores its arguments and always runs `browse_dir()` against `.` with
the hardcoded extension set `html.css.js` and a hardcoded 1,024-byte threshold.
This is not yet the explicit preparation command needed by WSER deployments.

**Required behavior:** Provide a mode such as:

```text
compressions assets DIRECTORY
```

Validate arguments before modifying files, expose extension and size policies
through deliberate options or configuration, and return nonzero when any
required output fails.

### 4 — P2: Windows directory compression and sidecar deletion do nothing

**Location:** `src/os_operations.c:345–366`, `README.md:7`.

The Windows branches of `browse_dir()` and `del_file()` are empty and report
success. This conflicts with the README claim that the program works on Windows
as-is.

**Required behavior:** Implement Windows enumeration and deletion with
equivalent suffix, type, close, and error handling, or return an explicit
unsupported-operation error and narrow the portability claim.

### 5 — P2: HTML minimization passes `long long` storage as `size_t *`

**Location:** `src/lz77.c:1133–1163`.

`size` is a `long long`, but its address is cast to `size_t *` for
`HTML_minimize()`. Accessing the object through that incompatible type violates
C aliasing rules and assumes the types have the same size and representation.

**Required behavior:** Range-check the file length, copy it into a real
`size_t` variable, pass that variable to the minimizer, and convert back only
when the API requires it.

### 6 — P3: The README documents an obsolete public API

**Location:** `README.md:9–32`, `include/lz77.h:93–99`.

The README still shows `F_Gzip(char *file_name)`, while the implementation takes
five parameters. It also does not document mode values, the skip result,
ownership of the returned memory buffer, or that a positive return is the
complete gzip length.

**Required behavior:** Document the exact current prototypes and the ownership,
length, skip, and error contracts before publishing a library package.

## Other current library safety issues

### 7 — P1 when using decompression: The output allocation check is invalid

**Location:** `src/lz77.c:1223–1257` (`inflate_GZIP`).

After allocating `*inflated_outup`, the code checks `inflated_outup` instead of
`*inflated_outup`. If allocation fails, execution can continue to `memset()` a
null pointer. Cleanup of the later input-allocation failure now frees the
correct pointer, but it does not reset the caller's pointer or output size.

**Required behavior:** Initialize outputs to `NULL` and zero, check
`*inflated_outup`, and restore the initialized output state on every failure
path. Exercise allocation failures in regression tests.
