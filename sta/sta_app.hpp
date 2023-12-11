#ifndef STA_APP_HPP
#define STA_APP_HPP

#include "sta_arg.hpp"
#include "iclib.hpp"
#include "sta_engine.hpp"
#include "rd_engine.hpp"

namespace FDU { namespace STA {

class STAApp
{

public:

	STAApp () : arg_(0), design_(0), engine_(0) {  FDU::LOG::init_log_file("log.txt"); }
	virtual ~STAApp ();

	void try_process(int argc, char* argv[]);

protected:

	void load_files();
	void save_files();

	STAArg*       arg_;
	COS::TDesign* design_;
	STAEngine*    engine_;

};

}} // namespace FDU::STA

#endif
