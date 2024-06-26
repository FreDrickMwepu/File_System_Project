#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILENAME_LENGTH 100
#define MAX_FILE_COUNT 100

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    int size;
} File;

typedef struct {
    File files[MAX_FILE_COUNT];
    int count;
} FileSystem;

void initializeFileSystem(FileSystem* fs) {
    fs->count = 0;
}

void createFile(FileSystem* fs, const char* name, int size) {
    if (fs->count >= MAX_FILE_COUNT) {
        printf("File system is full. Cannot create file.\n");
        return;
    }

    if (strlen(name) >= MAX_FILENAME_LENGTH) {
        printf("File name is too long. Cannot create file.\n");
        return;
    }

    File newFile;
    strcpy(newFile.name, name);
    newFile.size = size;

    fs->files[fs->count++] = newFile;

    printf("File created successfully.\n");
}

void listFiles(FileSystem* fs) {
    if (fs->count == 0) {
        printf("No files in the file system.\n");
        return;
    }

    printf("Files in the file system:\n");
    for (int i = 0; i < fs->count; i++) {
        printf("%s (%d bytes)\n", fs->files[i].name, fs->files[i].size);
    }
}

int main() {
    FileSystem fs;
    initializeFileSystem(&fs);

    createFile(&fs, "file1.txt", 100);
    createFile(&fs, "file2.txt", 200);
    createFile(&fs, "file3.txt", 150);

    listFiles(&fs);

    return 0;
}