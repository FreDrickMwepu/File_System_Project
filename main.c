#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 512
#define SECTOR_SIZE 64
#define SECTORS_PER_BLOCK (BLOCK_SIZE / SECTOR_SIZE)
#define MAX_BLOCKS 1024
#define MAX_FILES 100
#define MAX_FOLDERS 50
#define FILES_PER_FOLDER 20
#define FOLDERS_PER_FOLDER 10
#define DIRECT_BLOCKS 10
#define INDEX_BLOCK_SIZE (BLOCK_SIZE / sizeof(int))

typedef struct {
    char data[SECTOR_SIZE];
} Sector;

typedef struct {
    Sector sectors[SECTORS_PER_BLOCK];
} Block;

typedef struct {
    int pointers[INDEX_BLOCK_SIZE];
} IndexBlock;

struct Folder;

typedef struct {
    char name[20];
    int size;
    int direct[DIRECT_BLOCKS];
    int singleIndirect;
    int doubleIndirect;
    struct Folder* parentFolder;
} File;

typedef struct Folder {
    char name[20];
    struct Folder* parentFolder;
    File* files[FILES_PER_FOLDER];
    struct Folder* subFolders[FOLDERS_PER_FOLDER];
    int fileCount;
    int folderCount;
} Folder;

Folder rootFolder;
Folder* currentFolder;
File files[MAX_FILES];
IndexBlock indexBlocks[MAX_BLOCKS];
Block blocks[MAX_BLOCKS];
int fileCount = 0;
int folderCount = 0;
int freeBlockIndex = 0;

GtkWidget *outputLabel;

void createFolder(Folder* parent, const char* folderName);
void createFile(Folder* folder, const char* fileName);
void writeFile(Folder* folder, const char* fileName, const char* data);
void readFile(Folder* folder, const char* fileName);
void deleteFile(Folder* folder, const char* fileName);
void listFilesInFolder(Folder* folder);
void listSubFolders(Folder* folder);
Folder* findSubFolder(Folder* parent, const char* folderName);
void navigateToFolder(Folder* parent, const char* folderName);
void exitFolder();
void deleteDirectory(Folder* parentFolder, const char* folderName);
void updateOutputLabel(const char *message);

void clearInputBuffer() {
    while (getchar() != '\n');
}

int allocateBlock() {
    if (freeBlockIndex >= MAX_BLOCKS) {
        updateOutputLabel("Error: No more blocks available.\n");
        return -1;
    }
    return freeBlockIndex++;
}

void allocateSector(int blockIndex, int sectorIndex, const char* data, int dataSize) {
    strncpy(blocks[blockIndex].sectors[sectorIndex].data, data, dataSize);
}

Folder* findSubFolder(Folder* parent, const char* folderName) {
    for (int i = 0; i < parent->folderCount; ++i) {
        if (strcmp(parent->subFolders[i]->name, folderName) == 0) {
            return parent->subFolders[i];
        }
    }
    return NULL;
}

void updateOutputLabel(const char *message) {
    gtk_label_set_text(GTK_LABEL(outputLabel), message);

    int len = strlen(message);
    PangoFontDescription *font_desc = pango_font_description_new();
    pango_font_description_set_family(font_desc, "Monospace");

    if (len < 50) {
        pango_font_description_set_size(font_desc, 14 * PANGO_SCALE);
    } else if (len < 100) {
        pango_font_description_set_size(font_desc, 12 * PANGO_SCALE);
    } else {
        pango_font_description_set_size(font_desc, 10 * PANGO_SCALE);
    }

    gtk_widget_override_font(outputLabel, font_desc);
    pango_font_description_free(font_desc);
}

void on_create_folder_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = (GtkWidget*)data;
    const char* folderName = gtk_entry_get_text(GTK_ENTRY(entry));
    createFolder(currentFolder, folderName);
}

