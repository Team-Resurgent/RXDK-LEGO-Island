#include "config.h"

#include <SDL3/SDL_log.h>
#include <iniparser.h>

void XBOXOG_SetupDefaultConfigOverrides(dictionary* p_dictionary)
{
	SDL_Log("Overriding default config for the original Xbox");

	// D: is the folder the title was launched from; the game files (the CD's
	// LEGO folder) are copied next to the XBE.
	iniparser_set(p_dictionary, "isle:diskpath", "D:\\LEGO\\");
	iniparser_set(p_dictionary, "isle:cdpath", "D:\\");

	// T: is the title's own persistent storage on the hard disk.
	iniparser_set(p_dictionary, "isle:savepath", "T:\\");

	// There is no system cursor, so the game draws its own for the sticks and a USB mouse.
	iniparser_set(p_dictionary, "isle:Draw Cursor", "true");
}
