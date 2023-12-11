#include "plc_floorplan.h"
#include "plc_const_infer.h"
#include "arch/archlib.hpp"
#include "plc_utils.h"
#include "plc_args.h"
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/range/adaptors.hpp>

//#include "report.h"
//#include "reportmanager.hpp"


namespace FDU { namespace Place {


	using boost::lexical_cast;
	using namespace std;
	using namespace boost;
	//using namespace boost::lambda;
//	using namespace boost::adaptors;
	using namespace ARCH;

	/************************************************************************/
	/*	功能：为布局模拟退火过程做好准备
	 *	参数：void
	 *	返回值：void
	 *	说明：
	 */
	/************************************************************************/
	bool Floorplan::ready_for_place(string output_file)
	{
		//设置器件规模
        bool isAllFixed;
		_dev_info.set_device_scale(FPGADesign::instance()->scale());
		//设置每个tile中的silice数目
		_dev_info.set_slice_num(FPGADesign::instance()->get_slice_num());
		_dev_info.set_carry_num(FPGADesign::instance()->get_carry_num());
		_dev_info.set_carry_chain(FPGADesign::instance()->get_carry_chain());
		_dev_info.set_lut_inputs(FPGADesign::instance()->get_LUT_inputs());
		vector<vector<int>> a= DEVICE::carry_chain;
		//统计网表信息
		_nl_info.classify_used_resource();
		//统计器件可用资源
		_dev_info.sum_up_rsc_in_device();
		//输出网表资源信息
		_nl_info.echo_netlist_resource();
		_nl_info.echo_resource_usage(_dev_info.rsc_in_device());
		//输出report
		_nl_info.writeReport(_dev_info.rsc_in_device(), output_file);
		//按照需求建立一个FPGA
		INFO(CONSOLE::PROGRESS % "50" % "build FPGA architecture");
		build_fpga();
		//初始化布局
		INFO(CONSOLE::PROGRESS % "60" % "begin to initially place");
		isAllFixed = init_place();
        return isAllFixed;
	}
	/************************************************************************/
	/*	功能：创建一个FPGA
	*	参数：void
	*	返回值：void
	*	说明：
	*/
	/************************************************************************/
	void Floorplan::build_fpga()
	{
		Property<DeviceInfo::RscStat>& site_infos = create_temp_property<DeviceInfo::RscStat>(COS::MODULE, CELL::SITE_INFO);
		int rows = _dev_info.device_scale().x;
		int cols = _dev_info.device_scale().y;
		//_fpga是一个Tile矩阵
		_fpga.renew(rows, cols);

		for(int row = 0; row < rows; ++row){
			for(int col = 0; col < cols; ++col){
				ArchInstance* tile_inst = FPGADesign::instance()->get_inst_by_pos(Point(row, col));
				ArchCell*	  tile_cell = tile_inst->down_module();

				Tile& tile = _fpga.at(row, col);
				tile.set_type(tile_cell->name());
				tile.set_phy_pos(tile_inst->phy_pos());

				DeviceInfo::RscStat& rsc_stat = *tile_cell->property_ptr(site_infos);
				for (DeviceInfo::RscStat::value_type& rsc: rsc_stat) {
					Site& site = tile.add_site(rsc.first);
					if(site._type == Site::IOB){
						for(int z = 0; z < rsc.second; ++z){
							if(FPGADesign::instance()->is_io_bound(Point(row, col, z)))
								++site._capacity;
						}
					}
					else if(site._type == Site::BLOCKRAM){
						if(tile_cell->name() == DEVICE::LBRAMD || 
						   tile_cell->name() == DEVICE::RBRAMD)
							++site._capacity;
						else
							site._capacity = 0;
					}
					else 
						site._capacity = rsc.second;
					site._occ_insts.resize(rsc.second, NULL);
					site._logic_pos = Point(row, col);
					if(site._capacity)
						_dev_info.add_rsc_ava_pos(site._type, Point(row, col));
				}
			}
		}

		for(int row = 0; row < rows; ++row){
			for(int col = 0; col < cols; ++col){
				for (Tile::Sites::value_type& type_site_map: _fpga.at(row, col).sites()){
					Site*			  site = type_site_map.second;
					vector<Point>& ava_pos = _dev_info.rsc_ava_pos(site->_type);
					site->_available_pos.assign(ava_pos.begin(), ava_pos.end());
				}
			}
		}
	}

	/************************************************************************/
	/*	功能：初始化布局
	*	参数：void
	*	返回值：void
	*	说明：
	*/
	/************************************************************************/
	bool Floorplan::init_place()
	{
#ifdef CONST_GENERATE
		ConstantsGener						const_gener;
		const_gener.generate_constants(_nl_info.design());
#endif
        bool isAllFixed;
        //检查和保存约束
        check_and_save_cst();
		//找到design网表中的carry chain
		CarryChainInference::CarryChains	carrys;
		CarryChainInference					carry_infer;
		carry_infer.inference(_nl_info.design(), carrys);
		//找到design网表中的Lut6
		LUT6Inference::LUT6s				lut6s;
		LUT6Inference						lut6_infer;
		lut6_infer.inference(_nl_info.design(), lut6s);
		
		//初始化布局carry chain
		init_place_carrychain(carrys);
		//初始化布局lut6
		init_place_lut6(lut6s);
		//初始化布局site
		isAllFixed = init_place_site();
		//检查每个net看是否设置为ignored
		set_ignored_nets();
		//存储每个net连接的instance,对于ignored net不存储
		for (PLCNet* net: static_cast<PLCModule*>(_nl_info.design()->top_module())->nets())
				 net->store_connected_insts();
		//初始化布局完成以后，更新信息，更新内容看update_qualified_pos_for_carrychain解释
		for(int col = 0; col < _dev_info.device_scale().y; ++col){
			for (NLInfo::MacroInfo::CarryChains::value_type& chain: _nl_info.carry_chains()){
				for (SwapObject* obj: chain.second)
					obj->init_qualified_pos_tile_col(col);
 			}
			for(int index = 0; index < DEVICE::NUM_CARRY_PER_TILE; ++index)
				update_qualified_pos_for_carrychain(col, index);
		}
        return isAllFixed;
	}

	/************************************************************************/
	/*	功能：初始化carry chain
	*	参数：carrys：CarryChains，网表中的carry chain集合
	*	返回值：void
	*	说明：
	*/
	/************************************************************************/	
	void Floorplan::init_place_carrychain(CarryChainInference::CarryChains& carrys)
	{
		//对网表中找到的每一个carry chain
		for (CarryChainInference::CarryChains::value_type& carry: carrys) {
			//给网表信息里面增加一个可交换对象
			SwapObject* obj = _nl_info.add_swap_object(FLOORPLAN::CARRY_CHAIN);
			//设置可交换对象的属性
			obj->fill(carry.second);
			//为网表增加一个carry chain的可交换对象
			_nl_info.add_carry_swap_obj(obj);

			vector<Point> chain_pos_vec;
			const int MAX_LOOP_NUM = 5;
			int		  loop_count   = 0;
			//找到一个初始化位置
			while(!find_init_pos_for_carrychain(carry.second.size(), chain_pos_vec)){ 
				ASSERT(++loop_count < MAX_LOOP_NUM, 
					   (CONSOLE::PLC_ERROR % (carry.first + ": can NOT place carry chain."))); 
			}

			ASSERT(chain_pos_vec.size() == carry.second.size(), 
				   (CONSOLE::PLC_ERROR % "illegal found positions for carry chain."));

			for(int i = 0; i < chain_pos_vec.size(); ++i){
				Point		 pos  = chain_pos_vec[i];
				PLCInstance* inst = carry.second[i];
				Site&		 site = _fpga.at(pos).site(Site::SLICE);
				
				++site._occ;
				site._occ_insts[pos.z] = inst;

				inst->set_swapable_type(FLOORPLAN::CARRY_CHAIN);
				inst->set_curr_logic_pos(pos);
				inst->set_curr_loc_site(&site);
			}
		}
	}

