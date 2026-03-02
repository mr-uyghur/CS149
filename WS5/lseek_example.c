#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

void func()
{
    char buffer;
    // Open the file for READ only [cite: 11]
    int f_read = open("start.txt", O_RDONLY);
    
    // Open the file for WRITE, create if it doesn't exist [cite: 11]
    int f_write = open("end.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);

    if (f_read == -1 || f_write == -1) {
        return;
    }

    // Read 1 byte at a time
    while (read(f_read, &buffer, 1) > 0)
    {
        // Write the character we just read to the output file
        write(f_write, &buffer, 1);

        // To get the (1+3i)th character, we skip 2 bytes 
        // SEEK_CUR moves the pointer relative to the current position [cite: 12]
        lseek(f_read, 2, SEEK_CUR);
    }

    close(f_write);
    close(f_read);
}

int main()
{
    func();
    return 0;
}