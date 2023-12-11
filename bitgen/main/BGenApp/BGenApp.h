#ifndef _BGENAPP_H_
#define _BGENAPP_H_

#include "main/arguments/Args.h"
#include "cil/cilLib.h"
#include "circuit/circuit.h"
#include "bitstream/bstrGenerate/bstrGener.h"

namespace BitGen{
	using namespace FDU::cil_lib;
	using namespace BitGen::circuit;
	using namespace BitGen::bitstream;
	using namespace ARCH;
	
	class BGenApp{
	private:
		cilLibrary* _cil;
		Circuit*	_ckt;

	public:
		BGenApp()  {}
		~BGenApp() { delete _cil; delete _ckt; } 

		void tryBGen();
	};

}

#endif