
# Assignment 3–Parallel Count Names with IPC (pipe)
**Student Name:** Frederick Nguyen
**Student Id:** 016467373

**Student Name:** Ali Halmamat
**Student Id:** 017505410

---

## 📌 Description

This assignment extends Assignment 2 by enabling communication between the parent process (shell) and child processes (countnames).

Each child:
- Reads one input file
- Counts occurrences of names
- Writes results to:
  - `<PID>.out`
  - `<PID>.err`
- Sends structured data back to the parent via a pipe

The parent:
- Spawns all children in parallel
- Uses pipe() and dup2() to receive data
- Waits for children using wait()
- Aggregates results from all children
- Prints final combined counts to stdout

---

## ⚙️ Compilation

```bash
gcc -o countnames countnames.c -Wall -Werror
gcc -o shell1 shell1.c -Wall -Werror
```
---

## ▶️ Running
Command:
```bash
./shell1
```
Then enter commands like:

```bash
./countnames test/test1.txt test/test2.txt
```
---

## 🧪 Test Cases

### ✅ Test Case 1 – Multiple repeated names

Command:
```bash
./shell1
% ./countnames test/test1.txt test/test2.txt
```
Expected Output:
- Child 425 terminated normally with exit code: 0
- Child 426 terminated normally with exit code: 0
- Dave Joe: 3
- Tom Wu: 3
- Jenn Xu: 3

What It Tests:
- Duplicate name counting
- Multi-word names
- Correct formatting of output

---
### ✅ Test Case 2 – Missing file

Command:
```bash
./shell1
% ./countnames test/missing.txt test/names1.txt
```
Expected Output:
- Child 441 terminated normally with exit code: 1
- Child 442 terminated normally with exit code: 0
- Tom Wu: 3

What It Tests:
- Error handling when one of the input files does not exist
- Verifies the failing child exits with code 1 while the other child succeeds with code 0
- Confirms the parent still aggregates and prints results from the successful child

### ✅ Test Case 3 – Single file

Command:
```bash
./shell1
% ./countnames test/names2.txt
```
Expected Output:
- Child 456 terminated normally with exit code: 0
- Jenn Xu: 2
- Tom Wu: 1

### ✅ Test Case 4 – Empty line handling
Command:
```bash
./shell1
% ./countnames test/names2.txt test/namesB.txt
```

Expected Output:
- Child 469 terminated normally with exit code: 0
- Child 470 terminated normally with exit code: 0
- Jenn Xu: 2
- Tom Wu: 1
- Nicky: 1
- Dave Joe: 2
- Yuan Cheng Chang: 3
- John Smith: 1

--- 
## 🧠 Lessons Learned
- Learned how to use fork(), exec(), wait() together.
- Understood parent-child communication using pipe().
- Learned how dup2() redirects stdout.
- Implemented structured communication using MessageHeader.
- Practiced handling concurrent processes.

---

## 📚 References
- Course lecture slides (Process API, IPC)
    - man pages:
    - man 2 fork
    - man 2 pipe
    - man 2 dup2
    - man 2 wait
- https://man7.org/

---

## 🙏 Acknowledgements
- Course instructors and TAs
- zyLab environment and documentation

