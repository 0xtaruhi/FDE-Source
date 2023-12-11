#ifndef _XMLCKTHANDLER_H_
#define _XMLCKTHANDLER_H_

#include "netlist.hpp"
#include "circuit/cktHandler.h"

namespace BitGen { namespace circuit {
	using namespace FDU;

	class xmlCktHandler : public cktHandler{
	private:
		COS::Design _design;

	public:
		explicit xmlCktHandler(Circuit* ckt) : cktHandler(ckt) {}

		virtual void parse(const std::string& file);
	};

}}

#endif