void createFolder(Folder* parent, const char* folderName) {
    if (parent->folderCount >= FOLDERS_PER_FOLDER) {
        updateOutputLabel("Error: Maximum number of subfolders in this folder reached.\n");
        return;
    }

    Folder* newFolder = (Folder*)malloc(sizeof(Folder));
    if (!newFolder) {
        updateOutputLabel("Error: Memory allocation failed.\n");
        return;
    }
    strcpy(newFolder->name, folderName);
    newFolder->parentFolder = parent;
    newFolder->fileCount = 0;
    newFolder->folderCount = 0;

    parent->subFolders[parent->folderCount++] = newFolder;
    folderCount++;
    updateOutputLabel("Folder created successfully.\n");
}

void on_create_file_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = (GtkWidget*)data;
    const char* fileName = gtk_entry_get_text(GTK_ENTRY(entry));
    createFile(currentFolder, fileName);
}

void createFile(Folder* folder, const char* fileName) {
    for (int i = 0; i < folder->fileCount; ++i) {
        if (strcmp(folder->files[i]->name, fileName) == 0) {
            updateOutputLabel("Error: A file with the same name already exists.\n");
            return;
        }
    }

    if (folder->fileCount >= FILES_PER_FOLDER) {
        updateOutputLabel("Error: Maximum number of files in folder reached.\n");
        return;
    }
    if (fileCount >= MAX_FILES) {
        updateOutputLabel("Error: Maximum number of files reached.\n");
        return;
    }

    File* newFile = &files[fileCount];
    strcpy(newFile->name, fileName);
    newFile->size = 0;
    newFile->parentFolder = folder;

    for (int i = 0; i < DIRECT_BLOCKS; ++i) {
        newFile->direct[i] = -1;
    }
    newFile->singleIndirect = -1;
    newFile->doubleIndirect = -1;

    folder->files[folder->fileCount++] = newFile;
    fileCount++;

    updateOutputLabel("File created successfully.\n");
}

void on_write_file_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget **entries = (GtkWidget**)data;
    const char* fileName = gtk_entry_get_text(GTK_ENTRY(entries[0]));
    const char* fileData = gtk_entry_get_text(GTK_ENTRY(entries[1]));
    writeFile(currentFolder, fileName, fileData);
}

void writeFile(Folder* folder, const char* fileName, const char* data) {
    File* file = NULL;
    for (int i = 0; i < folder->fileCount; ++i) {
        if (strcmp(folder->files[i]->name, fileName) == 0) {
            file = folder->files[i];
            break;
        }
    }
    if (file == NULL) {
        updateOutputLabel("Error: File not found.\n");
        return;
    }

    int dataSize = strlen(data);
    int blocksNeeded = (dataSize + SECTOR_SIZE - 1) / SECTOR_SIZE;
    if (blocksNeeded > DIRECT_BLOCKS) {
        updateOutputLabel("Error: File too large to write using direct blocks.\n");
        return;
    }

    file->size = dataSize;

    int currentIndex = 0;
    for (int i = 0; i < blocksNeeded; ++i) {
        int blockIndex = allocateBlock();
        if (blockIndex == -1) {
            updateOutputLabel("Error: Unable to allocate block.\n");
            return;
        }
        file->direct[i] = blockIndex;

        for (int j = 0; j < SECTORS_PER_BLOCK && currentIndex < dataSize; ++j) {
            int remainingData = dataSize - currentIndex;
            int writeSize = remainingData < SECTOR_SIZE ? remainingData : SECTOR_SIZE;
            allocateSector(blockIndex, j, data + currentIndex, writeSize);
            currentIndex += writeSize;
        }
    }

    updateOutputLabel("Data written to file successfully.\n");
}

void on_read_file_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = (GtkWidget*)data;
    const char* fileName = gtk_entry_get_text(GTK_ENTRY(entry));
    readFile(currentFolder, fileName);
}

void readFile(Folder* folder, const char* fileName) {
    File* file = NULL;
    for (int i = 0; i < folder->fileCount; ++i) {
        if (strcmp(folder->files[i]->name, fileName) == 0) {
            file = folder->files[i];
            break;
        }
    }
    if (file == NULL) {
        updateOutputLabel("Error: File not found.\n");
        return;
    }

    char* data = (char*)malloc(file->size + 1);
    if (!data) {
        updateOutputLabel("Error: Memory allocation failed.\n");
        return;
    }

    int currentIndex = 0;
    for (int i = 0; i < DIRECT_BLOCKS && currentIndex < file->size; ++i) {
        if (file->direct[i] == -1) break;

        for (int j = 0; j < SECTORS_PER_BLOCK && currentIndex < file->size; ++j) {
            int remainingData = file->size - currentIndex;
            int readSize = remainingData < SECTOR_SIZE ? remainingData : SECTOR_SIZE;
            strncpy(data + currentIndex, blocks[file->direct[i]].sectors[j].data, readSize);
            currentIndex += readSize;
        }
    }
    data[file->size] = '\0';
    updateOutputLabel(data);
    free(data);
}