	void Floorplan::init_place_lut6(LUT6Inference::LUT6s& lut6s)
	{
		typedef pair<PLCInstance*, PLCInstance*>	LUT6;

		vector<Point>& ava_pos = _dev_info.rsc_ava_pos(Site::SLICE);

		for (LUT6& lut6: lut6s) {
			int		rand_idx;
			Site*	slice_site = NULL;
			do {
				rand_idx   = rand() % ava_pos.size();
				slice_site = &_fpga.at(ava_pos[rand_idx]).site(Site::SLICE);
			} while (slice_site->_occ != 0);

			PLCInstance* src_slice	= lut6.first;
			PLCInstance* sink_slice = lut6.second;
			
			slice_site->_occ_insts[0] = src_slice;
			slice_site->_occ_insts[1] = sink_slice;
			slice_site->_occ		  = DEVICE::NUM_SLICE_PER_TILE;

			Point pos = ava_pos[rand_idx];
			src_slice->set_curr_logic_pos(Point(pos.x, pos.y, 0));
			sink_slice->set_curr_logic_pos(Point(pos.x, pos.y, 1));

			src_slice->set_curr_loc_site(slice_site);
			sink_slice->set_curr_loc_site(slice_site);

			src_slice->set_swapable_type(FLOORPLAN::LUT6);
			sink_slice->set_swapable_type(FLOORPLAN::LUT6);

			SwapObject* obj = _nl_info.add_swap_object(FLOORPLAN::LUT6);
			obj->fill(src_slice);
			obj->fill(sink_slice);

			_dev_info.pop_rsc_ava_pos(Site::SLICE, rand_idx);
		}
	}

	bool Floorplan::init_place_site()
	{
		for (PLCInstance* inst: static_cast<PLCModule*>(_nl_info.design()->top_module())->instances())
		{
			Site::SiteType site_type = lexical_cast<Site::SiteType>(inst->module_type());

			if(!_dev_info.is_logic_site(site_type)		||
				site_type == Site::GCLKIOB				|| 
				inst->is_fixed()							||
				inst->is_macro()							||
				site_type == Site::VCC)
				continue;

			vector<Point>& ava_pos   = _dev_info.rsc_ava_pos(site_type);
			int			   rand_idx  = rand() % ava_pos.size();
			Site*		   site      = &_fpga.at(ava_pos[rand_idx]).site(site_type);

			while(site->_occ >= site->_capacity)
			{
				_dev_info.pop_rsc_ava_pos(site_type, rand_idx);
				ASSERT(ava_pos.size(), (CONSOLE::PLC_ERROR % "NOT enough resource to place this design."));

				rand_idx = rand() % ava_pos.size();
				site = &_fpga.at(ava_pos[rand_idx]).site(site_type);
			}

			Point pos = ava_pos[rand_idx];
			pos.z     = site->get_unocc_z_pos();
			site->_occ_insts[pos.z] = inst;
			++site->_occ;

			inst->set_curr_logic_pos(pos);
			inst->set_curr_loc_site(site);
			inst->set_swapable_type(FLOORPLAN::SITE);

			SwapObject* obj = _nl_info.add_swap_object(FLOORPLAN::SITE);
			obj->fill(inst);
		}
        return (_nl_info.num_swap_objects() == 0) ;
        
	}

	bool Floorplan::find_init_pos_for_carrychain(int chain_length, std::vector<Point>& pos)
	{
		string DeviceName = FPGADesign::instance()->name();
		int BottomRow = 3;
		if (DeviceName == "FDP500K")
			BottomRow = 5;
		static const int ROW_OF_BOT_CENTER_TILE = _dev_info.device_scale().x - BottomRow;
		static const int rows  = _dev_info.device_scale().x;
		static const int cols  = _dev_info.device_scale().y;

		static int  tile_col   = cols / 2;
		static bool used_lower = cols % 2 ? false : true;

		bool  found = false;
		vector<int> carry_chain1 = DEVICE::carry_chain[0];//对于V2，初始化布局时进位链只考虑布局在0，1位置
	
		Point chain_pos(ROW_OF_BOT_CENTER_TILE, tile_col, 0);
		while(tile_col >= 0 && tile_col < cols)
		{
			if(_fpga.at(chain_pos).exist_site(Site::SLICE))
			{
				for(int row = chain_pos.x; row >= 0; --row)
				{
					Tile& tile = _fpga.at(row, tile_col);
					if(tile.exist_site(Site::SLICE))
					{
						for (int zpos: carry_chain1)
						{
							if(!tile.site(Site::SLICE)._occ_insts[zpos])
							{
								pos.push_back(Point(row, tile_col, zpos));
								if (pos.size() >= chain_length)
									break;
							}
							else
							{
								pos.clear();
								break;					//进位链的起始点只布在z=0的位置
							}
						}
					}
					
					if(pos.size() >= chain_length)
					{
						found = true;
						break;
					}
				}
			}
			
			if (used_lower)
				tile_col = cols - tile_col - 1;
			else
				tile_col = cols - tile_col;
			used_lower  = !used_lower;
			chain_pos.y = tile_col;

			if(found)	return true;
			else		pos.clear();
		}

		if(tile_col < 0 || tile_col >= cols)
		{
			tile_col = cols / 2;
			used_lower = cols % 2 ? false : true;
		}

		return false;
	}

