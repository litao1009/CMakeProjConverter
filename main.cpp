#include "ProjConvertor.h"

int main()
{
	auto projInfoOpt = ReadConfig();
	if ( !projInfoOpt )
	{
		return 0;
	}

	auto& projInfo = *projInfoOpt;
	if ( !BuildProject(projInfo) )
	{
		return 0;
	}
}