// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include "system.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)


//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------
FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    
    // IFT320: Initialiser la table des fichiers ouverts
    InitializeOpenFilesTable();

    // First, allocate space for FileHeaders for the directory and bitmap
    // (make sure no one else grabs these!)
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        freeMap->Mark(FreeMapSector);	    
        freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents

        // of the directory and bitmap files.  There better be enough space!
        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

        // Flush the bitmap and directory FileHeaders back to disk

        // We need to do this before we can "Open" the file, since open

        // reads the file header off of disk (and currently the disk has garbage

        // on it!).
        DEBUG('f', "Writing headers back to disk.\n");
        mapHdr->WriteBack(FreeMapSector);    
        dirHdr->WriteBack(DirectorySector);
        
        // OK to open the bitmap and directory files now

        // The file system operations assume these two files are left open

        // while Nachos is running.
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        directory->WriteBack(directoryFile);
     
        // IFT320: Initialiser les entrées spéciales du répertoire racine
        directory->Add(".", DirectorySector, DIR_TYPE);
        directory->Add("..", DirectorySector, DIR_TYPE);
        // flush changes to disk
        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);
        directory->WriteBack(directoryFile);

        if (DebugIsEnabled('f')) {
            freeMap->Print();
            directory->Print();
            delete freeMap; 
            delete directory; 
            delete mapHdr; 
            delete dirHdr;
        }
    } else {
        // if we are not formatting the disk, just open the files representing

        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
        currentThread->SetCurrentDirectory(DirectorySector);
        currentDirectorySector = currentThread->GetCurrentDirectory();

    printf("FileSystem initialized. Current directory: sector %d\n", currentDirectorySector);
}


void FileSystem::InitializeOpenFilesTable()
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        openFilesTable[i].inUse = FALSE;
        openFilesTable[i].openFile = NULL;
        openFilesTable[i].sector = -1;
        openFilesTable[i].filename[0] = '\0';
        openFilesTable[i].currentPosition = 0;
    }
}

FileHandle FileSystem::FindFreeSlot()
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!openFilesTable[i].inUse) {
            return i;  // Retourner l'index du slot libre
        }
    }
    return INVALID_FILE_HANDLE;  // Table pleine
}


bool FileSystem::IsValidHandle(FileHandle handle)
{
    return (handle >= 0 && handle < MAX_OPEN_FILES && 
            openFilesTable[handle].inUse && 
            openFilesTable[handle].openFile != NULL);
}


//IFT320
void FileSystem::SetCurrentDirectory(int sector)
{
      currentThread->SetCurrentDirectory(sector);
}

int FileSystem::GetCurrentDirectory()
{
    return currentThread->GetCurrentDirectory();
    //return currentDirectorySector;
}

int FileSystem::Read(FileHandle file, char *into, int numBytes) {	
	if (!IsValidHandle(file)) {
        printf("Error: Invalid file handle %d for read\n", file);
        return 0;
    }

    if (into == NULL) {
        printf("Error: Null buffer for read\n");
        return 0;
    }

    int bytesRead = openFilesTable[file].openFile->Read(into, numBytes);
    openFilesTable[file].currentPosition += bytesRead;
    
    printf("Read %d bytes from file '%s' (handle %d)\n", 
           bytesRead, openFilesTable[file].filename, file);
    
    return bytesRead;
}

int FileSystem::Write(FileHandle file, char *from, int numBytes) {		
	if(!IsValidHandle(file)){
        printf("Error: Invalid file handle  %d for write\n",file);
        return 0;
    }
    if (from == NULL){
        printf("Error Null buffeer for write\n");
        return 0;
    }
    int bytesWritten = openFilesTable[file].openFile->Write(from,numBytes);
    openFilesTable[file].currentPosition += bytesWritten;
    printf("Wrote %d bytes to file '%s' (handle %d)\n",bytesWritten,openFilesTable[file].filename,file);
    return bytesWritten;
}