	/************************************************************************/
	/*	功能：找到一个可交换的单元site
	 *	参数：
			from_obj:	要交换的单元
			pos_to:		目的地址
			rlim:		距离约束
	 *	返回值：Result : pair<bool, FLOORPLAN::SwapableType>，说明是哪一种可交换类型
	 *	说明：		
	 *		随机从_available_pos中找到一个可交换的位置，如果不满足rlim，那么就将其
	 *		从_available_pos里面剔除，这样_available_pos也就逐步变小
	 *
	 *		剔除的标准：
	 */
	/************************************************************************/
	Floorplan::Result Floorplan::satisfy_cst_rules_for_site(SwapObject* from_obj, Point& pos_to, double rlim)
	{
		//得到form obj和to obj的信息
		Site& site_from   = from_obj->base_loc_site();
		int   rand_to_idx = rand() % site_from._available_pos.size();
		pos_to			  = site_from._available_pos[rand_to_idx];
		Site& site_to     = _fpga.at(pos_to).site(site_from._type);
		
		// if the site type is macro ,we don't accept this swap
		//得到to obj的宏类型
		FLOORPLAN::SwapableType site_to_macro_type = site_to.macro_type();
		//如果from是carry chain且to是LUT6，那么返回
		if(site_from.macro_type() == FLOORPLAN::CARRY_CHAIN && 
		   site_to_macro_type	  == FLOORPLAN::LUT6)
			return make_pair(false, FLOORPLAN::IGNORE);
		//如果from是fix，且to是LUT6
		if(site_from.has_fixed_insts() && site_to_macro_type == FLOORPLAN::LUT6)
			return make_pair(false, FLOORPLAN::IGNORE);
		// 得到地址信息
		Point pos_from	   = site_from._logic_pos;
		Point phy_pos_from = site_from._owner->phy_pos();
		Point phy_pos_to   = site_to._owner->phy_pos();
		//判断是否在rlim范围内，并且to和from不重合

		// optimization for IOB and BLOCKRAM
		if ( site_from._type == Site::IOB || site_from._type == Site::BLOCKRAM ) 
			rlim = max_rlim();

		if ( abs(phy_pos_from.x - phy_pos_to.x) <= rlim && 
			 abs(phy_pos_from.y - phy_pos_to.y) <= rlim &&
			(pos_from.x != pos_to.x || pos_from.y != pos_to.y) )
		{
			int num_fixed_insts = 0, num_carrychain_insts = 0;
			//对每一个to obj，检查fix和carry chain占用的数目
			for (PLCInstance* inst: site_to._occ_insts) {
				if(inst == NULL) continue;
				//如果是fix
				if(inst->is_fixed()) 
					++num_fixed_insts ;
				//如果被用作carry chain
				else if(inst->swapable_type() == FLOORPLAN::CARRY_CHAIN)
					++num_carrychain_insts;
			}
			//如果fix数目和carry chain的数目小于容量，说明还有可以交换的
			if (num_fixed_insts + num_carrychain_insts < site_to._capacity)
				return make_pair(true, site_to_macro_type);
			//如果fix的数目大于等于容量说明没有可以交换的
			else if(num_fixed_insts >= site_to._capacity)
				site_from.pop_unavailable_pos(rand_to_idx);
			//如果carry chain的数目大于等于容量的话原意为不能pop，
			//因为考虑到carry chain被交换走后该位置就可用于交换。
			//后来发现会当available_pos只有一个位置并且该位置均被carry chain占据后，
			//若不pop会导致死循环，所以还是应该pop
			else if(num_carrychain_insts >= site_to._capacity)
				site_from.pop_unavailable_pos(rand_to_idx);
		} else {
			//如果不在范围内，pop掉
			site_from.pop_unavailable_pos(rand_to_idx);
		}

		return make_pair(false, FLOORPLAN::IGNORE);
	}
	/************************************************************************/
	/*	功能：找到一个可交换的LUT6
	 *	参数：
		from_obj:	要交换的单元
		pos_to:		目的地址
		rlim:		距离约束
	 *	返回值：Result : pair<bool, FLOORPLAN::SwapableType>，说明是哪一种可交换类型
	 *	说明：		
	 *		随机从_available_pos中找到一个可交换的位置，如果不满足rlim，那么就将其
 	 *		从_available_pos里面剔除，这样_available_pos也就逐步变小
	 *
	 *		剔除的标准：
	 */
	/************************************************************************/
	Floorplan::Result Floorplan::satisfy_cst_rules_for_lut6(SwapObject* from_obj, Point& pos_to, double rlim)
	{
		//随机选取一个to position
		Site& site_from   = from_obj->base_loc_site();
		int   rand_to_idx = rand() % site_from._available_pos.size();
		pos_to			  = site_from._available_pos[rand_to_idx];
		Site& site_to     = _fpga.at(pos_to).site(site_from._type);
		//如果site_to有fix，那么返回
		//这里有问题：如果全都是fixed，那么循环退不出
		if(site_to.has_fixed_insts())
			return make_pair(false, FLOORPLAN::IGNORE);
		//如果to obj的宏类型为carry chain,返回
		FLOORPLAN::SwapableType site_to_macro_type = site_to.macro_type();
		if(site_to_macro_type == FLOORPLAN::CARRY_CHAIN) {
			// if not pop here, it will cause infinite loop in some cases
			site_from.pop_unavailable_pos(rand_to_idx);
			return make_pair(false, FLOORPLAN::IGNORE);
		}
		//得到from和to的地址信息
		Point pos_from	   = site_from._logic_pos;
		Point phy_pos_from = site_from._owner->phy_pos();
		Point phy_pos_to   = site_to._owner->phy_pos();
		//满足rlim约束
		if ( abs(phy_pos_from.x - phy_pos_to.x) <= rlim && 
			 abs(phy_pos_from.y - phy_pos_to.y) <= rlim &&
			(pos_from.x != pos_to.x || pos_from.y != pos_to.y) )
		{
			return make_pair(true, site_to_macro_type);
		} else {
			//不满足，剔除
			site_from.pop_unavailable_pos(rand_to_idx);
			return make_pair(false, FLOORPLAN::IGNORE);
		}
	}
	/************************************************************************/
	/*	功能：找到一个可交换的carry chain
	 *	参数：
			from_obj:	要交换的单元
			pos_to:		目的地址
			rlim:		距离约束
	 *	返回值：Result : pair<bool, FLOORPLAN::SwapableType>，说明是哪一种可交换类型
	 *	说明：		
	 *		随机从_available_pos中找到一个可交换的位置，如果不满足rlim，那么就将其
	 *		从_available_pos里面剔除，这样_available_pos也就逐步变小
	 *
	 *		剔除的标准：
	 */
	/************************************************************************/
	Floorplan::Result Floorplan::satisfy_cst_rules_for_carrychain(SwapObject* from_obj, Point& pos_to, double rlim)
	{
// 		Site& site_from   = from_obj->base_loc_site();
// 		int carry_size = DEVICE::carry_chain[0].size();
// 		//生成三次随机书，在给进位链专用的qualified_pos中找可用的坐标
// 		//与找site的坐标相比，这样找出来的坐标肯定是能用的，除非超出了rlim的范围。
// 		SwapObject::QualifiedPos& qualified_pos = from_obj->get_qualified_pos();
// 		int rand_index_of_y = rand()%qualified_pos.size();
// 		SwapObject::QualifiedPos::iterator it = qualified_pos.begin();
// 		for(int i = 0; i < rand_index_of_y; i++)
// 			it++;
// 		int y = it -> first;
// 		int rand_index_of_carry = rand()%DEVICE::carry_chain.size();
// 		int z = rand_index_of_carry * carry_size;
// 		int rand_index_of_x =rand()%qualified_pos[y][rand_index_of_carry].size();
// 		int x = qualified_pos[y][rand_index_of_carry][rand_index_of_x];
// 
// 		pos_to.x = x;
// 		pos_to.y = y;
// 		pos_to.z = z;
// 
// 		Site& site_to      = _fpga.at(pos_to).site(site_from._type);
// 		Point pos_from	   = site_from._logic_pos;
// 		Point phy_pos_from = site_from._owner->phy_pos();
// 		Point phy_pos_to   = site_to._owner->phy_pos();
// 
// 		if ( abs(phy_pos_from.x - phy_pos_to.x) <= rlim && 
// 			 abs(phy_pos_from.y - phy_pos_to.y) <= rlim &&
// 			(pos_from.x != pos_to.x || pos_from.y != pos_to.y) )
// 		{
// 			return make_pair(true, FLOORPLAN::SITE);
// 		} else {
// 			//如果超出了rlim的范围，pop掉相应的坐标，如果是x超出的话，只删除这个x坐标
// 			if(abs(phy_pos_from.x - phy_pos_to.x) > rlim)
// 			{
// 				qualified_pos[y][rand_index_of_carry][rand_index_of_x] =qualified_pos[y][rand_index_of_carry].back();
// 				qualified_pos[y].pop_back();
// 			}
// 			//如果是y超出rlim范围，pop掉整列的坐标
// 			if(abs(phy_pos_from.y - phy_pos_to.y) > rlim)
// 				qualified_pos.erase(y);
// 		
// 			return make_pair(false, FLOORPLAN::IGNORE);
// 		}
		Site& site_from   = from_obj->base_loc_site();
		int   rand_to_idx = rand() % site_from._available_pos.size();
		pos_to			  = site_from._available_pos[rand_to_idx];

		bool is_qualified = false;
		for (int index = 0; index < DEVICE::carry_chain.size(); index++)
		{
			if(from_obj->exist_qualified_pos(_dev_info.device_scale().x, pos_to, index)) {
				is_qualified = true;
				pos_to.z = index * DEVICE::carry_chain[0].size(); 
				break;
			}
		}
		if(!is_qualified) {
			// if not pop here, it will cause infinite loop in some cases
			site_from.pop_unavailable_pos(rand_to_idx);
			return make_pair(false, FLOORPLAN::IGNORE);
		}

		Site& site_to      = _fpga.at(pos_to).site(site_from._type);
		Point pos_from	   = site_from._logic_pos;
		Point phy_pos_from = site_from._owner->phy_pos();
		Point phy_pos_to   = site_to._owner->phy_pos();

		if ( abs(phy_pos_from.x - phy_pos_to.x) <= rlim && 
			abs(phy_pos_from.y - phy_pos_to.y) <= rlim &&
			(pos_from.x != pos_to.x || pos_from.y != pos_to.y) )
		{
			return make_pair(true, FLOORPLAN::SITE);
		} else {
			site_from.pop_unavailable_pos(rand_to_idx);
			return make_pair(false, FLOORPLAN::IGNORE);
		}
	}
	/************************************************************************/
	/*	功能：更新制定列的z坐标的一列的qualified_pos值
	 *	参数：
	 *		tile_col:int,tile中的列坐标
	 *		z_pos:int,该列的z坐标
	 *	返回值：void
	 *	说明：
	 */
	/************************************************************************/	
	void Floorplan::update_qualified_pos_for_carrychain(int tile_col, int index)
	{
		static int num_rows = _fpga.size().x - 1;
		vector<int> carry_chain_index = DEVICE::carry_chain[index];
		int carry_size = DEVICE::carry_chain[0].size();

		for (NLInfo::MacroInfo::CarryChains::value_type& chain: _nl_info.carry_chains()){
			for (SwapObject* obj: chain.second)
				obj->clr_qualified_pos(tile_col, index);
		}

		int				length = 0;
		vector<Point>	consecutive_pos_vec;
		for(int tile_row = num_rows; tile_row >= 0; --tile_row)
		{
			Tile& tile = _fpga.at(tile_row, tile_col);
			if(tile.exist_site(Site::SLICE))
			{
				Site& slice_site = tile.site(Site::SLICE);
				bool continue_flag = true;
				for(int i=0; i < carry_chain_index.size(); i++)
				{
					int zpos=carry_chain_index[i];
					PLCInstance* slice_inst = slice_site._occ_insts[zpos];
					if(!slice_inst || (!slice_inst->is_macro() && !slice_inst->is_fixed()))
					{
						++length;
						if(i == 0)//只把tile中进位链的开始位置作为进位链的起始点
						consecutive_pos_vec.push_back(Point(tile_row, tile_col, zpos));
					}
					else
					{
						continue_flag = false;//如果在一个tile中的进位链出现了被占有的inst，则坐标的连续性被中断
						break;
					}
				}
				if(continue_flag)	
				continue;
			} 
			else if(tile_row > 0)
				continue;
			
			//更新carry chain的可用位置
			if(length != 0)
			{
				for (NLInfo::MacroInfo::CarryChains::value_type& chain: _nl_info.carry_chains()) {
					if(chain.first <= length)
					{
						int index_for_pos = consecutive_pos_vec.size();
						//下面这个while循环计算在存储连续坐标vector中能放下进位链的最后一个坐标的索引号
						//因为consecutive_pos_vec中只存了每个tile中进位链开始的坐标，所以计算方法比较奇怪
						while ((length - (index_for_pos-1)*carry_size) < chain.first)
							index_for_pos--;
						for (SwapObject* obj: chain.second) {
							for(int i = 0; i < index_for_pos; ++i)
								obj->add_qualified_pos(index,consecutive_pos_vec[i]);
						}
					}
				}
				length = 0;
				consecutive_pos_vec.clear();
			}
		} // end of "for"
	}

// 	int Floorplan::index_of_carry_chain(int zpos)
// 	{
// 		bool found = false;
// 		int index = 0;
// 		for (int i = 0; i < DEVICE::carry_chain.size(); i++)
// 		{
// 			vector<int> one_carry_chain = DEVICE::carry_chain[i]
// 			for (int z_pos_of_carry: one_carry_chain)
// 			{
// 				if (z_pos_of_carry == zpos)
// 				{
// 					found = true;
// 					break;
// 				}
// 			}
// 			if (found)
// 			{
// 				index = i;
// 				break;
// 			}
// 		}
// 		ASSERT(found,CONSOLE::PLC_ERROR % (zpos + ": don't have a carry chain."))
// 		return index;
// 	}

