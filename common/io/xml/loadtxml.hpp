#ifndef LOADTXML_HPP
#define LOADTXML_HPP

#include "tnetlist.hpp"
#include "loadxml.hpp"

namespace COS { namespace XML {

	class TNetlistHandler : public NetlistHandler {
	protected:
		Port* load_port(xml_node* node, Module* mod);
	private:
		void load_timing(xml_node* node, TPort* port);
	};

}} // namespace

#endif
