// diff_match_patch.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


int _tmain(int argc, _TCHAR* argv[])
{
	google_dmp d;
	std::vector<Diff*> * a = d.diff_main(std::string("aaaaaavbasbabsabsabs \r\n 12345678adad9999ad1234"), std::string("aaaaaasbasbabsabsabs \r\n 12345678{{T}}9999{{T2}}1234"));
	return 0;
}

