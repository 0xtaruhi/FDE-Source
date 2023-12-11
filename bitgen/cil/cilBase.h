#ifndef _CILBASE_H_
#define _CILBASE_H_

#include <string>
#include <boost/noncopyable.hpp>

namespace FDU { namespace cil_lib {

	class CilBase : boost::noncopyable {
	protected:
		std::string		_name;
	public:
		virtual ~CilBase() = 0;
		CilBase(const std::string& name) : _name(name) {}
		std::string getName() const { return _name; }
		std::string name() const { return _name; }
	};

}}

#endif