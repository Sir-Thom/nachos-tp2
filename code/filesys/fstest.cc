// fstest.cc 
//	Simple test routines for the file system.  
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file 
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"

#define TransferSize 	10 	// make it small, just to be difficult

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void
Copy(char *from, char *to)
{
    FILE *fp;
    FileHandle openFile;
    int amountRead, fileLength;
    char *buffer;

// Open UNIX file
    if ((fp = fopen(from, "r")) == NULL) {	 
	printf("Copy: couldn't open input file %s\n", from);
	return;
    }

// Figure out length of UNIX file
    fseek(fp, 0, 2);		
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

// Create a Nachos file of the same length
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength)) {	 // Create Nachos file
	printf("Copy: couldn't create output file %s\n", to);
	fclose(fp);
	return;
    }
    
    openFile = fileSystem->Open(to);
	
 //IFT320: Assertion a retirer lorsqu'on change le type de FileHandle.
 //   ASSERT(openFile != NULL);
    
// Copy the data in TransferSize chunks
    buffer = new char[TransferSize];
    while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0)
	fileSystem->Write(openFile,buffer, amountRead);	
    delete [] buffer;

// Close the UNIX and the Nachos files
    fileSystem->Close(openFile);		// close the Nachos file
    fclose(fp);
}

//----------------------------------------------------------------------
// Print
// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void
Print(char *name)
{
    FileHandle openFile;    
    int i, amountRead;
    char *buffer;

	//IFT320: a modifier lorsque FileHandle change de type
    openFile = fileSystem->Open(name);
    int handle = openFile;
    if (handle < 0) {
	printf("Print: unable to open file %s\n", name);
	return;
    }
    
    buffer = new char[TransferSize];
    while ((amountRead = fileSystem->Read(openFile,buffer, TransferSize)) > 0)
	for (i = 0; i < amountRead; i++)
	    printf("%c", buffer[i]);
    delete [] buffer;

    fileSystem->Close(openFile);		// close the Nachos file
    return;
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName 	"TestFile"
#define Contents 	"1234567890"
#define ContentSize 	strlen(Contents)
#define FileSize 	((int)(ContentSize * 5000))

static void 
FileWrite()
{
    FileHandle openFile;    
    int i, numBytes;

    printf("Sequential write of %d byte file, in %d byte chunks\n", 
	FileSize, ContentSize);
    if (!fileSystem->Create(FileName, 0)) {
      printf("Perf test: can't create %s\n", FileName);
      return;
    }
    openFile = fileSystem->Open(FileName);
    if (openFile == NULL) {
	printf("Perf test: unable to open %s\n", FileName);
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = fileSystem->Write(openFile,Contents, ContentSize);
	if (numBytes < 10) {
	    printf("Perf test: unable to write %s\n", FileName);
	    fileSystem->Close(openFile);		// close the Nachos file
	    return;
	}
    }
    fileSystem->Close(openFile);	// close the Nachos file	// close file
}

static void 
FileRead()
{
    FileHandle openFile;    
    char *buffer = new char[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %d byte chunks\n", 
	FileSize, ContentSize);

    if ((openFile = fileSystem->Open(FileName)) == NULL) {
	printf("Perf test: unable to open file %s\n", FileName);
	delete [] buffer;
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = fileSystem->Read(openFile,buffer, ContentSize);
	if ((numBytes < 10) || strncmp(buffer, Contents, ContentSize)) {
	    printf("Perf test: unable to read %s\n", FileName);
	    fileSystem->Close(openFile);		// close the Nachos file
	    delete [] buffer;
	    return;
	}
    }
    delete [] buffer;
    fileSystem->Close(openFile);		// close the Nachos file
}

void
PerformanceTest()
{
    printf("Starting file system performance test:\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FileName)) {
      printf("Perf test: unable to remove %s\n", FileName);
      return;
    }
    stats->Print();
}

void
DirectoryTest()
{
	printf("\n\n\n-------------IFT320 DirectoryTest -----------------\n");
	printf("Test pour les sous-repertoires\n\n\n");
	
	//Creation de repertoire
	fileSystem->CreateDirectory("Simpsons");
	fileSystem->List();
	
	//Changmeent de repertoire
	fileSystem->ChangeDirectory("Simpsons");
	fileSystem->List();
	
	//Creation de fichiers dans sous-repertoire
	Copy("big","Homer");
	fileSystem->List();
	Copy("medium","Marge");
	fileSystem->List();
	Copy("small","Maggie");
	fileSystem->List();
	
	//Creation de repertoire dans sous-repertoire
	fileSystem->CreateDirectory("Bart");	
	fileSystem->List();	
	fileSystem->ChangeDirectory("Bart");
	fileSystem->List();	
	
	//Creation de fichier dans sous-sous-repertoire
	Copy("small","Milhouse");
	fileSystem->List();
	
	//Test du lien vers le parent 
	fileSystem->ChangeDirectory("..");
	
	//Creation de fichier apres recul et creation de repertoire
	Copy("small","Lisa");
	fileSystem->List();	
	fileSystem->ChangeDirectory("..");
	fileSystem->List();	
	
	//Creation d'un second sous-repertoire
	fileSystem->CreateDirectory("Flanders");
	fileSystem->List();	
	fileSystem->ChangeDirectory("Flanders");
	fileSystem->List();
	
	//Creation de fichiers dans le second sous-repertoire
	Copy("medium","Ned");
	fileSystem->List();
	Copy("medium","Maud");
	fileSystem->List();
	Copy("small","Rod");
	fileSystem->List();
	Copy("small","Todd");
	fileSystem->List();
	
	//Effacement de fichier dans un sous-repertoire
	fileSystem->Remove("Maud");
	fileSystem->List();
	
	//Effacement d'un repertoire non-vide
	fileSystem->ChangeDirectory("..");
	fileSystem->ChangeDirectory("Simpsons");
	bool result = fileSystem->Remove("Bart");
	if(result == TRUE){
		printf("ERROR: Directory is not empty\n");
	}
	fileSystem->List();
	
	//Effacement d'un repertoire vide	
	fileSystem->ChangeDirectory("Bart");
	fileSystem->Remove("Milhouse");
	fileSystem->ChangeDirectory("..");
	fileSystem->Remove("Bart");
	fileSystem->List();
	
	//Ajout de fichiers pour les tests TabSysTest et TabPcsTest
	fileSystem->ChangeDirectory("..");
	fileSystem->CreateDirectory("Futurama");
	fileSystem->ChangeDirectory("Futurama");
	Copy("small","Fry");
	Copy("small","Leela");
	Copy("small","Hubert");
	Copy("small","Bender");
	Copy("small","Amy");
	Copy("small","Hermes");
	Copy("small","Zoidberg");
	fileSystem->List();
	fileSystem->ChangeDirectory("..");
	Copy("small","Groening");
	Copy("small","Cohen");
	fileSystem->List();
	fileSystem->CreateDirectory("Mirror");
	fileSystem->ChangeDirectory("Mirror");
	fileSystem->List();
	Copy("small","Fry");
	Copy("small","Leela");
	Copy("small","Hubert");
	Copy("small","Bender");
	Copy("small","Amy");
	Copy("small","Hermes");
	Copy("small","Zoidberg");	
	fileSystem->List();
	
	printf("\n\n\n-------------Fin DirectoryTest --------------------\n\n\n\n");
}

void
HierarTest()
{
	
	printf("\n\n\n-----------------IFT320 HierarTest --------------------\n");
	printf("Test exhaustif pour le systeme de fichiers hierarchique\n\n\n");
	
	
	
	
	
	
	printf("\n\n\n--------------------Fin HieraTest ---------------------\n\n\n\n");
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Fonction test pour la table des fichiers ouverts
//  On re-utilise les fichiers et repertoire crees dans le
//  test de la hierarchie
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void
TabSysTest()
{

	printf("\n\n\n-----------------IFT320 TabSysTest --------------------\n");
	printf("Test pour la table de fichiers ouverts du systeme\n\n\n");
	
	// Premier test pour la table des fichiers ouverts
       
	//  Dans le repertoire de Futurama
	fileSystem->ChangeDirectory("Futurama");
	
	fileSystem->Open("Fry");	
	fileSystem->Open("Leela");	
	fileSystem->Open("Bender");
		
	//-------------------------------------------------------------------
	// On va dans le repertoire Simpsons
    fileSystem->ChangeDirectory("..");
	fileSystem->ChangeDirectory("Simpsons");
	
	FileHandle f4 = fileSystem->Open("Maggie");
	//-------------------------------------------------------------------
	// On va dans le repertoire parent de Simpsons
    fileSystem->ChangeDirectory("..");	
	fileSystem->Open("Cohen");
	//-------------------------------------------------------------------
	// On va dans le repertoire Mirror
	fileSystem->ChangeDirectory("Mirror");
	
	fileSystem->Open("Fry");
	
	fileSystem->Open("Leela");

	fileSystem->Open("Zoidberg");
	
	fileSystem->Open("Bender");
	//-------------------------------------------------------------------
	// On va dans le repertoire Simpsons
	fileSystem->ChangeDirectory("..");
	fileSystem->ChangeDirectory("Simpsons");	
	fileSystem->Open("Lisa");	
	fileSystem->Open("Homer");
	fileSystem->Open("Marge");	
	
	// 	 Fermeture du quatrieme fichier ouvert 
	fileSystem->Close(f4);
	
	// 	 Quelques ouvertures de plus...
	fileSystem->ChangeDirectory("..");
	fileSystem->ChangeDirectory("..");
	fileSystem->Open("Groening");
	fileSystem->ChangeDirectory("Futurama");
	fileSystem->Open("Hermes");
	fileSystem->Open("Amy");
	fileSystem->ChangeDirectory("..");	
	
	//  On modifie les fichiers ouverts.
	fileSystem->TouchOpenedFiles("NACHOS!!!");
	
	//  On ferme tous les fichiers
	fileSystem->CloseAll();	
	
	
	
	printf("\n\n\n------------------Fin TabSysTest ----------------------\n\n\n\n");
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Fichiers test pour la table des fichiers ouverts des processus
//
//  - Trois fonctions pour les trois threads concurrents
//  - La fonction qui demarre les threads
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void
TabPcsTest1(int No)
{	
	// Thread 1 du test #2 pour table fichiers ouverts
   
	//  Dans le repertoire de base 	
	
	fileSystem->Open("Groening");
	fileSystem->Open("Cohen");
	
	//-------------------------------------------------------------------
	// On va dans le repertoire Simpsons
    fileSystem->ChangeDirectory("Simpsons");	
	fileSystem->Open("Homer");
	fileSystem->Open("Marge");
	fileSystem->Open("Lisa");	

	//  On modifie les fichiers ouverts.
	fileSystem->TouchOpenedFiles("DORITOS!!!");
	
	//  On ferme tous les fichiers
	fileSystem->CloseAll();	
}
	
void
TabPcsTest2(int No)
{	
	// Thread 2 du test #2 pour table fichiers ouverts
   
	//  Dans le repertoire de base 	
	
	
	fileSystem->Open("Groening");	
	fileSystem->Open("Cohen");
	
	//-------------------------------------------------------------------
	// On va dans le repertoire Futurama
    fileSystem->ChangeDirectory("Futurama");	
	fileSystem->Open("Fry");
	fileSystem->Open("Leela");
	fileSystem->Open("Bender");	
			
	
	//  On modifie les fichiers ouverts.
	fileSystem->TouchOpenedFiles("CHEETOS!!!");
	
	//  On ferme tous les fichiers
	fileSystem->CloseAll();	
}


void
TabPcsTest3(int No)
{	
	// Thread 3 du test #2 pour table fichiers ouverts
   
	//  Dans le repertoire de base 	
	
	fileSystem->Open("Groening");
	fileSystem->Open("Cohen");
	
	//-------------------------------------------------------------------
	// On va dans le repertoire Mirror
    fileSystem->ChangeDirectory("Mirror");	
	fileSystem->Open("Fry");
	fileSystem->Open("Leela");
	fileSystem->Open("Bender");	
			
	
	//  On modifie les fichiers ouverts.
	fileSystem->TouchOpenedFiles("TOSTITOS!!!");
	
	//  On ferme tous les fichiers
	fileSystem->CloseAll();	
}

void
TabPcsTest()
{
	printf("\n\n\n-----------------IFT320 TabPcsTest --------------------\n");
	printf("Test de table des fichiers ouverts par processus\n\n\n");
	
    // Programme de demarrage du test multi-thread pour 
    // la table des fichiers ouverts
 
    Thread *t0 = new Thread("second thread");
    t0->Fork(TabPcsTest2, 2);
    Thread *t1 = new Thread("troisieme thread");
    t1->Fork(TabPcsTest3, 3);  
    TabPcsTest1(1);
	
	printf("\n\n\n-------------------Fin TabPcsTest ---------------------\n\n\n\n");
}




//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//   Fonction pour imprimer les fichiers.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void
TabPcsPrint()
{
	printf("\n\n\n-----------------IFT320 TabPcsPrint --------------------\n");
	printf("Resultats des tests de table des fichiers ouverts\n\n\n");
	
	//     Impression des fichiers pour verifier les modifications
	printf("\n--------------Groening-----------------\n\n");
	Print("Groening");
	printf("\n--------------Cohen-----------------\n\n");
	Print("Cohen");
	
	fileSystem->ChangeDirectory("Simpsons");
	printf("\n--------------Homer-----------------\n\n");
	Print("Homer");
	printf("\n--------------Marge-----------------\n\n");
	Print("Marge");
	printf("\n--------------Lisa-----------------\n\n");
	Print("Lisa");
	printf("\n--------------Maggie-----------------\n\n");
	Print("Maggie");
	fileSystem->ChangeDirectory("..");
	fileSystem->ChangeDirectory("Futurama");	
	printf("\n--------------Fry-----------------\n\n");
	Print("Fry");
	printf("\n--------------Leela-----------------\n\n");
	Print("Leela");
	printf("\n--------------Hubert-----------------\n\n");
	Print("Hubert");
	printf("\n--------------Bender-----------------\n\n");
	Print("Bender");
	printf("\n--------------Amy-----------------\n\n");
	Print("Amy");
	printf("\n--------------Hermes-----------------\n\n");
	Print("Hermes");
	printf("\n--------------Zoidberg-----------------\n\n");
	Print("Zoidberg");
	
	fileSystem->ChangeDirectory("..");
	fileSystem->ChangeDirectory("Mirror");	
	printf("\n--------------Fry2-----------------\n\n");
	Print("Fry");
	printf("\n--------------Leela2-----------------\n\n");
	Print("Leela");
	printf("\n--------------Hubert2-----------------\n\n");
	Print("Hubert");
	printf("\n--------------Bender2-----------------\n\n");
	Print("Bender");
	printf("\n--------------Amy2-----------------\n\n");
	Print("Amy");
	printf("\n--------------Hermes2-----------------\n\n");
	Print("Hermes");
	printf("\n--------------Zoidberg2-----------------\n\n");
	Print("Zoidberg");
	
	printf("\n\n\n-------------------Fin TabPcsPrint ---------------------\n\n\n\n");	
}