	void Floorplan::update_qualified_pos_for_carrychain(SwapInsts& from_insts, SwapInsts& to_insts)
	{
		typedef set<pair<int, int> >	AffectedZCols;
		AffectedZCols					affected_z_cols;

		PLCInstance* inst = NULL;
		Point		 pos;
		int			 index;
		int			 carry_size = DEVICE::carry_chain[0].size();//每条进位链所占的slice个数
		for(int i = 0; i < from_insts.size(); ++i){
			inst = from_insts[i];
			if(inst && lexical_cast<Site::SiteType>(inst->module_type()) == Site::SLICE){
				pos = inst->past_logic_pos();
				//index = index_of_carry_chain(pos.z);
				index = pos.z/carry_size;
				affected_z_cols.insert(make_pair(pos.y, index));
				pos = inst->curr_logic_pos();
				//index = index_of_carry_chain(pos.z);
				index = pos.z/carry_size;
				affected_z_cols.insert(make_pair(pos.y, index));
			}
			inst = to_insts[i];
			if(inst && lexical_cast<Site::SiteType>(inst->module_type()) == Site::SLICE){
				pos = inst->past_logic_pos();
				//index = index_of_carry_chain(pos.z);
				index = pos.z/carry_size;
				affected_z_cols.insert(make_pair(pos.y, index));
				pos = inst->curr_logic_pos();
				//index = index_of_carry_chain(pos.z);
				index = pos.z/carry_size;
				affected_z_cols.insert(make_pair(pos.y, index));
			}
		}
		//对于每一个受影响的列进行更新
		for (const AffectedZCols::value_type& z_col: affected_z_cols)
			update_qualified_pos_for_carrychain(z_col.first, z_col.second);
	}

