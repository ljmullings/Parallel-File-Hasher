# Parallel-File-Hasher

This program performs a hash calculation on a file using multiple threads. It is designed to efficiently process large files by dividing the work among several threads that compute hashes concurrently. The results from each thread are combined to produce a final hash value for the entire file. The hashing algorithm used is the Jenkins "one-at-a-time" hash.

*Features*
Efficient processing of large files using multi-threading.
Utilizes the Jenkins one-at-a-time hashing algorithm.
Measures and outputs the time taken to compute the hash.
Prerequisites
GCC (GNU Compiler Collection)
POSIX Threads (pthread)
Linux-based operating system with support for POSIX API
Building the Program
To compile the program, navigate to the directory containing the source file and run the following command:

bash
Copy code
gcc -o file_hasher file_hasher.c -lpthread
This command will compile the code and link the pthread library, producing an executable named file_hasher.

Running the Program
To run the program, use the following syntax:

bash
Copy code
./file_hasher <filename> <num_threads>
filename: The path to the file you wish to hash.
num_threads: The number of threads to use for computing the hash.
Example:

bash
Copy code
./file_hasher example.txt 4
This will compute the hash of example.txt using 4 threads.

Output
The program will output the final hash value of the file and the time taken to compute it in seconds.

Implementation Details
The file is divided into blocks of 4096 bytes (defined by BSIZE).
Each thread is responsible for hashing one or more blocks of the file.
Threads are created in a binary tree structure, where each thread can spawn two child threads to further parallelize the work.
The hash values computed by child threads are combined with the parent thread's hash value to produce a final result.
Limitations
The program is designed for POSIX-compliant systems and might not run as expected on non-POSIX systems.
The maximum number of threads that can effectively be used depends on the size of the file and the system capabilities.
