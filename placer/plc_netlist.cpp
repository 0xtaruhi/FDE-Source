#include "plc_netlist.h"
#include "plc_factory.h"
#include "plc_utils.h"
#include "arch/archlib.hpp"
#include <boost/lexical_cast.hpp>

#include <string>  
#include <iostream>
#include <fstream>

#include "report/report.h"

namespace FDU { namespace Place {

	using boost::lexical_cast;
	using namespace boost;
	using namespace std;
	using namespace ARCH;
	using namespace rapidxml;
	//////////////////////////////////////////////////////////////////////////
	// SwapObject
	/************************************************************************/
	/* 得到一个swapobj的逻辑地址                                            */
	/************************************************************************/
	Point SwapObject::base_logic_pos() const 
	{
		switch(_type)
		{
		case FLOORPLAN::SITE: 
			return _insts[0]->curr_logic_pos();
		case FLOORPLAN::LUT6: 
			return _insts[0]->curr_logic_pos();
		case FLOORPLAN::CARRY_CHAIN:
			return _insts[0]->curr_logic_pos();
		default:
			ASSERT(0, (CONSOLE::PLC_ERROR % "unknown swap object.")) ;
		}
	}
	/************************************************************************/
	/* 得到该swap obj的site                                                 */
	/************************************************************************/
	Site& SwapObject::base_loc_site() const 
	{
		switch(_type)
		{
		case FLOORPLAN::SITE: 
			return *_insts[0]->curr_loc_site();
		case FLOORPLAN::LUT6: 
			return *_insts[0]->curr_loc_site();
		case FLOORPLAN::CARRY_CHAIN:
			return *_insts[0]->curr_loc_site();
		default:
			ASSERT(0, (CONSOLE::PLC_ERROR % "unknown swap object."));
		}
	}
	/************************************************************************/
	/* 功能：更新一个swapobj的逻辑位置和单元类型                                  
	* 参数：
	*		inst_idx：int，要修改的instance的index；
	*		pos:Point，要更新的目标逻辑位置
	*		loc_site:Site，要更新的目标site
	*/
	/************************************************************************/
	void SwapObject::update_place_info(int inst_idx, const Point& pos, Site& loc_site)
	{
		_insts[inst_idx]->set_curr_logic_pos(pos);
		_insts[inst_idx]->set_curr_loc_site(&loc_site);
	}

