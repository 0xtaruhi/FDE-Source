#ifndef SAVERTXML_HPP
#define SAVERTXML_HPP

#include "savetxml.hpp"
#include "rtnetlist.hpp"

namespace COS { namespace XML {

	class RTNetlistWriter : public TNetlistWriter {
	protected:
		xml_node* write_net(xml_node* mod_node, const Net* net);

	private:
		xml_node* write_pip(xml_node* node, const PIP* pip);
	};

}} // namespace

#endif
