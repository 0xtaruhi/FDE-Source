#ifndef MAPINSTANCE_H
#define MAPINSTANCE_H

#include <map>
#include <string>

namespace VL2XML_PARSER{
	
	using namespace std;

	class MapInstance
	{
	public:
		MapInstance();
		~MapInstance();
		
		//about instance name mapping
		string						get_real_name()					{ return RealName;		}
		string						get_mapped_name()				{ return MappedName;	}
		void						set_real_name(string name)		{ RealName = name;		}
		void						set_mapped_name(string name)	{ MappedName = name;	}
		
		//about port name mapping
		string						get_mapped_port_through_real_port(string name)		{ return PortsMap[name];}
		void						insert_port_pair(string real, string mapped)		{ PortsMap.insert(make_pair(real, mapped));}


	private:
		string						RealName;
		string						MappedName;

		map<string ,string>			PortsMap;
	};
}





#endif