#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/sysinfo.h>

#define BUFFER 2048

pthread_mutex_t mutex; // Initializing mutex

// Create struct to easily keep track of each file and its uncompressed data
typedef struct {
    char* filename;
    char* uncompressedData;
    int uncompressedSize;
    int uncompressedCapacity;
} ThreadData;

// Creating function for threads to decompress data
void *unzipThread(void *data) {

    ThreadData *threadData = (ThreadData*) data;

    // Open the file in read binary
    FILE *file = fopen(threadData->filename, "rb");

    // Check if the file was successfully opened
    if (file == NULL) {
        printf("punzip: cannot open file\n"); // Print error message
        exit(1);
    }
    
    int count; // Initializing count to store the count of the characters
    char character; // Initializing character to store the character

    // Reading the count and character and storing them in variables count and character
    while (fread(&count, sizeof(int), 1, file) == 1) {
        fread(&character, sizeof(char), 1, file);

        // For loop for storing each character their respective amount of count times 
        for (int j = 0; j < count; j++) {
            // Check if there is enough memory allocated if not reallocate more
            if (threadData->uncompressedSize + 1 > threadData->uncompressedCapacity) {
                threadData->uncompressedCapacity *= 2;
                threadData->uncompressedData = realloc(threadData->uncompressedData, sizeof(char) * threadData->uncompressedCapacity);
            }
            threadData->uncompressedData[threadData->uncompressedSize++] = character;
        }
    }

    fclose(file); // Closing the input file

    return NULL;
}

int main(int argc, char *argv[]) {
    // Check if there were enough arguments given
    if (argc < 2) {
        printf("punzip: file1 [file2 ...]\n"); // Print error message
        exit(1);
    }

    int numCores = get_nprocs(); // Get the number of CPU cores
    int numFiles = argc - 1; // Number of input files

    pthread_t threads[numFiles]; // Create thread variables
    ThreadData threadData[numFiles]; // Create thread data structures

    // Initialize the thread data structures
    for (int i = 0; i < numFiles; i++) {
        threadData[i].filename = strdup(argv[i + 1]);

        // Initial buffer size
        threadData[i].uncompressedData = malloc(sizeof(char) * BUFFER);
        if (threadData[i].uncompressedData == NULL) {
            printf("malloc failed\n"); // Print error message
            exit(1);
        }

        threadData[i].uncompressedSize = 0;
        threadData[i].uncompressedCapacity = BUFFER;
    }

    int currentThread = 0; // Initializing a variable to keep track of threads to wait for at the end
    int currentFile = 0; // Initializing a variable to keep track of the current file so we know when to stop
    int filesToCores = numFiles / numCores + 1; // Get the ceiling of numfiles / numcores so we know how many loops we have to do with available cores to cover all files
    // For loop to decompress all files in parallel with available cores/threads
    for (int i = 0; i < filesToCores; i++) {

        // Creating threads to decompress their respective file
        for (int j = 0; j < numCores; j++) {
            currentFile++;

            // Checking if we have already gone through all the files
            if (!(currentFile > numFiles)) {
                pthread_create(&threads[j], NULL, unzipThread, &threadData[j]);
                currentThread++;
            }
        }

        // Waiting for all the threads to finish before continuing
        for (int j = 0; j < currentThread; j++) {
            pthread_join(threads[j], NULL);
        }
        currentThread = 0; // Resetting for potential next loop 
    }

    // Write the uncompressed data to the stdout (standart output)
    for (int i = 0; i < numFiles; i++) {
        fwrite(threadData[i].uncompressedData, sizeof(char), threadData[i].uncompressedSize, stdout);
    }

    // For loop to free all dynamically allocated memory
    for (int i = 0; i < numFiles; i++) {
        free(threadData[i].filename);
        free(threadData[i].uncompressedData);
    }

    pthread_mutex_destroy(&mutex); // Destroying mutex

    return 0;
}