int FileSystem::ReadAt(FileHandle file, char *into, int numBytes,int position) {
	if (!IsValidHandle(file)) {
        printf("Error: Invalid file handle %d for ReadAt\n", file);
        return 0;
    }

    if (into == NULL) {
        printf("Error: Null buffer for ReadAt\n");
        return 0;
    }

    int bytesRead = openFilesTable[file].openFile->ReadAt(into, numBytes,position);
    openFilesTable[file].currentPosition += bytesRead;
    
    printf("Read %d bytes from file '%s' at position %d (handle %d)\n", 
           bytesRead, openFilesTable[file].filename, position,file);
    
    return bytesRead;

    //return file->ReadAt(into,numBytes,position);
}

int FileSystem::WriteAt(FileHandle file, char *from, int numBytes,int position) {
	  if (!IsValidHandle(file)) {
        printf("Error: Invalid file handle %d for WriteAt\n", file);
        return 0;
    }

    if (from == NULL) {
        printf("Error: Null buffer for WriteAt\n");
        return 0;
    }

    int bytesWritten = openFilesTable[file].openFile->WriteAt(from, numBytes, position);
    
    printf("Wrote %d bytes to file '%s' at position %d (handle %d)\n", 
           bytesWritten, openFilesTable[file].filename, position, file);
    
    return bytesWritten;
    //return file->WriteAt(from,numBytes,position);
}


void FileSystem::Close (FileHandle file){
	 if (!IsValidHandle(file)) {
        printf("Error: Invalid file handle %d\n", file);
        return;
    }

    printf("Closing file '%s' (handle %d)\n", 
           openFilesTable[file].filename, file);
    
    // Fermer le fichier
    delete openFilesTable[file].openFile;
    
    // Libérer l'entrée de la table
    openFilesTable[file].inUse = FALSE;
    openFilesTable[file].openFile = NULL;
    openFilesTable[file].sector = -1;
    openFilesTable[file].filename[0] = '\0';
    openFilesTable[file].currentPosition = 0;
}
void FileSystem::CloseAll(){
     printf("Closing all open files...\n");
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFilesTable[i].inUse) {
            Close(i);
        }
    }
	//IFT320: Partie B
	//printf("!!CloseAll non implemente!!\n");
	//ASSERT(FALSE);
}

void FileSystem::TouchOpenedFiles(char * modif){
	//IFT320: Partie B
	printf("TouchOpenedFiles: %s\n",modif ? modif : "NULL");
    for (size_t i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (openFilesTable[i].inUse)
        {
            printf(" -File '%s' (handle %d) would be modified\n",openFilesTable[i].filename,i);

          
        }
        
    }
    
    //printf("!!TouchOpenedFiles non implemente!!\n");
	//ASSERT(FALSE);
}


//IFT320: Fonction de changement de repertoire. Doit etre implementee pour la partie A.
bool FileSystem::ChangeDirectory(char* name)
{
    // Vérifier les paramètres d'entrée
    if (name == NULL || strlen(name) == 0) {
        printf("Error: Invalid directory name\n");
        return FALSE;
    }

    int currentSector = GetCurrentDirectory();
    
    // Vérifier que le secteur est valide
    if (currentSector < 0 || currentSector >= NumSectors) {
        printf("Error: Invalid current directory sector %d\n", currentSector);
        return FALSE;
    }

    OpenFile *currentDirFile = new OpenFile(currentSector);
    if (currentDirFile == NULL) {
        printf("Error: Could not open current directory\n");
        return FALSE;
    }
    
    Directory *currentDirectory = new Directory(NumDirEntries);
    if (currentDirectory == NULL) {
        printf("Error: Could not create directory object\n");
        delete currentDirFile;
        return FALSE;
    }

    currentDirectory->FetchFrom(currentDirFile);

    int sector = currentDirectory->Find(name);
    if (sector == -1) {
        printf("Directory %s not found\n", name);
        delete currentDirectory;
        delete currentDirFile;
        return FALSE;
    }

    // Vérifier que c'est bien un répertoire
    if (currentDirectory->GetEntryType(name) != DIR_TYPE) {
        printf("Error: %s is not a directory\n", name);
        delete currentDirectory;
        delete currentDirFile;
        return FALSE;
    }

    // Vérifier que le nouveau secteur est valide
    if (sector < 0 || sector >= NumSectors) {
        printf("Error: Invalid target directory sector %d\n", sector);
        delete currentDirectory;
        delete currentDirFile;
        return FALSE;
    }

    SetCurrentDirectory(sector);
    printf("Changed to directory %s (sector %d)\n", name, sector);
    
    delete currentDirectory;
    delete currentDirFile;
    return TRUE;


	//IFT320: Partie A
//	printf("!!ChangeDirectory non implemente!!\n");
//	ASSERT(FALSE);			
}


