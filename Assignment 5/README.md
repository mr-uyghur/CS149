## Assignment 5 – countnames with Dynamic Memory Allocation

**Student Name:** Frederick Nguyen
**Student Id:** 016467373

**Student Name:** Ali Halmamat
**Student Id:** 017505410

---

## How to Compile

```bash
gcc -o shell shell.c -Wall -Werror
gcc -o countnames countnames.c -Wall -Werror
```

Both programs compile with zero warnings and zero errors.

To compile with debug symbols for valgrind:
```bash
gcc -g -Wall -Werror -o shell shell.c
gcc -g -Wall -Werror -o countnames countnames.c
```

---

## How to Run the Test Cases

### Test Case 1 – Two Files with Overlapping Names
```bash
./shell test/names1.txt test/names2.txt
```
**Expected output:**
```
Tom Wu: 4
Yuan Cheng Chang: 2
Jenn Xu: 2
Dave Joe: 2
```
**What it tests:** Aggregation across two parallel child processes. Names shared
between files have their counts correctly summed in the parent's malloc'd hash table.

---

### Test Case 2 – Single Input File
```bash
./shell test/names3.txt
```
**Expected output:**
```
Maria Garcia: 3
John Smith: 2
```
**What it tests:** Correct behavior with exactly one child process. Verifies
strdup copies each name into a fresh heap allocation and hash table inserts correctly.

---

### Test Case 3 – All Unique Names (No Duplicates)
```bash
./shell test/names4.txt
```
**Expected output:**
```
Delta Four: 1
Beta Two: 1
Gamma Three: 1
Alpha One: 1
```
**What it tests:** Every name appears once. Every NameCountData node is created
with count = 1 and no false duplicates.

---

### Test Case 4 – Large File (100 Lines, 20 Distinct Names)
```bash
./shell test/names_long.txt
```
**Expected output:** All names , each with count 1.

**What it tests:** Initial bucket count is 10. With 20 distinct names, realloc
is triggered to double the bucket array to 20 and all nodes are rehashed.

---

### Test Case 5 – Three Input Files (Maximum per spec)
```bash
./shell test/names1.txt test/names2.txt test/names3.txt
```
**Expected output:**
```
Tom Wu: 4
Maria Garcia: 3
Yuan Cheng Chang: 2
Jenn Xu: 2
John Smith: 2
Dave Joe: 2
```
**What it tests:** Three simultaneous children. Verifies parallel spawning
(all forks before any wait) and aggregation across all three mmap segments.

---

## Running with valgrind

```bash
valgrind --leak-check=full ./shell test/names1.txt test/names2.txt
valgrind --leak-check=full ./shell test/names_long.txt
```

Expected: `definitely lost: 0 bytes in 0 blocks`

---

# Lessons Learned

- strdup allocates and copies; never pre-malloc the destination pointer.
- realloc may return a different pointer; the old pointer is invalid afterward.
  The compiler catches this with -Werror=use-after-free.
- Hash table resize requires rehashing every node, not just extending the array.
- wait() reaps any child; waitpid(pids[i]) forces fixed order and blocks needlessly.
- Track children_spawned separately so partial fork failures reap exactly the right count.
- A unified cleanup() on every error path eliminates whole classes of memory leaks.
- free(NULL) is safe in C99; initialise pointers to NULL for safe partial cleanup.
- fd_str must be built with snprintf BEFORE free(shm_fds) in the child process.
- MAP_SHARED + wait() guarantees writes are visible to the parent with no msync needed.

---

# References

- man 3 strdup, man 3 getline, man 3 realloc, man 2 memfd_create, man 2 wait
- https://stackoverflow.com/questions/42478868/how-do-i-properly-free-memory-related-to-getline-function
- https://www.geeksforgeeks.org/strdup-strdndup-functions-c/
- Course slides: hash table with double pointer, parallel process spawning

---

# Acknowledgements

