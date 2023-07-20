#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/sysinfo.h>

#define BUFFER 2048
pthread_mutex_t mutex; // Initializing mutex

// Create struct to easily keep track of each file and its copmressed data
typedef struct {
    char* filename;
    int* compressedData;
    int compressedSize;
    int compressedCapacity;
} ThreadData;

// Creating function for threads to compress data
void *zipThread(void *data) {

    ThreadData *threadData = (ThreadData*) data;

    // Open the file in read binary
    FILE* file = fopen(threadData->filename, "rb");

    // Check if the file was successfully opened
    if (file == NULL) {
        printf("pzip: cannot open file\n"); // Print error message
        exit(1);
    }

    // Initializing variables for keeping count and previous character
    int count = 1;
    int prevChar = -1;
    int currentChar;

    // Read the file character by character until the end
    while ((currentChar = fgetc(file)) != EOF) {

        // If it's the first character of the file we skip it
        if (prevChar == -1) {
            prevChar = currentChar;
            continue;
        }

        // Checking if the current character is the same as the previous character to count consecutive occurrences
        if (currentChar == prevChar) {
            count++;
        } else {

            // Check if there is enough memory allocated if not reallocate more 
            if (threadData->compressedSize + 2 > threadData->compressedCapacity) {
                threadData->compressedCapacity *= 2;
                threadData->compressedData = realloc(threadData->compressedData, sizeof(int) * threadData->compressedCapacity);
            }

            // Store count and charaacter
            threadData->compressedData[threadData->compressedSize++] = count;
            threadData->compressedData[threadData->compressedSize++] = prevChar;
            count = 1; // Reset count for next character
        }

        prevChar = currentChar; // Store current character as previous character
    }

    // Check if there is enough memory allocated for the final character if not reallocate more 
    if (threadData->compressedSize + 2 > threadData->compressedCapacity) {
        threadData->compressedCapacity *= 2;
        threadData->compressedData = realloc(threadData->compressedData, sizeof(int) * threadData->compressedCapacity);
    }

    // Store the final count and charaacter
    threadData->compressedData[threadData->compressedSize++] = count;
    threadData->compressedData[threadData->compressedSize++] = prevChar;
    fclose(file); // Close the input file

    return NULL;
}

int main(int argc, char *argv[]) {
    // Check if there were enough arguments given
    if (argc < 2) {
        printf("pzip: file1 [file2 ...]\n"); // Print error message
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
        if ((threadData[i].compressedData = malloc(sizeof(int) * BUFFER)) == NULL) {
            printf("malloc failed\n");
            exit(1);
        }

        threadData[i].compressedSize = 0;
        threadData[i].compressedCapacity = BUFFER;
    }

    int currentThread = 0; // Initializing a variable to keep track of threads to wait for at the end
    int currentFile = 0; // Initializing a variable to keep track of the current file so we know when to stop
    int filesToCores = numFiles / numCores + 1; // Get the ceiling of numfiles / numcores so we know how many loops we have to do with available cores to cover all files
    // For loop to compress all files parallel with avaible cores/threads
    for (int i = 0; i < filesToCores; i++) {

        // Creating threads to compress their respective file
        for (int j = 0; j < numCores; j++) {
            currentFile++;

            // Checking if we have already gone through all the files
            if (!(currentFile > numFiles)) {
                pthread_create(&threads[j], NULL, zipThread, &threadData[j]);
                currentThread++;
            }
        }

        // Waiting for all the threads to finish before continuing
        for (int j = 0; j < currentThread; j++) {
            pthread_join(threads[j], NULL);
        }
        currentThread = 0; // Reseting for potential next loop 
    }

    // Combining and writing off the compressed data from files in correct order to the stdout (standart output)
    for (int i = 0; i < numFiles; i++) {
        for (int j = 0; j < threadData[i].compressedSize; j += 2) {
            if (i != numFiles - 1 && threadData[i + 1].compressedData[1] == threadData[i].compressedData[threadData[i].compressedSize - 1]) {
                threadData[i + 1].compressedData[0] += threadData[i].compressedData[threadData[i].compressedSize - 2];
                threadData[i].compressedSize -= 2;
            }
        }

        for (int j = 0; j < threadData[i].compressedSize; j += 2) {
            fwrite(&threadData[i].compressedData[j], sizeof(int), 1, stdout);
            fwrite(&threadData[i].compressedData[j + 1], sizeof(char), 1, stdout);
        }
    }

    for (int i = 0; i < numFiles; i++) {
        free(threadData[i].filename); // Freeing the allocated filename memory
        free(threadData[i].compressedData); // Freeing the compressed data buffer
    }

    pthread_mutex_destroy(&mutex); // Destroy mutex

    return 0;
}