void on_delete_file_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = (GtkWidget*)data;
    const char* fileName = gtk_entry_get_text(GTK_ENTRY(entry));
    deleteFile(currentFolder, fileName);
}

void deleteFile(Folder* folder, const char* fileName) {
    for (int i = 0; i < folder->fileCount; ++i) {
        if (strcmp(folder->files[i]->name, fileName) == 0) {
            for (int j = i; j < folder->fileCount - 1; ++j) {
                folder->files[j] = folder->files[j + 1];
            }
            folder->fileCount--;
            updateOutputLabel("File deleted successfully.\n");
            return;
        }
    }
    updateOutputLabel("Error: File not found.\n");
}

void on_list_files_clicked(GtkWidget *widget, gpointer data) {
    listFilesInFolder(currentFolder);
}

void listFilesInFolder(Folder* folder) {
    char buffer[1024] = "Files:\n";
    for (int i = 0; i < folder->fileCount; ++i) {
        strcat(buffer, folder->files[i]->name);
        strcat(buffer, "\n");
    }
    updateOutputLabel(buffer);
}

void on_delete_directory_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = (GtkWidget*)data;
    const char* folderName = gtk_entry_get_text(GTK_ENTRY(entry));
    deleteDirectory(currentFolder, folderName);
}

void deleteDirectory(Folder* parentFolder, const char* folderName) {
    for (int i = 0; i < parentFolder->folderCount; ++i) {
        if (strcmp(parentFolder->subFolders[i]->name, folderName) == 0) {
            for (int j = i; j < parentFolder->folderCount - 1; ++j) {
                parentFolder->subFolders[j] = parentFolder->subFolders[j + 1];
            }
            parentFolder->folderCount--;
            updateOutputLabel("Directory deleted successfully.\n");
            return;
        }
    }
    updateOutputLabel("Error: Directory not found.\n");
}

void on_navigate_folder_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = (GtkWidget*)data;
    const char* folderName = gtk_entry_get_text(GTK_ENTRY(entry));
    navigateToFolder(currentFolder, folderName);
}

void navigateToFolder(Folder* parent, const char* folderName) {
    if (strcmp(folderName, "..") == 0) {
        if (currentFolder->parentFolder != NULL) {
            currentFolder = currentFolder->parentFolder;
            updateOutputLabel("Navigated to parent folder.\n");
        } else {
            updateOutputLabel("Error: Already in root folder.\n");
        }
    } else {
        Folder* subFolder = findSubFolder(parent, folderName);
        if (subFolder != NULL) {
            currentFolder = subFolder;
            updateOutputLabel("Navigated to subfolder.\n");
        } else {
            updateOutputLabel("Error: Subfolder not found.\n");
        }
    }
}

void on_exit_folder_clicked(GtkWidget *widget, gpointer data) {
    exitFolder();
}

void exitFolder() {
    if (currentFolder->parentFolder != NULL) {
        currentFolder = currentFolder->parentFolder;
        updateOutputLabel("Exited to parent folder.\n");
    } else {
        updateOutputLabel("Error: Already in root folder.\n");
    }
}

void on_list_subfolders_clicked(GtkWidget *widget, gpointer data) {
    listSubFolders(currentFolder);
}

