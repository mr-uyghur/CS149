# Assignment 4 — Shared Memory with mmap

**Student Name:** Frederick Nguyen
**Student Id:** 016467373

**Student Name:** Ali Halmamat
**Student Id:** 017505410


---

## Overview

This assignment extends A1–A3 by replacing pipe-based IPC with **shared memory** using `mmap()`.  
The parent (`shell.c`) allocates a large `GLOBAL` shared memory region, divides it into per-child  
sub-regions, spawns one `countnames` child per input file, and then aggregates all results into a  
summation region before printing the final combined name counts.

---

## How to Compile

```bash
gcc -o countnames countnames.c -Wall -Werror
gcc -o shell      shell.c      -Wall -Werror
```
---

## How to Run

### Shell (A4 mode — shared memory, multiple files)

```bash
./shell test/names1.txt test/names2.txt test/names3.txt
```

### countnames standalone (A1 mode — single file)

```bash
./countnames test/names1.txt
```

### countnames from stdin

```bash
cat test/names1.txt test/names2.txt | ./countnames
```

---

## Test Cases

---

### Test Case 1 — Multiple files, overlapping names (provided files)

**Command:**
```bash
./shell test/names1.txt test/names2.txt test/names3.txt
```

**What it tests:**  
Core A4 functionality: three child processes run in parallel, each writing results to their own  
shared memory region. The parent aggregates all three and sums overlapping names correctly.  
Also verifies that an empty line in `names1.txt` triggers a stderr warning.

**Expected output (stdout):**
```
[shell] Spawned child PID <pid1> for file: test/names1.txt
[shell] Spawned child PID <pid2> for file: test/names2.txt
[shell] Spawned child PID <pid3> for file: test/names3.txt
[shell] Child PID <pidX> (file: ...) exited with code 0
[shell] Child PID <pidX> (file: ...) exited with code 0
[shell] Child PID <pidX> (file: ...) exited with code 0

=== Final combined name counts ===
Nicky: 1
Dave Joe: 3
John Smith: 2
Yuan Cheng Chang: 4
Alice: 2
Bob: 2
Carol: 2
```

**Expected stderr:**
```
Warning - file test/names1.txt line 2 is empty.
```

**Verification (ground truth):**
```bash
cat test/names1.txt test/names2.txt test/names3.txt | grep -v '^$' | sort | uniq -c
```

---

### Test Case 2 — Single file with empty line (provided file, standalone)

**Command:**
```bash
./countnames test/names1.txt
```

**What it tests:**  
Standalone A1-compatible mode. Verifies empty line detection, name counting, and correct output  
format (`name: count`) without invoking the shell or shared memory.

**Expected stdout:**
```
Nicky: 1
Dave Joe: 2
John Smith: 1
Yuan Cheng Chang: 3
```

**Expected stderr:**
```
Warning - file test/names1.txt line 2 is empty.
```

---

### Test Case 3 — Nonexistent file (error handling)

**Command:**
```bash
./shell test/nonexistent.txt
```

**What it tests:**  
When a child cannot open its file it prints the required error message to stderr and exits with  
code 1. The parent still waits correctly and reports the child's exit code.  
The summation region remains empty (no output names), but the parent exits with code 0  
(it successfully ran; the child failure is reported separately).

**Expected stdout:**
```
[shell] Spawned child PID <pid> for file: test/nonexistent.txt
[shell] Child PID <pid> (file: test/nonexistent.txt) exited with code 1

=== Final combined name counts ===
```

**Expected stderr:**
```
error: cannot open file test/nonexistent.txt
```

---

### Test Case 4 (own) — File containing only empty lines

**Command:**
```bash
./shell test/empty_lines.txt
```

**What it tests:**  
Edge case: a file with no valid names at all (every line is empty). The child should warn for  
each empty line and produce zero name-count entries. The summation region stays empty.  
Exit code must be 0.

**`test/empty_lines.txt` contents:** two blank lines.

**Expected stdout:**
```
[shell] Spawned child PID <pid> for file: test/empty_lines.txt
[shell] Child PID <pid> (file: test/empty_lines.txt) exited with code 0

=== Final combined name counts ===
```

**Expected stderr:**
```
Warning - file test/empty_lines.txt line 1 is empty.
Warning - file test/empty_lines.txt line 2 is empty.
```

---

### Test Case 5 (own) — Single name repeated many times

