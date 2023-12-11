#ifndef PLCCARRYINFER_H
#define PLCCARRYINFER_H

#include "plc_utils.h"

#include <map>

namespace COS {  
	class TDesign;
}

namespace FDU { namespace Place {

	using namespace COS;

	class PLCInstance; class PLCModule;
	struct ChainNode;  struct ChainEdge;

	/************************************************************************/
	/* carry chain中的节点之间的边，可以考虑去掉                            */
	/************************************************************************/
	struct ChainEdge
	{
		ChainNode*		_from_node;
		ChainNode*		_to_node;

		ChainEdge(ChainNode* f = NULL, ChainNode* t = NULL) 
			: _from_node(f), _to_node(t) {}
	};

	/************************************************************************/
	/* carry chain中的节点定义                                              */
	/************************************************************************/
	struct ChainNode
	{
		PLCInstance*	_owner;		//指向该slice的指针
		ChainEdge*		_from_edge; //该slice的from边
		ChainEdge*		_to_edge;	//该slice的to边
		
		explicit ChainNode(PLCInstance* owner)
			: _owner(owner), _from_edge(NULL), _to_edge(NULL) {}
		~ChainNode() { delete _to_edge; }
		//创建该节点的to edge，并将两个节点连接好
		void create_to_edge(ChainNode* t)
		{ t->_from_edge = _to_edge = new ChainEdge(this, t); }
	};

	/************************************************************************/
	/* 用来处理carry chain                                                  */
	/************************************************************************/
	class CarryChainInference
	{
	public:
		~CarryChainInference()
		{ for (ChainNodes::value_type& node: _nodes) delete node.second; }
		//map<name, insts>
		typedef std::map<std::string, std::vector<PLCInstance*> > CarryChains;
		//找到design网表中的各个carry chain,并且存储在chains
		void inference(TDesign* design, CarryChains& chains);

	protected:
		//创建carry chain中的一个节点
		ChainNode*	create_node	(PLCInstance* owner);
		//找到design网表中的carry chain的各个节点，但是没有设置成carry chain
		void		find_chains	(PLCModule* top_cell);
		//将分立节点组织成各个carry chain然后存储，并设置好属性
		void		store_chains(CarryChains& chains);

	private:
		typedef std::map<PLCInstance*, ChainNode*>	ChainNodes;
		ChainNodes									_nodes;
	};

}}

#endif