//IFT320: Fonction de creation de repertoires. Doit etre implementee pour la partie A.
bool FileSystem::CreateDirectory(char *name)
{
    // Vérifier que le thread courant existe
    if (currentThread == NULL) {
        printf("Error: No current thread\n");
        return FALSE;
    }
    
    int parentSector = currentThread->GetCurrentDirectory();
    printf("DEBUG: CreateDirectory called for '%s' in parent directory sector %d\n", name, parentSector);
    
    // Vérifier que le secteur parent est valide
    if (parentSector < 0) {
        printf("Error: Invalid parent directory sector %d\n", parentSector);
        return FALSE;
    }
    
    OpenFile *parentDirectoryFile = new OpenFile(parentSector);
    if (parentDirectoryFile == NULL) {
        printf("Error: Could not open parent directory\n");
        return FALSE;
    }
    
    Directory *parentDirectory = new Directory(NumDirEntries);
    parentDirectory->FetchFrom(parentDirectoryFile);

    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG('f', "Creating directory %s\n", name);

    if (parentDirectory->Find(name) != -1) {
        success = FALSE; // Un fichier ou répertoire avec ce nom existe déjà
        printf("Error: Directory or file '%s' already exists\n", name);
    } else {
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find(); // Trouver un secteur libre

        if (sector == -1) {
            success = FALSE; // Pas de secteurs libres
            printf("Error: No free sectors available\n");
        } else if (!parentDirectory->Add(name, sector, DIR_TYPE)) {
            success = FALSE; // Plus de place dans le répertoire parent
            printf("Error: No space in parent directory\n");
        } else {
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, DirectoryFileSize)) {
                success = FALSE; // Pas assez d'espace pour les données
                printf("Error: Not enough space for directory data\n");
            } else {
                success = TRUE;
                
                // Écrire le header du nouveau répertoire
                hdr->WriteBack(sector);

                // Initialiser le nouveau répertoire avec "." et ".."
                Directory *newDirectory = new Directory(NumDirEntries);
                newDirectory->Add(".", sector, DIR_TYPE);
                newDirectory->Add("..", parentSector, DIR_TYPE);

                // Écrire le contenu du nouveau répertoire
                OpenFile *newDirFile = new OpenFile(sector);
                newDirectory->WriteBack(newDirFile);

                // Mettre à jour les structures sur disque
                parentDirectory->WriteBack(parentDirectoryFile);
                freeMap->WriteBack(freeMapFile);
                
                printf("Directory '%s' created successfully at sector %d\n", name, sector);
                
                delete newDirectory;
                delete newDirFile;
            }
            delete hdr;
        }
        delete freeMap;
    }
    delete parentDirectory;
    delete parentDirectoryFile;
    return success;


	//IFT320: Partie A
	//printf("!!CreateDirectory non implemente!!\n");
	//ASSERT(FALSE);	
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(char *name, int initialSize)
{
     // IFT320: Utiliser le répertoire courant au lieu du répertoire racine
    // Vérifications de base avant de proceder a la creation

    // Vérifier les paramètres d'entrée
    if (name == NULL || strlen(name) == 0) {
        printf("Error: Invalid file name\n");
        return FALSE;
    }
    
    if (initialSize < 0) {
        printf("Error: Invalid file size %d\n", initialSize);
        return FALSE;
    }

    int currentSector = GetCurrentDirectory();
    
    // Vérifier que le secteur est valide
    if (currentSector < 0 || currentSector >= NumSectors) {
        printf("Error: Invalid current directory sector %d\n", currentSector);
        return FALSE;
    }

    OpenFile *currentDirFile = new OpenFile(currentSector);
    if (currentDirFile == NULL) {
        printf("Error: Could not open current directory\n");
        return FALSE;
    }
    
    Directory *directory = new Directory(NumDirEntries);
    if (directory == NULL) {
        printf("Error: Could not create directory object\n");
        delete currentDirFile;
        return FALSE;
    }
    
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success = FALSE;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    directory->FetchFrom(currentDirFile);

    if (directory->Find(name) != -1) {
        printf("Error: File %s already exists\n", name);
        success = FALSE;			// file is already in directory
    } else {    
        freeMap = new BitMap(NumSectors);
        if (freeMap == NULL) {
            printf("Error: Could not create bitmap\n");
            success = FALSE;
        } else {
            freeMap->FetchFrom(freeMapFile);
            sector = freeMap->Find();	// find a sector to hold the file header

            if (sector == -1) {
                printf("Error: No free sectors available\n");
                success = FALSE;		// no free block for file header

            } else if (!directory->Add(name, sector, FILE_TYPE)) {
                printf("Error: No space in directory\n");
                success = FALSE; // no space in directory
            } else {
                hdr = new FileHeader;
                if (hdr == NULL) {
                    printf("Error: Could not create file header\n");
                    success = FALSE;  // could not create file header
                } else {
                    if (!hdr->Allocate(freeMap, initialSize)) {
                        printf("Error: Not enough space for file data\n");
                        success = FALSE; // no space on disk for data
                    } else {    
                        success = TRUE;
                        // everthing worked, flush all changes back to disk
                        hdr->WriteBack(sector);         
                        directory->WriteBack(currentDirFile);
                        freeMap->WriteBack(freeMapFile);
                    }
                    delete hdr;
                }
            }
            delete freeMap;
        }
    }
    
    delete directory;
    delete currentDirFile;
    return success;


}