**Command:**
```bash
./shell test/single_name.txt
```

**What it tests:**  
Verifies the counting logic correctly accumulates a single distinct name appearing multiple times.

**`test/single_name.txt` contents:**
```
Alice
Alice
Alice
```

**Expected stdout:**
```
[shell] Spawned child PID <pid> for file: test/single_name.txt
[shell] Child PID <pid> (file: test/single_name.txt) exited with code 0

=== Final combined name counts ===
Alice: 3
```

---

### Test Case 6 (own) — Names containing spaces and a whitespace-only line

**Command:**
```bash
./shell test/spaces.txt
```

**What it tests:**  
Per the spec, a line consisting of only whitespace characters (e.g., a single space `" "`) is  
treated as a **valid name** (not an empty line), since `strlen(" ") >= 1`. This test ensures  
multi-word names like `"Tom Wu"` and `"Ann Lee"` are treated as single names, and that a  
space-only line counts as its own distinct name.

**`test/spaces.txt` contents:**
```
Tom Wu
Tom Wu
 
Ann Lee
Tom Wu
```

**Expected stdout:**
```
[shell] Spawned child PID <pid> for file: test/spaces.txt
[shell] Child PID <pid> (file: test/spaces.txt) exited with code 0

=== Final combined name counts ===
Tom Wu: 3
 : 1
Ann Lee: 1
```

---

## Shared Memory Design

```
GLOBAL shared memory (mmap via memfd_create)
┌─────────────────┬─────────────────┬─────────────────┬─────────────────────┐
│ Child 0 region  │ Child 1 region  │ Child 2 region  │ Summation region    │
│ (names1.txt)    │ (names2.txt)    │ (names3.txt)    │ (parent aggregates) │
│ MAX_NAMES slots │ MAX_NAMES slots │ MAX_NAMES slots │ MAX_NAMES slots     │
└─────────────────┴─────────────────┴─────────────────┴─────────────────────┘
 offset=0          offset=1*region   offset=2*region   offset=n*region
```

- Each region is `MAX_NAMES * sizeof(NameCountData)` bytes.
- A sentinel entry (name[0] == '\0') marks the end of valid data in each region.
- The `memfd_create()` syscall creates an fd-backed anonymous memory object that  
  survives `execvp()`, so children can re-`mmap()` using the inherited fd number  
  passed via `argv`.

---

## IPC Method Used

**mmap with `memfd_create`** (Option 1 — parent aggregates results).

The parent creates shared memory using `memfd_create()` which returns a real file descriptor  
backed by anonymous memory. This fd is inherited across `fork()` and also across `execvp()`  
(since `FD_CLOEXEC` is not set). The parent passes the fd number, byte offset, and region  
size to each child as command-line arguments. Each child calls `mmap()` on the inherited fd  
to access its assigned sub-region and writes its name counts directly there.

---

# Lessons Learned

- Anonymous `mmap` regions (`MAP_ANONYMOUS | MAP_SHARED`) are **not preserved across `execvp()`**, because `exec` replaces the entire process image. To share memory after exec, you need either a real fd (e.g., from `memfd_create` or `shm_open`) that survives exec.
- `mmap` with a non-zero `offset` argument requires the offset to be a multiple of the system page size. The workaround used here is to always map from offset 0 and advance the pointer manually in C using byte arithmetic (`(char *)map + offset`).
- `wait()` in a loop (rather than `waitpid` for a specific PID) allows the parent to reap children in the order they finish — which matches the "no bottleneck" requirement: a fast child doesn't wait behind a slow one.
- `memset(region, 0, region_size)` before writing is critical to ensure the sentinel (`name[0] == '\0'`) is correctly placed after the last valid entry.

---

# References

- Assignment 1–4 PDFs (course materials)
- `man 2 mmap`, `man 2 fork`, `man 2 execvp`, `man 2 wait`, `man 2 memfd_create`
- Linux man pages online: https://man7.org/linux/man-pages/
- Course slides on processes, IPC, and shared memory
- https://man7.org/linux/man-pages/man2/memfd_create.2.html
- https://www.tutorialspoint.com/c_standard_library/c_function_fgets.htm

---

# Acknowledgements

No direct help received from other students. Consulted Linux man pages and course slides  
for `mmap`, `memfd_create`, `fork`, `execvp`, and `wait` usage.
