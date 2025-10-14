/* Console synchrone */

#include "console.h"
#include "synch.h"

class SyncConsole
{
	Lock *readlock;
	Lock *writelock;
	Console *console;

	public:
	
		Semaphore *readsem;
		Semaphore *writesem;
	
		SyncConsole(char * readname, char * writename);
		~SyncConsole();
		char lirechar();
		void ecrirechar(char car);
		void ReadDone();
		void WriteDone();
};
