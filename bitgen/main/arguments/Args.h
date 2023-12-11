#ifndef _ARGS_H_
#define _ARGS_H_

#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>

using namespace std;
namespace po = boost::program_options;

struct Args {
  static const char *_ARCH_PATH;
  static const char *_CIL_PATH;
  static const char *_NETLIST_PATH;
  static const char *_BITFILE_PATH;
  static const char *_LOGFILE_PATH;
  /*	static const char* _FRMFILE_PATH;*/
  static const char *_FORMAT;

  string _device;
  string _package;
  string _arch;
  string _cil;
  string _netlist;
  string _bitstream;
  int _partialbitstream;
  string _lib_dir;
  string _work_dir;
  string _log_dir;
  bool _logSwitch;
  bool _frmSwitch;
  bool _encrypt;
  bool _sopcSwitch;

  void tryParse(int argc, char *argv[]);
  void check();
  void dispHelp() const;
};

extern Args args;

inline ostream &operator<<(ostream &out, const Args &args) {
  boost::format fm(Args::_FORMAT);
  out << str(fm % "arch   " % args._arch);
  out << str(fm % "cil    " % args._cil);
  out << str(fm % "netlist" % args._netlist);
  out << str(fm % "bstream" % args._bitstream);
  return out;
}

#endif