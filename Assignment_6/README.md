# Assignment 6 — Multi-threaded `countnames`

## Student Name(s)

Frederick Nguyen (quangnguyen1705@gmail.com) | Ali Halmamat (ali.Halmamat@sjsu.edu)

---

## How to Compile

```bash
gcc -o shell shell.c -Wall -Werror
gcc -D_REENTRANT -pthread -o countnames countnames.c -Wall -Werror
```

Both commands must produce **zero** warnings or errors.

---

## How to Run the Test Cases

All test input files live under the `test/` directory.

```bash
# Test 1 — two input files (core functionality)
./countnames test/names1.txt test/names2.txt

# Test 2 — single input file
./countnames test/names1.txt

# Test 3 — one non-existent file + one valid file
./countnames test/nonexistent.txt test/names1.txt

# Test 4 — three files (max for this assignment)
./countnames test/names1.txt test/names2.txt test/names3.txt

# Test 5 — empty file + valid file
./countnames test/names_empty.txt test/names1.txt

# Test 6 — via shell wrapper (same as Test 1 but through shell)
./shell test/names1.txt test/names2.txt
```

---

## Expected Output for Each Test Case

### Test 1 — Two input files

```
Tom Wu: 4
Jenn Xu: 2
Alice Lee: 1
Bob Smith: 1
```

*(names1.txt has Tom Wu ×2, Jenn Xu ×1, Alice Lee ×1;
  names2.txt has Tom Wu ×2, Jenn Xu ×1, Bob Smith ×1)*

### Test 2 — Single input file

```
Tom Wu: 2
Jenn Xu: 1
Alice Lee: 1
```

### Test 3 — Non-existent file + valid file

stderr:
```
Error: cannot open file 'test/nonexistent.txt'
```
stdout:
```
Tom Wu: 2
Jenn Xu: 1
Alice Lee: 1
```
The program prints an error for the missing file but continues processing the valid file.

### Test 4 — Three files

```
Tom Wu: 4
Jenn Xu: 2
Alice Lee: 4
Bob Smith: 1
```

*(names3.txt adds Alice Lee ×3)*

### Test 5 — Empty file + valid file

```
Tom Wu: 2
Jenn Xu: 1
Alice Lee: 1
```

The empty file contributes nothing; the valid file is processed normally.

### Test 6 — Via shell wrapper

Same as Test 1:
```
Tom Wu: 4
Jenn Xu: 2
Alice Lee: 1
Bob Smith: 1
```

---

## What Each Test Case Is Testing

| # | What it tests |
|---|---------------|
| 1 | Core multi-threaded aggregation across two files |
| 2 | Correct single-thread / single-file execution path |
| 3 | Graceful error handling for a non-existent file; program continues |
| 4 | All three threads running concurrently; correct count across three files |
| 5 | Edge case: empty input file produces no output entries |
| 6 | Shell wrapper correctly forks and execs `countnames` |

---

## Memory Leak Check

```bash
gcc -D_REENTRANT -pthread -g -o countnames_dbg countnames.c -Wall -Werror
valgrind --leak-check=full ./countnames_dbg test/names1.txt test/names2.txt
```

Expected: `All heap blocks were freed -- no leaks are possible`.

---

# Lessons Learned

- **pthread_create / pthread_join** — creating threads is straightforward, but every created thread must be joined (or detached) to avoid resource leaks.
- **Mutex locking scope** — the lock must be held for the entire read-modify-write on the shared array (search + increment or insert), not just the increment, to prevent TOCTOU (time-of-check-time-of-use) races.
- **getline memory management** — `getline` allocates (or re-allocates) the buffer internally; only a single `free(line)` is needed after the loop, regardless of how many lines were read.
- **Thread-safety vs. correctness** — running threads truly concurrently exposes subtle ordering issues; using `PTHREAD_MUTEX_INITIALIZER` for static initialization keeps the code simple and avoids `pthread_mutex_init` / `pthread_mutex_destroy` sequencing bugs.

---

# References

- `man 3 pthread_create`, `man 3 pthread_mutex_lock`  
- `man 3 getline`  
- POSIX Threads Programming — https://hpc-tutorials.llnl.gov/posix/  
- Stack Overflow: proper use of `getline` — https://stackoverflow.com/questions/42478868/how-do-i-properly-free-memory-related-to-getline-function

---

# Acknowledgements

*(List any people or AI tools consulted here, per your course policy.)*
