#include "ProjConvertor.h"

int main()
{
	auto projList = ReadConfig();
	
	for ( auto& curProj : projList )
	{
		if ( !BuildProject(curProj) )
		{
			return 0;
		}
	}	
}