//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------
static char* getNextPathComponent(char **path) {
    char *component = *path;
    char *separator = strchr(component, '/');

    if (separator != NULL) {
        *separator = '\0';
        *path = separator + 1;
    } else {
        *path = NULL;
    }
    return component;
}

FileHandle FileSystem::Open(char *name)
{ 
     if (name == NULL || strlen(name) == 0) {
        printf("Error: Invalid file name\n");
        return INVALID_FILE_HANDLE;
    }

    int currentSector = GetCurrentDirectory();
    OpenFile *currentDirFile = new OpenFile(currentSector);
    if (currentDirFile == NULL) {
        printf("Error: Could not open current directory sector %d\n", currentSector);
        return INVALID_FILE_HANDLE;
    }
    
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(currentDirFile);
    
    int sector = directory->Find(name); 
    
    if (sector == -1) {
        printf("File '%s' not found in directory\n", name);
        delete directory;
        delete currentDirFile;
        return INVALID_FILE_HANDLE;
    }

    // Vérifier si le fichier est déjà ouvert
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFilesTable[i].inUse && openFilesTable[i].sector == sector) {
            printf("File '%s' is already open (handle %d)\n", name, i);
            delete directory;
            delete currentDirFile;
            return i;  // Retourner le handle existant
        }
    }

    // Trouver un slot libre
    FileHandle handle = FindFreeSlot();
    if (handle == INVALID_FILE_HANDLE) {
        printf("Error: Open files table is full (max %d files)\n", MAX_OPEN_FILES);
        delete directory;
        delete currentDirFile;
        return INVALID_FILE_HANDLE;
    }

    // Ouvrir le fichier
    OpenFile *file = new OpenFile(sector);
    if (file == NULL) {
        printf("Error: Could not open file at sector %d\n", sector);
        delete directory;
        delete currentDirFile;
        return INVALID_FILE_HANDLE;
    }

    // Remplir l'entrée de la table
    openFilesTable[handle].inUse = TRUE;
    openFilesTable[handle].openFile = file;
    openFilesTable[handle].sector = sector;
    strncpy(openFilesTable[handle].filename, name, 31);
    openFilesTable[handle].filename[31] = '\0';
    openFilesTable[handle].currentPosition = 0;

    printf("File '%s' opened successfully (handle %d, sector %d)\n", name, handle, sector);
    
    delete directory;
    delete currentDirFile;
    
    return handle;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::Remove(char *name)
{ 
    // Vérifier les paramètres d'entrée
    if (name == NULL || strlen(name) == 0) {
        printf("Error: Invalid file name\n");
        return FALSE;
    }

    int currentSector = GetCurrentDirectory();
    
    // Vérifier que le secteur est valide
    if (currentSector < 0 || currentSector >= NumSectors) {
        printf("Error: Invalid current directory sector %d\n", currentSector);
        return FALSE;
    }

    OpenFile *currentDirFile = new OpenFile(currentSector);
    if (currentDirFile == NULL) {
        printf("Error: Could not open current directory\n");
        return FALSE;
    }
    //
    Directory *directory = new Directory(NumDirEntries);
    if (directory == NULL) {
        printf("Error: Could not create directory object\n");
        delete currentDirFile;
        return FALSE;
    }
    
    directory->FetchFrom(currentDirFile);
    
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    sector = directory->Find(name);
    if (sector == -1) {
        printf("File %s not found\n", name);
        delete directory;
        delete currentDirFile;
        return FALSE; // file not found
    }
        
    fileHdr = new FileHeader;
    if (fileHdr == NULL) {
        printf("Error: Could not create file header\n");
        delete directory;
        delete currentDirFile;
        return FALSE;
    }
    
    fileHdr->FetchFrom(sector);
    
    freeMap = new BitMap(NumSectors);
    if (freeMap == NULL) {
        printf("Error: Could not create bitmap\n");
        delete fileHdr;
        delete directory;
        delete currentDirFile;
        return FALSE;
    }
    
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap); // remove data blocks
    freeMap->Clear(sector); // remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile); // flush to disk
    directory->WriteBack(currentDirFile); // flush to disk
    
    delete fileHdr;
    delete directory;
    delete freeMap;
    delete currentDirFile;
    
    printf("File %s removed successfully\n", name);
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List()
{
    int currentSector = GetCurrentDirectory();
    
    // Vérifier que le secteur est valide
    if (currentSector < 0 || currentSector >= NumSectors) {
        printf("Error: Invalid current directory sector %d\n", currentSector);
        return;
    }

    OpenFile *currentDirFile = new OpenFile(currentSector);
    if (currentDirFile == NULL) {
        printf("Error: Could not open current directory\n");
        return;
    }
    
    Directory *directory = new Directory(NumDirEntries);
    if (directory == NULL) {
        printf("Error: Could not create directory object\n");
        delete currentDirFile;
        return;
    }
    
    directory->FetchFrom(currentDirFile);
    directory->List();
    
    delete directory;
    delete currentDirFile;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    // IFT320: Utiliser le répertoire courant
    int currentSector = GetCurrentDirectory();
    OpenFile *currentDirFile = new OpenFile(currentSector);
    
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(currentSector);
    dirHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();
	
    directory->FetchFrom(currentDirFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
    delete currentDirFile;
} 