	void Floorplan::swap_insts_for_site(SwapObject* from_obj,   Point&	   pos_to,
										SwapInsts&  from_insts, SwapInsts& to_insts, 
										FLOORPLAN::SwapableType to_type)
	{
		Site& site_from = from_obj->base_loc_site();
		Site& site_to   = _fpga.at(pos_to).site(site_from._type);

		switch(to_type)
		{
			// only 1 site will be swapped
			case FLOORPLAN::SITE:
				do{
					int rand_to_index = 0;
					rand_to_index = rand() % site_to._capacity;

					// move to
					if (rand_to_index >= site_to._occ){
						pos_to.z = site_to.get_unocc_z_pos();
						ASSERT(pos_to.z != -1, (CONSOLE::PLC_ERROR % "can NOT found unoccupation z pos."));

						from_obj->update_place_info(0, pos_to, site_to);
						from_insts.push_back(from_obj->plc_insts()[0]);
						to_insts.push_back(NULL);
					} else { // swap with
						PLCInstance* to_inst = site_to.get_occ_inst(rand_to_index);
						if (!to_inst) continue;
						pos_to = to_inst->curr_logic_pos();

						to_inst->set_curr_logic_pos(from_obj->base_logic_pos());
						to_inst->set_curr_loc_site(&site_from);
						from_obj->update_place_info(0, pos_to, site_to);
						from_insts.push_back(from_obj->plc_insts()[0]);
						to_insts.push_back(to_inst);
					}
					break;
				} while(true);
				break;
			// 2 slices will be swapped
			case FLOORPLAN::LUT6:
				pos_to.z = 0;
				for(int z = 0; z < DEVICE::NUM_SLICE_PER_TILE; ++z){
					PLCInstance* from_inst = site_from._occ_insts[z];
					PLCInstance* to_inst   = site_to._occ_insts[z];
					if(from_inst){
						from_inst->set_curr_logic_pos(Point(pos_to.x, pos_to.y, z));
						from_inst->set_curr_loc_site(&site_to);
					}
					if(to_inst){
						to_inst->set_curr_logic_pos(Point(site_from._logic_pos.x, site_from._logic_pos.y, z));
						to_inst->set_curr_loc_site(&site_from);
					}
					from_insts.push_back(from_inst);
					to_insts.push_back(to_inst);
				}
				break;
			// only 1 slice will be swapped, meanwhile the to_slice is NOT part of carry chain
			case FLOORPLAN::CARRY_CHAIN:
				for(int z = 0; z < DEVICE::NUM_SLICE_PER_TILE; ++z){
					PLCInstance* to_inst = site_to._occ_insts[z];
					if(!to_inst || to_inst->swapable_type() != FLOORPLAN::CARRY_CHAIN){
						if(to_inst){
							to_inst->set_curr_logic_pos(from_obj->base_logic_pos());
							to_inst->set_curr_loc_site(&site_from);
						}
						pos_to.z = z;
						from_obj->update_place_info(0, pos_to, site_to);
						from_insts.push_back(from_obj->plc_insts()[0]);
						to_insts.push_back(to_inst);
						break;
					}
				}
				break;
			default:
				ASSERT(0, (CONSOLE::PLC_ERROR % "unknown swap object."));
		}
	}

	void Floorplan::swap_insts_for_lut6(SwapObject* from_obj,   Point&	   pos_to,
										SwapInsts&  from_insts, SwapInsts& to_insts, 
										FLOORPLAN::SwapableType to_type)
	{
		Point pos_from  = from_obj->base_logic_pos();
		
		Site& site_from = from_obj->base_loc_site();
		Site& site_to	= _fpga.at(pos_to).site(Site::SLICE);

		for(int z = 0; z < DEVICE::NUM_SLICE_PER_TILE; ++z){
			PLCInstance* from_inst = site_from._occ_insts[z];
			PLCInstance* to_inst   = site_to._occ_insts[z];

			from_inst->set_curr_logic_pos(Point(pos_to.x, pos_to.y, z));
			from_inst->set_curr_loc_site(&site_to);
			if(to_inst){
				to_inst->set_curr_logic_pos(Point(pos_from.x, pos_from.y, z));
				to_inst->set_curr_loc_site(&site_from);
			}
			from_insts.push_back(from_inst);
			to_insts.push_back(to_inst);
		}
	}

	void Floorplan::swap_insts_for_carrychain(SwapObject* from_obj,   Point&	 pos_to,
											  SwapInsts&  from_insts, SwapInsts& to_insts, 
											  FLOORPLAN::SwapableType to_type)
	{
		int				count = 0, length = from_obj->plc_insts().size();
		vector<Point>	chain_to_pos_vec;
		Point			cur_pos = pos_to;
		while(count < length){
			if(_fpga.at(cur_pos).exist_site(Site::SLICE)){
				for(int i = 0; i < DEVICE::carry_chain[0].size(); i++)
				{
					++count;
					Point tmp(cur_pos.x, cur_pos.y, cur_pos.z+i);
					chain_to_pos_vec.push_back(tmp);
					if(count == length)
						break;
				}
				
			}
			--cur_pos.x;
		}
		ASSERT(from_obj->plc_insts().size() == chain_to_pos_vec.size(),
			   (CONSOLE::PLC_ERROR % "illegal chain pos for swap."));

		for(int i = 0; i < length; ++i){
			PLCInstance* from_inst = from_obj->plc_insts()[i];

			Point pos_from  = from_inst->curr_logic_pos();
			Point pos_to	= chain_to_pos_vec[i];

			Site& site_from = *(from_inst->curr_loc_site());
			Site& site_to	= _fpga.at(pos_to).site(Site::SLICE);

			PLCInstance* to_inst   = site_to._occ_insts[pos_to.z];

			from_inst->set_curr_logic_pos(pos_to);
			from_inst->set_curr_loc_site(&site_to);
			if(to_inst){
				to_inst->set_curr_logic_pos(pos_from);
				to_inst->set_curr_loc_site(&site_from);
			}
			from_insts.push_back(from_inst);
			to_insts.push_back(to_inst);
		}
	}

