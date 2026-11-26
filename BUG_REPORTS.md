# COMPRESSIONS — current issues

Reviewed commit: `2fdf4f5e327e6d10e6379bd00b730816d0422959`
(`fix-ci-workflow-2`).

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

### 1 — P3: Temporary-output recovery and I/O checks remain fragile

**Location:** `src/os_operations.c:227–285` (`write_file`).

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

### 2 — P2: The asset CLI always scans the current directory

**Location:** `src/main.c:55–86`, `src/os_operations.c:342–405`.

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

### 3 — P2: Windows directory compression and sidecar deletion do nothing

**Location:** `src/os_operations.c:396–417`, `README.md:7`.

The Windows branches of `browse_dir()` and `del_file()` are empty and report
success. This conflicts with the README claim that the program works on Windows
as-is.

**Required behavior:** Implement Windows enumeration and deletion with
equivalent suffix, type, close, and error handling, or return an explicit
unsupported-operation error and narrow the portability claim.

### 4 — P2: HTML minimization passes `long long` storage as `size_t *`

**Location:** `src/lz77.c:1139–1169`.

`size` is a `long long`, but its address is cast to `size_t *` for
`HTML_minimize()`. Accessing the object through that incompatible type violates
C aliasing rules and assumes the types have the same size and representation.

**Required behavior:** Range-check the file length, copy it into a real
`size_t` variable, pass that variable to the minimizer, and convert back only
when the API requires it.

### 5 — P3: The README documents an obsolete public API

**Location:** `README.md:9–32`, `include/lz77.h:93–99`.

The README still shows `F_Gzip(char *file_name)`, while the implementation takes
five parameters. It also does not document mode values, the skip result,
ownership of the returned memory buffer, or that a positive return is the
complete gzip length.

**Required behavior:** Document the exact current prototypes and the ownership,
length, skip, and error contracts before publishing a library package.
