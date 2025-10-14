/* Console synchrone */
#include "sync_console.h"


static void ConsoleReadDone(int arg)
{
	SyncConsole * console = (SyncConsole *) arg;
	console->ReadDone();
}

static void ConsoleWriteDone(int arg)
{
	SyncConsole * console = (SyncConsole *) arg;
	console->WriteDone();
}



SyncConsole::SyncConsole(char * readname, char * writename)
{
	readsem = new Semaphore("sync console read",0);
	writesem = new Semaphore("sync console write", 0);
	readlock = new Lock("sync console lock read");
	writelock = new Lock("sync console lock write");
	console = new Console(NULL, NULL, ConsoleReadDone, ConsoleWriteDone,
					(int) this);
}

SyncConsole::~SyncConsole()
{ 
	delete readsem;
	delete writesem;
	delete readlock;
	delete writelock;
	delete console;
}


char SyncConsole::lirechar()
{
	char temp;
	readlock->Acquire();
	readsem->P();
	temp = console->GetChar();
	readlock->Release();
	return temp;
}

void SyncConsole::ecrirechar(char car)
{
	writelock->Acquire();
	console->PutChar(car);
	writesem->P();
	writelock->Release();
}


void SyncConsole::ReadDone()
{
	//printf("readdone\n");
	readsem->V();
}
void SyncConsole::WriteDone()
{
	//printf("write done\n");
	writesem->V();
}