	/************************************************************************/
	/*	功能：找到一个可交换的单元
	*	参数：
			from_obj:	要交换的单元
			pos_to:		目的地址
			rlim:		距离约束
	*	返回值：SwapableType，说明是哪一种可交换类型
	*	说明：
	*		每个site最初在_available_pos变量里面存储了整个芯片范围可交换的位置，
	*		随着SA的过程，rlim逐渐变小，其可交换范围逐步缩小
	*		当执行find_to过程中调用satisfy_cst_rules_for_XXXX时，
	*		随机从_available_pos中找到一个可交换的位置，如果不满足rlim，那么就将其
	*		从_available_pos里面剔除，这样_available_pos也就逐步变小
	*/
	/************************************************************************/
	FLOORPLAN::SwapableType Floorplan::find_to(SwapObject* from_obj, Point& pos_to, double rlim)
	{
		Site& site_from = from_obj->base_loc_site();
		do {
			//如果没有可交换位置，返回
			if(site_from._available_pos.size() == 0)
				return FLOORPLAN::IGNORE;
			//寻找一个可交换位置
			bool					has_found = false;
			FLOORPLAN::SwapableType	to_type   = FLOORPLAN::IGNORE;
			switch(from_obj->type())
			{
				case FLOORPLAN::SITE:
					boost::tie(has_found, to_type) 
						= satisfy_cst_rules_for_site(from_obj, pos_to, rlim);
					break;
				case FLOORPLAN::LUT6:
					boost::tie(has_found, to_type)
						= satisfy_cst_rules_for_lut6(from_obj, pos_to, rlim);
					break;
				case FLOORPLAN::CARRY_CHAIN:
					boost::tie(has_found, to_type)
						= satisfy_cst_rules_for_carrychain(from_obj, pos_to, rlim);
					break;
				default:
					ASSERT(0, (CONSOLE::PLC_ERROR % "unknown swap object."));
			}
			//找到，返回
			if(has_found)
				return to_type;

		} while(true);
	}
	/************************************************************************/
	/*	功能：交换两个单元instance
	*	参数：
			from_insts:	源交换单元
			to_insts:	目的交换单元
			pos_from:	源位置
			pos_to:		目的地址
			rlim:		距离约束
	*	返回值：SwapableType，说明是那一种可交换类型
	*	说明：随机挑选一个From obj，然后寻找一个to obj，最后进行交换
	*/
	/************************************************************************/
	void Floorplan::swap_insts(SwapInsts& from_insts, SwapInsts& to_insts, 
							   Point&	  pos_from,	  Point&	 pos_to, 
							   double	  rlim)
	{
		FLOORPLAN::SwapableType to_type;
		SwapObject* from_obj = NULL;
		//如果这里_nl_info.num_swap_objects()为0，那么就不会跳出循环，这里以后需要修改
		do{
			//随机找到一个可以交换的from obj，这个是从网表信息里面寻找
			int rand_from_idx  = rand() % _nl_info.num_swap_objects();
			from_obj = _nl_info.swap_object(rand_from_idx);
			//找到一个可交换to obj
			to_type = find_to(from_obj, pos_to, rlim);
			//如果没有找到一个可交换 to obj，那么从swap_obj里面剔除这个from obj
			if(to_type == FLOORPLAN::IGNORE)
				_nl_info.pop_swap_object(rand_from_idx);

		} while(to_type == FLOORPLAN::IGNORE);
		//交换相应的isntance
		switch(from_obj->type())
		{
			case FLOORPLAN::SITE:
				swap_insts_for_site(from_obj, pos_to, from_insts, to_insts, to_type);
				break;
			case FLOORPLAN::LUT6:
				swap_insts_for_lut6(from_obj, pos_to, from_insts, to_insts, to_type);
				break;
			case FLOORPLAN::CARRY_CHAIN:
				swap_insts_for_carrychain(from_obj, pos_to, from_insts, to_insts, to_type);
				break;
		}
	}
	/************************************************************************/
	/*	功能：try swap以后根据是否能交换来决定是交换还是回退操作
	 *	参数：
	 *		keey_swap: bool,判断交换是否被接受
	 *		from_insts:SwapInsts,from obj
	 *		to_insts:SwapInsts,to obj
	 *	返回值：void
	 *	说明：
	 */
	/************************************************************************/	
	void Floorplan::maintain(bool		 keey_swap, 
							 SwapInsts&  from_insts,  SwapInsts& to_insts)
	{
		//判断两个交换对象是否是相同大小
		ASSERT(from_insts.size() == to_insts.size(),
			   (CONSOLE::PLC_ERROR % "swap instances' size not equal."));
		
		int size = from_insts.size();
		//对于每一交换的inst
		for(int i = 0; i < size; ++i){
			PLCInstance* from_inst = from_insts[i];
			PLCInstance* to_inst   = to_insts[i];
				   
			Point			pos_from, pos_to;
			Site::SiteType	site_type;
			//已经交换过了
			if(from_inst){
				pos_from  = from_inst->past_logic_pos();
				pos_to	  = from_inst->curr_logic_pos();
				site_type = from_inst->curr_loc_site()->_type;
			}
			else if(to_inst){
				pos_from  = to_inst->curr_logic_pos();
				pos_to    = to_inst->past_logic_pos();
				site_type = to_inst->curr_loc_site()->_type;
			}
			else
				throw (CONSOLE::PLC_ERROR % "both from_inst & to_inst are NULL.");
			//得到site from和site to
			Site& site_from = _fpga.at(pos_from).site(site_type);
			Site& site_to   = _fpga.at(pos_to).site(site_type);
			//如果交换被接受
			if(keey_swap){
				//
				if(_mode == PLACER::TIMING_DRIVEN){
					update_tcost(from_inst, to_inst);
					update_tcost(to_inst, from_inst);
				}
				//设置site_from和site_to
				site_from._occ_insts[pos_from.z] = to_inst;
				site_to._occ_insts[pos_to.z]	 = from_inst;
				//如果from有单是to没有，那么更新
				if(from_inst && !to_inst){
					--site_from._occ;
					++site_to._occ;
				} 
				//如果from没有，但是to有,更新
				else if(!from_inst && to_inst){
					++site_from._occ;
					--site_to._occ;
				}
//				else if(from_inst && to_inst) { /* need to do nothing */ }
			}
			// swap was rejected,reset all information
			else{
				//恢复from obj
				if(from_inst){
					from_inst->reset_logic_pos();
					from_inst->reset_loc_site();
				}
				//恢复to obj
				if(to_inst){
					to_inst->reset_logic_pos();
					to_inst->reset_loc_site();
				}
			}
		} // end of "for"
		//对线网的处理
		typedef pair<PLCInstance*, PLCNet*> AffectedNet;
		if(keey_swap){
			//如果交换接受，清除线网的pre_bb_cost和affected type
			for (AffectedNet& affected_net: _nl_info.affected_nets()) {
				affected_net.second->reset_pre_bb_cost();
				affected_net.second->reset_affected_type();
			}
		} else {
			//如果交换没有被接受
			for (AffectedNet& affected_net: _nl_info.affected_nets()) {
				affected_net.second->restore_bounding_box();
				affected_net.second->reset_affected_type();
			}
		}
		//如果被交换，重新计算受影响的列的_qualified_pos
		if(keey_swap)
			update_qualified_pos_for_carrychain(from_insts, to_insts);
	}
	/************************************************************************/
	/*	功能：更新一个isnts的受影响线网
	 *	参数：
	 *		insts:SwapInsts,受影响的swap insts
	 *		t:AffectedType,影响线网的类型
	 *	返回值：void
	 *	说明：
	 */
	/************************************************************************/	
	void Floorplan::find_affected_nets(SwapInsts& insts, PLCNet::AffectedType t)
	{
		for (PLCInstance* inst: insts) {
			if(inst){
				for (Pin* pin: inst->pins()) {
					PLCNet* p_net = static_cast<PLCNet*>(pin->net());
					if (p_net && !p_net->is_ignored()){
						//这里以前是错的，所以注释
						//p_net->set_affected_type(PLCNet::FROM);
						p_net->set_affected_type(t);
						//防止重复增加，如果<0，说明没有加入过里面
						if (p_net->pre_bb_cost() < 0.){
							_nl_info.add_affected_net(inst, p_net);
							p_net->set_pre_bb_cost(1.0);
						}
					}
				}
			}
		}
	}

	/************************************************************************/
	/*	功能：更新一次交换后两个insts的受影响线网
	 *	参数：
	 *		from_insts:SwapInsts,受影响的swap insts
	 *		to_insts:SwapInsts,受影响的swap insts
	 *	返回值：void
	 *	说明：
	 */
	/************************************************************************/	
	void Floorplan::find_affected_nets(SwapInsts& from_insts, SwapInsts& to_insts)
	{
		//先将受影响线网清除
		_nl_info.clr_affected_nets();
		//分别计算FROM和TO的受影响线网
		find_affected_nets(from_insts, PLCNet::FROM);
		find_affected_nets(to_insts,   PLCNet::TO);
	}

