#include <assert.h>
#include <fstream>

using namespace std;

static ofstream* odbg;

void INITDEBUG(ofstream* ofs)
{
	odbg = ofs;
}

void MDEBUG(string msg)
{
#ifdef DEBUG	
	assert(odbg);
	odbg->write(msg.c_str(), msg.length());
	odbg->write("\n", 1);
	odbg->flush();
	// *odbg << msg << endl;
#endif	
}