# COMPRESSIONS review — precompressed assets for WSER

Reviewed commit: `cef76d845bc1a59169882720d4464c49e66969ab` (`fix() browse_dir`).
Baseline: the earlier report for `508e57a` in `BUG_REPORTS.md`.

The intended path is **source `.html`/`.css`/`.js` → `browse_dir()` → `.gz`
sidecars on disk → WSER serves those files**. Runtime compression is not required
for this design. Findings below prioritize that path; other public API issues
are separated. Production C sources were not changed by this review.

Tests used GCC on Linux, ASan/UBSan, temporary fixtures and Python's independent
`gzip.decompress()`. Native Windows/macOS and WSER's live HTTP selection,
Content-Encoding, freshness checks and identity fallback were not tested.
P1 means fix before relying on this asset pipeline; P2 means important correctness
or portability work; P3 is lower-impact input validation.

## What your changes fixed

| Earlier finding | Current result |
| --- | --- |
| Substring-only extension selection / gzip recompression | Fixed for the tested ordinary filenames: `index.html.gz`, `app.js.map`, `backup.html.old` and `htmlnotes.txt` are skipped. `main.min.js`, `site.v2.css`, `.hidden.js` and `index.html` are selected. Non-regular inputs remain a problem; see A4. |
| Modifying the extension string | Fixed: parsing now uses a separate `ex` buffer. A literal `"html.css.js"` works repeatedly. |
| Empty-list buffer underflow | The earlier `p[-1]` read is gone; ASan no longer reports it. Empty input still selects a trailing-dot filename; see B4. |
| Directory handle leak | Fixed in the tested paths: 100 successful scans and 100 callback-failure scans work with a 32-descriptor limit. Real directories ending in `.js` are skipped. |
| Windows `found` variable scope | Fixed at source level. Windows enumeration itself remains unimplemented; see A7. |
| Empty/tiny inputs crashing the default batch | The new 1,024-byte threshold avoids these inputs in that batch. This is a skip policy, not a compressor fix; see A5 and B2. |

## A. Findings affecting precompressed WSER assets

### A1 — P1: Some matching files still crash or produce invalid gzip

**Location:** `src/lz77.c:559–567`, `720–755`, `904–908`.

**Reproduction:** A 2,100-byte file containing `console.log("hello");\n` repeated
100 times passes the new threshold. `F_Gzip()` returns success and writes a
72-byte gzip file, but Python rejects it with `invalid distance code`. A
2,048-byte fixture with no repeated three-byte sequence crashes the **default
batch** with an ASan-confirmed stack overflow in `assign_depth`. The companion
runner generates that fixture deterministically. The six-byte `abcdef` case
also still crashes when the threshold is disabled.

**Cause:** Distance Huffman trees with zero used symbols recurse through an
uninitialized/empty root; a single used symbol receives depth zero and no usable
distance code is emitted. File-size filtering cannot make these trees valid.

**Suggested solution:** Handle zero/single-symbol alphabets explicitly when
building and serializing dynamic trees. Use the RFC 1951 legal one-symbol
representation and consistent HLIT/HDIST/code-length serialization, or fall back
to a valid fixed/stored block. An empty stream still needs an end-of-block.

**Regression check:** Independently decompress no-match, one-distance-symbol,
repeated-byte and ordinary web assets, including files above 1 KB; compare every
output byte with the source. A successful return alone is insufficient.

### A2 — P1: Shrinking a source below the threshold leaves a stale `.gz`

**Location:** `src/lz77.c:1131–1134`; `src/os_operations.c:275–282`.

**Reproduction:** Start with `page.html.gz` containing an older >1 KB source;
replace `page.html` with `{}` and run the batch. It prints `1 files compressed`
and exits successfully, but the gzip still decompresses to the old content.

**Impact:** If WSER selects an existing sidecar without independently checking
freshness, compressed clients receive the old page/script while identity clients
receive the new source. That HTTP behavior is conditional, not a tested WSER claim.

**Suggested solution:** Decide whether small files should have sidecars. To
compress every match, remove/disable this size cutoff after fixing A1/B2. If
skipping is intentional, explicitly invalidate an existing generated sidecar and
have WSER fall back to the source. Publish source and sidecar changes as one
asset-build result; do not leave a stale pair marked successful. A manifest can
also remove generated sidecars for deleted sources.

**Regression check:** Build, shrink a source below the cutoff, rebuild, and
verify there is either a fresh matching gzip or no sidecar. Include the exactly
1,024-byte boundary, which is skipped by `<=`.

### A3 — P1: Output writes can report false success or overwrite a link target

**Location:** `src/os_operations.c:173–191` (`write_file`).

**Reproduction:** The first 2,048 bytes of `lorem.txt` normally produce a valid
916-byte gzip. With `RLIMIT_FSIZE=0` and SIGXFSZ ignored, the buffered write reports
success; the final flush fails, but `fclose()` is ignored, leaving a zero-byte
output while `F_Gzip()` returns 0. Separately, an existing `page.html.gz` symlink
to `unrelated.txt` causes that unrelated file to be replaced with gzip bytes.
Both probes operate only on temporary files.