	double Floorplan::compute_bb_cost()
	{
		double cost = 0.;
		for (PLCNet* net: static_cast<PLCModule*>(_nl_info.design()->top_module())->nets()){
			if (net->is_ignored())
				continue;

			net->compute_bounding_box(_dev_info.device_scale()); 
			cost += net->compute_bb_cost(_dev_info.inv_of_chan_width());
		}

		return cost;
	}

	double Floorplan::compute_tcost(TEngine::WORK_MODE emode, double crit_exp)
	{
		double dmax;
		double tcost = 0.;
		PLCInstance *source_inst, *sink_inst;
		Property<COS::TData>& delays = 
			create_temp_property<COS::TData>(COS::PIN, PIN::DELAY);
		//if(dynamic_cast<TDesign*>(_nl_info.design()) != NULL )
			dmax = _nl_info.design()->timing_analyse(emode);

		for (PLCNet* net: static_cast<PLCModule*>(_nl_info.design()->top_module())->nets()){
			if (net->is_ignored())
				continue;

			source_inst = static_cast<PLCInstance*>(net->source_pins().begin()->owner());
			for (Pin* pin: net->sink_pins()){
				sink_inst = static_cast<PLCInstance*>(pin->owner());

				double delay = _dev_info.get_p2p_delay(source_inst, sink_inst);
				pin->set_property(delays, TData(delay, delay));
			}

			tcost += net->compute_tcost(dmax, crit_exp);
		}

		return tcost;
	}

	double Floorplan::compute_delta_bb_cost(SwapInsts& from_insts, SwapInsts& to_insts)
	{
		find_affected_nets(from_insts, to_insts);

		double delta_bb_cost = 0.;
		typedef pair<PLCInstance*, PLCNet*> AffectNet;
		for (const AffectNet& net: _nl_info.affected_nets()) {
			PLCInstance* cause_inst   = net.first;
			PLCNet*		 affected_net = net.second;

			affected_net->save_bounding_box();

			if((affected_net->affected_type() & PLCNet::FROM) && 
			   (affected_net->affected_type() & PLCNet::TO))
				continue;

			if(affected_net->affected_type() & PLCNet::FROM)
				affected_net->update_bounding_box(cause_inst->past_logic_pos(), 
												  cause_inst->curr_logic_pos(), 
												  _dev_info.device_scale());
			else if(affected_net->affected_type() & PLCNet::TO)
				affected_net->update_bounding_box(cause_inst->past_logic_pos(),
												  cause_inst->curr_logic_pos(),
												  _dev_info.device_scale());

			affected_net->compute_bb_cost(_dev_info.inv_of_chan_width());

			delta_bb_cost += affected_net->bb_cost() - affected_net->pre_bb_cost();
		}

		return delta_bb_cost;
	}

	double Floorplan::compute_delta_tcost(PLCInstance* target, PLCInstance* reference)
	{
		double delta_delay = 0., delta_tcost = 0.;
		Property<COS::TData>& delays = 
			create_temp_property<COS::TData>(COS::PIN, PIN::DELAY);
		Property<double>& tcosts = 
			create_temp_property<double>(COS::PIN, PIN::TCOST);
		Property<double>& temp_delays = 
			create_temp_property<double>(COS::PIN, PIN::TEMP_DELAY);
		Property<double>& temp_tcost = 
			create_temp_property<double>(COS::PIN, PIN::TEMP_TCOST);
		Property<double>& crits = 
			create_temp_property<double>(COS::PIN, PIN::CRIT);

		if(!target)	return delta_tcost;

		for (Pin* pin: target->pins()) {
			PLCNet* p_net = static_cast<PLCNet*>(pin->net());
			if (!p_net || p_net->is_ignored())
				continue;

			if (pin->is_sink()){
				PLCInstance* p_inst = static_cast<PLCInstance*>(p_net->source_pins().begin()->owner());
				if (p_inst != target && p_inst != reference){
					double delay = _dev_info.get_p2p_delay(p_inst, target);
					double tcost = pin->property_value(crits) * delay;
					pin->set_property(temp_delays, delay);
					pin->set_property(temp_tcost, tcost);

					delta_delay += delay - pin->property_value(delays)._rising;
					delta_tcost += tcost - pin->property_value(tcosts);
				}
			} else { // pin is source
				for (Pin* sink_pin: p_net->sink_pins()) {
					PLCInstance* p_inst = static_cast<PLCInstance*>(sink_pin->owner());
					double delay = _dev_info.get_p2p_delay(target, p_inst);
					double tcost = sink_pin->property_value(crits) * delay;
					sink_pin->set_property(temp_delays, delay);
					sink_pin->set_property(temp_tcost, tcost);

					delta_delay += delay - sink_pin->property_value(delays)._rising;
					delta_tcost += tcost - sink_pin->property_value(tcosts);
				}
			}	
		}

		return delta_tcost;
	}

	double Floorplan::compute_delta_tcost(SwapInsts& from_insts, SwapInsts& to_insts)
	{
		double delta_tcost = 0.;

		for(int i = 0; i < from_insts.size(); ++i){
			delta_tcost += compute_delta_tcost(from_insts[i], to_insts[i]);
			delta_tcost += compute_delta_tcost(to_insts[i], from_insts[i]);
		}

		return delta_tcost;
	}

	pair<double, double> Floorplan::recompute_cost()
	{
		double new_bb_cost = 0., new_tcost = 0.;
		for (PLCNet* net: static_cast<PLCModule*>(_nl_info.design()->top_module())->nets()) {
			if (net->is_ignored())
				continue;

			new_bb_cost += net->bb_cost();
			if (_mode == PLACER::TIMING_DRIVEN)
				new_tcost += net->get_total_tcost();
		}

		return make_pair(new_bb_cost, new_tcost);
	}

	void Floorplan::update_tcost(PLCInstance* target, PLCInstance* reference)
	{
		Property<COS::TData>& delays = 
			create_temp_property<COS::TData>(COS::PIN, PIN::DELAY);
		Property<double>& tcosts = 
			create_temp_property<double>(COS::PIN, PIN::TCOST);
		Property<double>& temp_delays = 
			create_temp_property<double>(COS::PIN, PIN::TEMP_DELAY);
		Property<double>& temp_tcost = 
			create_temp_property<double>(COS::PIN, PIN::TEMP_TCOST);

		if(!target) return;

		for (Pin* pin: target->pins()) {
			PLCNet* p_net = static_cast<PLCNet*>(pin->net());
			if (!p_net || p_net->is_ignored())
				continue;

			if (pin->is_sink()){
				PLCInstance* p_inst = static_cast<PLCInstance*>(p_net->source_pins().begin()->owner());
				if (p_inst != target && p_inst != reference) {
					double temp_value = pin->property_value(temp_delays);
					pin->set_property(delays, TData(temp_value, temp_value));

					temp_value = pin->property_value(temp_tcost);
					pin->set_property(tcosts,temp_value);
				}
			} else {
				for (Pin* sink_pin: p_net->sink_pins()) {
					double temp_value = sink_pin->property_value(temp_delays);
					sink_pin->set_property(delays, TData(temp_value, temp_value));

					temp_value = sink_pin->property_value(temp_tcost);
					sink_pin->set_property(tcosts, temp_value);
				}
			}			
		}	
	}

