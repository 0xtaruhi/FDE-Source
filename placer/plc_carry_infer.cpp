#include "plc_carry_infer.h"
#include "plc_factory.h"
#include "plc_utils.h"

namespace FDU { namespace Place {

	using namespace std;
	using namespace boost;

#undef EXPORT_NETLIST 

	/************************************************************************/
	/*	功能：找到网表中的carry chain,并存储
	 *	参数：design:设计网表， chains: 找到的carry chain
	 *返回值：void
	 *	说明：chains为一个map,其key为string,carry chain的名字，content为vector，为
	 *			其内部instance结构的指针集合
	 */
	/************************************************************************/
	void CarryChainInference::inference(TDesign* design, CarryChains& chains)
	{
		find_chains(static_cast<PLCModule*>(design->top_module()));
		store_chains(chains);
#ifdef EXPORT_NETLIST
		design->save(fileio::xml, design->name() + "_carrychain.xml");
#endif
	}
	/************************************************************************/
	/*	功能：创建一个carry chain中的节点，并存储在_nodes里面
	 *	参数：owner: 一个carry chain 中的节点
	 *返回值：ChainNode* ：创建节点的指针
	 *	说明：
	 *			
	 */
	/************************************************************************/
	ChainNode* CarryChainInference::create_node(PLCInstance* owner)
	{
		ChainNode* node = NULL;
		//如果存在在_nodes里面，那么返回该节点
		//如果不存在，创建一个并插入到_nodes后
		if(_nodes.count(owner))
			node = _nodes[owner];
		else {
			node = new ChainNode(owner);
			_nodes.insert(make_pair(owner, node));
		}
		return node;	
	}
	/************************************************************************/
	/*	功能：找到网表中各个carry chain的节点
	 *	参数：top_cell: design的顶层单元
	 *返回值：void
	 *	说明：网表中每个net的pin name属性为cin,cout的为carry chain,
	 *			这个函数只是找到所有具有这些属性的点，并将这些点及其边创建好
	/************************************************************************/
	void CarryChainInference::find_chains(PLCModule* top_cell)
	{
		//找到线网中pin的名字是CIN，COUT的线网
        PLCInstance * cin_instance;
        PLCInstance * cout_instance;
		for (PLCNet* net: top_cell->nets()) {
			PLCNet::pin_iter cin_pin  = find_if(net->pins(), [](const Pin* pin) { return pin->name() == DEVICE::CIN;  });
			PLCNet::pin_iter cout_pin = find_if(net->pins(), [](const Pin* pin) { return pin->name() == DEVICE::COUT; });
			//找到了一个net
            if(cin_pin != net->pins().end() && cout_pin != net->pins().end()){

                cin_instance = static_cast<PLCInstance*>(cin_pin->owner());
                cout_instance = static_cast<PLCInstance*>(cout_pin->owner());

				//设置为ignord，在计算cost中不用考虑
				net->set_ignored();
                if (cout_instance->is_fixed() && cin_instance->is_fixed())
                {
                    //printf("fixed carry chain");
                }
                else if (!cout_instance->is_fixed() && !cin_instance->is_fixed())
                {
                    ChainNode* cout_node = create_node(cout_instance);
                    ChainNode* cin_node	 = create_node(cin_instance);
                    cout_node->create_to_edge(cin_node);
                }
                else
                    ASSERT(0,
                    (CONSOLE::PLC_ERROR % ("illegal constraint for carry chain.")));
				//为net连接的两个slice创建两个节点，并创建一条边
				
			}
		}
	}
	/************************************************************************/
	/*	功能：将find_chains找到的carry chain的各个节点串成相应的carry chain，并修改instance的属性
	 *	参数：chains，carry chain的集合
	 *返回值：void
	 *	说明：
	/************************************************************************/
	void CarryChainInference::store_chains(CarryChains& chains)
	{
		for (ChainNodes::value_type& node: _nodes) {
			ChainNode* chain_node = node.second;
			if(!chain_node->_from_edge){ // 如果一个点没有from edge，那么就为carry chain的开始
				PLCInstance* owner  = chain_node->_owner;
				//hset为网表需要的一个名字，网表要求carry chain的名字为carry chain开始的instance的名字
				string		 hset   = owner->name();
//				chains.insert(make_pair(hset, vector<PLCInstance*>()));
				//得到存放这条carry chain的vector
				vector<PLCInstance*>& chain = chains[hset];
				//放入carry chain vector
				chain.push_back(owner);
				//依次压入后续instance
				while(chain_node->_to_edge){
					chain.push_back(chain_node->_to_edge->_to_node->_owner);
					chain_node = chain_node->_to_edge->_to_node;
				}
				//下面的属性设置是根据网表需求
				int rloc = chain.size() - 1;
				Property<string>& hsets = create_property<string>(COS::INSTANCE, INSTANCE::HSET);
				Property<Point>& RLOCs = create_property<Point>(COS::INSTANCE, INSTANCE::RLOC);
				Property<string>& SET_TYPE = create_property<string>(COS::INSTANCE, INSTANCE::SET_TYPE);
				for (PLCInstance* inst: chain) {
					inst->set_property(hsets, hset);
					inst->set_property(RLOCs, Point(rloc--, 0, 0));
					inst->set_property(SET_TYPE, DEVICE::CARRY);
				}
			}
		}
	}

}}