void listSubFolders(Folder* folder) {
    char buffer[1024] = "Subfolders:\n";
    for (int i = 0; i < folder->folderCount; ++i) {
        strcat(buffer, folder->subFolders[i]->name);
        strcat(buffer, "\n");
    }
    updateOutputLabel(buffer);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    rootFolder.parentFolder = NULL;
    rootFolder.fileCount = 0;
    rootFolder.folderCount = 0;
    strcpy(rootFolder.name, "root");
    currentFolder = &rootFolder;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Filesystem Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    GtkWidget *createFolderButton = gtk_button_new_with_label("Create Folder");
    g_signal_connect(createFolderButton, "clicked", G_CALLBACK(on_create_folder_clicked), entry);
    gtk_box_pack_start(GTK_BOX(hbox), createFolderButton, FALSE, FALSE, 0);

    GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    GtkWidget *entry2 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox2), entry2, TRUE, TRUE, 0);

    GtkWidget *createFileButton = gtk_button_new_with_label("Create File");
    g_signal_connect(createFileButton, "clicked", G_CALLBACK(on_create_file_clicked), entry2);
    gtk_box_pack_start(GTK_BOX(hbox2), createFileButton, FALSE, FALSE, 0);

    GtkWidget *hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox3, FALSE, FALSE, 0);

    GtkWidget *entry3 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox3), entry3, TRUE, TRUE, 0);

    GtkWidget *entry4 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox3), entry4, TRUE, TRUE, 0);

    GtkWidget *writeFileButton = gtk_button_new_with_label("Write File");
    GtkWidget *entries[] = {entry3, entry4};
    g_signal_connect(writeFileButton, "clicked", G_CALLBACK(on_write_file_clicked), entries);
    gtk_box_pack_start(GTK_BOX(hbox3), writeFileButton, FALSE, FALSE, 0);

    GtkWidget *hbox4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox4, FALSE, FALSE, 0);

    GtkWidget *entry5 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox4), entry5, TRUE, TRUE, 0);

    GtkWidget *readFileButton = gtk_button_new_with_label("Read File");
    g_signal_connect(readFileButton, "clicked", G_CALLBACK(on_read_file_clicked), entry5);
    gtk_box_pack_start(GTK_BOX(hbox4), readFileButton, FALSE, FALSE, 0);

    GtkWidget *hbox5 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox5, FALSE, FALSE, 0);

    GtkWidget *entry6 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox5), entry6, TRUE, TRUE, 0);

    GtkWidget *deleteFileButton = gtk_button_new_with_label("Delete File");
    g_signal_connect(deleteFileButton, "clicked", G_CALLBACK(on_delete_file_clicked), entry6);
    gtk_box_pack_start(GTK_BOX(hbox5), deleteFileButton, FALSE, FALSE, 0);

    GtkWidget *listFilesButton = gtk_button_new_with_label("List Files");
    g_signal_connect(listFilesButton, "clicked", G_CALLBACK(on_list_files_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), listFilesButton, FALSE, FALSE, 0);

    GtkWidget *hbox6 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox6, FALSE, FALSE, 0);

    GtkWidget *entry7 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox6), entry7, TRUE, TRUE, 0);

    GtkWidget *deleteDirectoryButton = gtk_button_new_with_label("Delete Directory");
    g_signal_connect(deleteDirectoryButton, "clicked", G_CALLBACK(on_delete_directory_clicked), entry7);
    gtk_box_pack_start(GTK_BOX(hbox6), deleteDirectoryButton, FALSE, FALSE, 0);

    GtkWidget *hbox7 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox7, FALSE, FALSE, 0);

    GtkWidget *entry8 = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox7), entry8, TRUE, TRUE, 0);

    GtkWidget *navigateFolderButton = gtk_button_new_with_label("Navigate Folder");
    g_signal_connect(navigateFolderButton, "clicked", G_CALLBACK(on_navigate_folder_clicked), entry8);
    gtk_box_pack_start(GTK_BOX(hbox7), navigateFolderButton, FALSE, FALSE, 0);

    GtkWidget *exitFolderButton = gtk_button_new_with_label("Exit Folder");
    g_signal_connect(exitFolderButton, "clicked", G_CALLBACK(on_exit_folder_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox7), exitFolderButton, FALSE, FALSE, 0);

    GtkWidget *listSubFoldersButton = gtk_button_new_with_label("List Subfolders");
    g_signal_connect(listSubFoldersButton, "clicked", G_CALLBACK(on_list_subfolders_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), listSubFoldersButton, FALSE, FALSE, 0);

    outputLabel = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox), outputLabel, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