	//////////////////////////////////////////////////////////////////////////
	// NLInfo
	/************************************************************************/
	/* 功能：检查是否同一个位置上有两个单元                                       
	* 参数：void
	* 返回值：void
	* 说明：GCLKIOB忽略
	*/
	/************************************************************************/
	void NLInfo::check_place()
	{
		vector<map<Point, PLCInstance*> > pos_checker(Site::NUM_OF_SITE_TYPE);
		//对网表中的每一个inst检查是否有位置重合
		for (PLCInstance* inst: static_cast<PLCModule*>(_design->top_module())->instances()) {
			Site::SiteType site_type = lexical_cast<Site::SiteType>(inst->module_type());
			//忽略GCKIOB
			if (site_type == Site::GCLKIOB||site_type == Site::VCC) 
				continue;
			//检查是否存在非法inst，如没有pack成功的inst
			ASSERT(site_type != Site::IGNORE, 
				(CONSOLE::PLC_ERROR % (inst->name() + ": invalid instance type.")));
			//检查是否有重合
			ASSERT(!pos_checker[site_type].count(inst->curr_logic_pos()),
				(CONSOLE::PLC_ERROR % (inst->name() + ": overlap position.")));
			//没有重合的话直接设置好该map vector
			pos_checker[site_type][inst->curr_logic_pos()] = inst;
		}
	}
	/************************************************************************/
	/* 功能：保存布局的信息                                       
	* 参数：void
	* 返回值：void
	* 说明：GCLKIOB特殊处理
	*/
	/************************************************************************/	
	void NLInfo::save_place()
	{
		Property<Point>& positions = create_property<Point>(COS::INSTANCE, INSTANCE::POSITION);
		for (PLCInstance* inst: static_cast<PLCModule*>(_design->top_module())->instances()) {
			Site::SiteType site_type = lexical_cast<Site::SiteType>(inst->module_type());
			//处理GCLK
			if (site_type == Site::GCLKIOB){
				Pin* gclkout_pin = inst->pins().find(DEVICE::GCLKOUT);
				ASSERT(gclkout_pin, (CONSOLE::PLC_ERROR % (inst->name() + ": can NOT place GCLKIOB.")));
				PLCInstance* gclkbuf = 
					static_cast<PLCInstance*>(gclkout_pin->net()->sink_pins().begin()->owner());
				inst->set_curr_logic_pos(gclkbuf->curr_logic_pos());
			}
			//保存每一个instance的position信息

			inst->set_property(positions, inst->curr_logic_pos());
		}
		/************************************************************************/
		/*处理V2中的VCC器件，配合ISE流程用*/
		/************************************************************************/	
		for (PLCNet* net: static_cast<PLCModule*>(_design->top_module())->nets())
		{
			PLCNet::pin_iter vcc_pin  = find_if(net->pins(), [](const Pin* pin) { return pin->name() == DEVICE::VCCOUT; });
			if (vcc_pin != net->pins().end())
			{
				PLCInstance* VCC = static_cast<PLCInstance*>(vcc_pin->owner());
				ASSERT(lexical_cast<Site::SiteType>(VCC->module_type())==Site::VCC, (CONSOLE::PLC_ERROR % (VCC->name() + ": is not a VCC device")));
				PLCInstance* inst = static_cast<PLCInstance*>(net->sink_pins().begin()->owner());
				Point invalid_pos;
				Point pos = inst->property_value(positions);
				ASSERT(pos!=invalid_pos, (CONSOLE::PLC_ERROR % (inst->name() + ": is not placed")));
				ArchCell* tile_cell = FPGADesign::instance()->get_inst_by_pos(pos)->down_module();
				ASSERTD(find_if(tile_cell->instances(), [](const ArchInstance* inst) { return inst->module_type() == lexical_cast<string>(Site::VCC) })
						!= tile_cell->instances().end(),
					(CONSOLE::PLC_ERROR % ("VCC site does not exist at:" + lexical_cast<string>(pos))));
				VCC->set_property(positions, pos);
			}
		}
		// 		for (PLCNet& net: static_cast<PLCModule&>(_design->top_cell()).nets())
		// 			net.clear_pips();
	}
	/************************************************************************/
	/* 功能：统计用的资源                                      
	* 参数：void
	* 返回值：void
	* 说明：
	*/
	/************************************************************************/	
	void NLInfo::classify_used_resource()
	{
		for (PLCInstance* inst: static_cast<PLCModule*>(_design->top_module())->instances()){
			//如果已经有一个，那么该类型+1
			if(_rsc_stat.count(inst->module_type()))
				_rsc_stat[inst->module_type()] += 1;
			//以前没有，那么为1
			else
				_rsc_stat[inst->module_type()]  = 1;
		}

#ifdef UNUSED
		int carrychain_count = 0;
		for (MacroInfo::CarryChains::value_type& chains: _macro_info._carrychains)
			carrychain_count += chains.second.size();

		if(carrychain_count)
			_rsc_stat[Module::CARRY_CHAIN] = carrychain_count;
#endif
	}
	/************************************************************************/
	/* 功能：统计网表中的资源信息                                       
	* 参数：void
	* 返回值：void
	* 说明：
	*/
	/************************************************************************/	
	void NLInfo::echo_netlist_resource()
	{
		//design name
		INFO(CONSOLE::DESIGN % _design->name());
		//输出每一个类型的树木
		for (RscStat::value_type& rsc: _rsc_stat)
			INFO(CONSOLE::RSC_IN_DESIGN % rsc.first % rsc.second);
		//输出线网数目
		INFO(CONSOLE::RSC_IN_DESIGN % "Net" % _design->top_module()->num_nets());
	}
	/************************************************************************/
	/* 功能：输出资源利用率                                      
	* 参数：void
	* 返回值：void
	* 说明：
	*/
	/************************************************************************/	
	void NLInfo::echo_resource_usage(DeviceInfo::RscStat& dev_rsc)
	{
		//器件类型
		INFO(CONSOLE::DEVICE_TYPE % ARCH::FPGADesign::instance()->name());
		//输出每一类型的东西所用的percent
		for (const RscStat::value_type& rsc: _rsc_stat) {
			Site::SiteType type = lexical_cast<Site::SiteType>(rsc.first);

			if(dev_rsc.count(type)){
				double percent = (double)rsc.second / dev_rsc[type] *100.;
				if(type == Site::SLICE)
					INFO(CONSOLE::RSC_SLICE % rsc.first % DEVICE::NUM_LUT_INPUTS % percent);
				else			
					INFO(CONSOLE::RSC_USAGE % rsc.first % percent);				
			}
		}
	}
	/************************************************************************/
	/* 功能：统计网表中的资源信息                                       
	* 参数：void
	* 返回值：void
	* 说明：
	*/
	/************************************************************************/	
	
	void NLInfo::writeReport(DeviceInfo::RscStat& dev_rsc, string output_file)
	{
		using namespace FDU::RPT;
		char val_string[25];
		char percent_string[25];
		double percent;

		string design_name = output_file;
		int str_len = design_name.length();
		design_name = design_name.replace(str_len - 8, 8, "");
		Report* rpt = new Report();
		rpt->set_design(design_name);
		
		Section* sec_type = rpt->create_section("Type_Count", "Type Count");
		Table* table_type = sec_type->create_table("General_Table");
		table_type->create_column("Type_Name", "Type Name");
		table_type->create_column("Count", "Count");

		for (RscStat::value_type& rsc: _rsc_stat)
		{
			Site::SiteType type = lexical_cast<Site::SiteType>(rsc.first);
			if(dev_rsc.count(type)){
				percent = (double)rsc.second / dev_rsc[type] *100.;
				sprintf(percent_string, "%.2f%%", percent);
				sprintf(val_string, "%d", rsc.second);

				Row* row = table_type->create_row();
				row->set_item("Type_Name", rsc.first);
				row->set_item("Count", rsc.second);
			}
		}

		rpt->write(design_name+"_plc_rpt.xml");
		
	}

}}