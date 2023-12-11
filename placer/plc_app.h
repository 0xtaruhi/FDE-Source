#ifndef PLCAPP_H
#define PLCAPP_H

#include "plc_algorithm.h"
#include "plc_args.h"
#include "log.h"

namespace FDU { namespace Place {

	using COS::TDesign;

	class PlaceApp
	{
	public:
		PlaceApp() : _placer(&_design) { LOG::init_log_file("place.log", "place"); }//初始化log文件

		void parse_command(int argc, char* argv[]);
		void try_process();

	protected:
		void load_files();
		void export_jtag_csts();

	private:
		PlaceArgs		_args; //布局的参数

		TDesign			_design; //用户网表
		SAPlacer		_placer; //布局器，模拟退火过程算法在这里完成，计算cost的在FloorPlan里面完成
	};

}}


#endif