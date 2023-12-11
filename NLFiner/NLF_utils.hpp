#ifndef __SR_UTILS_H__
#define __SR_UTILS_H__

#include <iostream>
#include <boost/format.hpp>

namespace FDU { namespace NLF {

	//////////////////////////////////////////////////////////////////////////
	// preprocessor

	#define EXCEPTION_HANDLE

	//////////////////////////////////////////////////////////////////////////
	// cmd user information

	namespace UserInfo {
		extern boost::format copy_rights_fmt;
		extern boost::format progress_fmt;
		extern boost::format finish_fmt;
	}
}}

#endif