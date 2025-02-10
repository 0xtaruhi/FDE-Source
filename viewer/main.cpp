#include "ChipViewer.h"
#include "version.h"
#include <QtGui>
#include <QtCore>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <fstream>
#include <string>

using namespace std;
using namespace FDU::LOG;

int main(int argc, char *argv[])
{
	init_log_file("run.log");
	FDU_LOG(INFO) << fde_copyright("Chip Viewer");

	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("arch,a", po::value<string>(), "set arch file path")
		("design,d", po::value<string>(), "set design file path")
		;

	po::variables_map vm;        
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);    

	if (vm.count("help"))
	{
		cout << desc << endl;
		return 0;
	}

	QApplication a(argc, argv);
	FPGAViewer::ChipViewer w;
	w.show();

	if (vm.count("arch"))
	{
		w.LoadArch( vm["arch"].as<string>() );
		if (vm.count("design"))
		{
			w.LoadDesign( vm["design"].as<string>() );
		}
	}

	return a.exec();
}