	/************************************************************************/
	/*	功能：设定网表设计中的一些nets为ignored
	 *	参数：void
	 *	返回值： void
	 *	说明：1. carry chain里面的线网设置为ignored
	 *		  2. net只有名字没有pins(ISE下来后会出现这个情况)
	 *		  3. CONST_GENERATE里面的用到，目前版本不处理
	 *		  4. GCLKIOB的net设为ignored
	 *		  5. Port的CELL_pin也设置为ignored（同时检查cell_pin是否连接）
	 */
	/************************************************************************/
	void Floorplan::set_ignored_nets()
	{
		for (PLCNet* net: static_cast<PLCModule*>(_nl_info.design()->top_module())->nets()) {
			if( net->type() == COS::CARRY ||
				net->type() == COS::CLOCK ||
				net->type() == COS::VCC	 ||
			   !net->num_pins()				 
#ifndef CONST_GENERATE
			   || !count_if(net->pins(), [](const Pin* pin) { return pin->is_source(); })
#endif
			  )
			   net->set_ignored();
		}

		for (PLCInstance* inst: static_cast<PLCModule*>(_nl_info.design()->top_module())->instances()) {
			Site::SiteType site_type = lexical_cast<Site::SiteType>(inst->module_type());
			if (site_type == Site::GCLKIOB){
				for (Pin* pin: inst->pins())
					if (pin->net())
						static_cast<PLCNet*>(pin->net())->set_ignored();
			}
		}

		for (Pin* pin: static_cast<PLCModule*>(_nl_info.design()->top_module())->pins()) {
//			ASSERT(port.cell_pin().net(), 
//				   place_error(CONSOLE::PLC_ERROR % (port.name() + ": cell pin unconnect.")));
			// allow cell pin of top cell unconnect 
			//if(pin->name()=="out[2]")
			//	int a=2;
			if (pin->net())
				static_cast<PLCNet*>(pin->net())->set_ignored();
		}
	}

	/************************************************************************/
	/*	功能：检查约束文件的正确性
	*	参数：void
	*	返回值： void
	*	说明：1. carry chain里面的线网设置为ignored
	*		  2. 
	*		  3. 
	*		  4. GCLKIOB的net设为ignored
	*		  5. Port的CELL_pin也设置为ignored（同时检查cell_pin是否连接）
	*/
	/************************************************************************/
	void Floorplan::check_and_save_cst()
	{
		const int nx = _dev_info.device_scale().x;
		const int ny = _dev_info.device_scale().y;
		Property<string>& pads = create_temp_property<string>(COS::INSTANCE, "pad");
		Property<Point>& positions = create_property<Point>(COS::INSTANCE, INSTANCE::POSITION);
		Property<string>& SET_TYPE = create_property<string>(COS::INSTANCE, INSTANCE::SET_TYPE);

		Point invalid_pos;
		for (PLCInstance* inst: static_cast<PLCModule*>(_nl_info.design()->top_module())->instances()) {
			//如果约束位置不正确，返回
			string position = inst->property_value(pads);
			//inst没有被约束
			if(position == "")
				continue;
			
			Point pos;
			try
			{ pos = lexical_cast<Point>(position); }
			catch (boost::bad_lexical_cast& e)
			{ pos = FPGADesign::instance()->find_pad_by_name(position); }
			ASSERT(pos != Point(), ("parse failed. constraint: illegal position for " + inst->name()));

			
			inst->set_property(positions,pos);
			Point logic_pos = inst->property_value(positions);
			//如果约束位置不在逻辑范围之内
			ASSERT(logic_pos.x >= 0 && logic_pos.x <= nx &&
				   logic_pos.y >= 0 && logic_pos.y <= ny &&
				   logic_pos.z >= 0,
				   (CONSOLE::PLC_ERROR % (inst->name() + ": illegal constraint.")));
			//约束的类型不是FPGA接受类型
			Site::SiteType site_type = lexical_cast<Site::SiteType>(inst->module_type());

			ASSERT(site_type != Site::IGNORE, 
				   (CONSOLE::PLC_ERROR % (inst->name() + ": illegal cell type.")));
			//在该位置上是否存在约束的site类型
			ASSERT(_fpga.at(logic_pos).exist_site(site_type),
				   (CONSOLE::PLC_ERROR % (inst->name() + ": illgal constraint.")));
			//如果是IOB或是GCLKIOB，那么判断是否封装出来
			if(site_type == Site::IOB || site_type == Site::GCLKIOB){
				//is_io_bound，判断是否被封装出来
				ASSERT(FPGADesign::instance()->is_io_bound(logic_pos),
					   (CONSOLE::PLC_ERROR % (inst->name() + ": illegal constraint.")));
			}
			//获得FPGA位置为logic_pos类型为site_type的site
			Site& site = _fpga.at(logic_pos).site(site_type);
			//判断z坐标是否满足
			ASSERT(logic_pos.z < site._occ_insts.size(), 
				   (CONSOLE::PLC_ERROR % (inst->name() + ": illegal constraint.")));
			//判断是否有重复约束到同一位置
			ASSERT(!site._occ_insts[logic_pos.z],
				   (CONSOLE::PLC_ERROR % (inst->name() + ": illegal constraint.")));
			//不能约束carry chain
			//ASSERT(inst->property_value(SET_TYPE) != DEVICE::CARRY,
			//	   place_error(CONSOLE::PLC_ERROR % (inst->name() + ": illegal constraint.")));
			//不能约束LUT6
			ASSERT(inst->property_value(SET_TYPE) != DEVICE::LUT_6,
				   (CONSOLE::PLC_ERROR % (inst->name() + ": illegal constraint.")));
			//将instance放入约束的site里面
			site._occ_insts[logic_pos.z] = inst;
			++site._occ;
			//设置inst的属性
			inst->set_curr_logic_pos(logic_pos);
			inst->set_curr_loc_site(&site);
			inst->set_swapable_type(FLOORPLAN::SITE);
			inst->fix_inst();

			// ugly hard code, need optimization
			//GCLKIOB的约束判断：GCLK包括： GCLK_pin ， GCLK buffer
			if (site_type == Site::GCLKIOB){
				Pin* gclkout_pin = inst->pins().find(DEVICE::GCLKOUT);
				ASSERT(gclkout_pin && gclkout_pin->net(),
					   (CONSOLE::PLC_ERROR % (inst->name() + ": NOT connect to GCLK.")));
				PLCInstance* gclkbuf = 
					static_cast<PLCInstance*>(gclkout_pin->net()->sink_pins().begin()->owner());

				Point gclkbuf_pos = gclkbuf->property_value(positions);
				if (gclkbuf_pos == invalid_pos){
					Site& gclk_site = _fpga.at(logic_pos).site(Site::GCLK);

					gclk_site._occ_insts[logic_pos.z] = gclkbuf;
					++gclk_site._occ;

					gclkbuf->set_curr_logic_pos(logic_pos);
					gclkbuf->set_curr_loc_site(&gclk_site);
					gclkbuf->set_swapable_type(FLOORPLAN::SITE);
					gclkbuf->fix_inst();
				} else {
					ASSERT(gclkbuf_pos == logic_pos, 
						   (CONSOLE::PLC_ERROR % (gclkbuf->name() + ": pos not equal to GCLKIOB " + inst->name())));
				}
			}		
		}
	}

}}