**Cause:** Direct `fopen(destination, "wb")` follows links/truncates existing
outputs, and final buffered-write failures are discarded. A failed replacement
can destroy the previous valid sidecar.

**Suggested solution:** Generate into an exclusively created temporary regular
file in the destination directory. Check write, flush and close results; clean
up and return an error on failure. Atomically rename a completed file into place
so readers see a complete member and existing symlinks are replaced as directory
entries rather than followed. If crash durability is required, also sync the file
and destination directory. Do not deploy a failed source/sidecar build.

**Regression check:** Inject flush failure and verify an error plus preservation
of the previous output. Test an existing destination symlink and confirm its
target remains unchanged.

### A4 — P2: Matching FIFOs and symlinks are treated as source files

**Location:** `src/os_operations.c:251–252`, `275–277`.

**Reproduction:** The scan callback selects both `pipe.js` (a FIFO) and `link.js`
(a symlink to a `.txt` file). The real batch hangs opening the FIFO; the review
terminates it after two seconds. Skipping only `DT_DIR` does not establish that
an entry is a regular file; `d_type` can also be `DT_UNKNOWN`.

**Suggested solution:** Check actual file metadata, preferably relative to the
opened directory descriptor using `fstatat`. Define a symlink policy explicitly:
reject links, or deliberately resolve them and require a regular target within
the asset root. Never pass FIFOs, sockets, devices or directories to the gzip
callback. For concurrent/untrusted directories, open with appropriate no-follow
and nonblocking protections, then validate the opened descriptor before reading.

**Regression check:** Ordinary and multi-dot assets are selected; FIFO, socket,
directory and disallowed symlink entries are skipped without blocking. Exercise
a `DT_UNKNOWN` fallback rather than relying only on `dirent.d_type`.

### A5 — P2: The batch does not compress every match and counts skips as successes

**Location:** `src/os_operations.c:275–282`; `src/lz77.c:1131–1134`.

**Reproduction:** A directory containing only `page.html` with `{}` prints
`1 files compressed`, returns 0 and creates no `page.html.gz`. The new callback
passes a hardcoded limit of 1,024 bytes and increments `found` before it knows
whether any output was created.

**Suggested solution:** Make the minimum-size policy configurable; default to
compressing all matches if that is the intended contract. Distinguish
`COMPRESSED`, `SKIPPED` and `ERROR` in the callback result and report separate
counts. Clear output-pointer/length results on entry for memory mode. Document
that skipped files require WSER identity fallback if that policy is retained.
A counter change alone does not fix stale outputs in A2.

**Regression check:** Mix a skipped small source, a successful large source and
a failing source; verify accurate counts, output presence and a failing build
status when a required asset could not be generated.

### A6 — P2: The CLI mixes asset generation and ZIP extraction, with wrong exits

**Location:** `src/main.c:70–83`.

**Reproduction:** Running the CLI with no arguments successfully creates a valid
sidecar, then exits 255 with `you gotta specify a file to zip!`. Running it with
`missing.zip` in an empty directory exits 0 despite the failed extraction,
because `test_ZIP()`'s return is ignored.

**Suggested solution:** Separate commands, for example `assets [directory]` and
`unzip archive.zip`. Validate the chosen command before modifying anything;
return that operation's result. The assets command must not require a ZIP file.

**Regression check:** Successful assets-only generation exits 0; invalid usage,
missing input and compression/extraction failures exit nonzero, with no unrelated
operation executed.

### A7 — P2: Windows directory compression silently does nothing

**Location:** `src/os_operations.c:288–296`.

**Evidence:** Source review: the `_WIN32`/`_WIN64` branch is empty, then returns 0
and prints `no files found to compress`. Moving `found` fixed its earlier scope
error, but did not implement scanning. No Windows toolchain/runtime was available
to verify a complete Windows build.

**Suggested solution:** Implement enumeration with `FindFirstFileW` /
`FindNextFileW`, equivalent suffix/type checks and guaranteed `FindClose`, including
error paths. Until supported, return an explicit unsupported-operation failure
and narrow the README's portability claim.

**Regression check:** On Windows, generate and independently decode sidecars for
matching files, skip non-matches/directories, and test Unicode names and failures.

## B. Other confirmed API/configuration issues

These are not prerequisites for a WSER path that only consumes valid on-disk
sidecars, but remain library bugs or configuration traps.

### B1 — P1 for memory callers: Returned gzip length excludes wrapper bytes

**Location:** `src/lz77.c:1195–1197`.

**Reproduction:** For the repository's `lorem.txt` copied to `page.html`, memory
mode returns 5,103, but the complete member is 5,131 bytes. Decoding the returned
length fails with premature EOF; decoding the full member succeeds.

**Suggested solution:** Return `bwritten`, not the raw DEFLATE `comp_size`, with a
clearly documented ownership/length contract. Test exact returned-length

