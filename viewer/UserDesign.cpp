#include "UserDesign.h"
#include "Arch.h"
#include "ViewWidget.h"
#include <stdexcept>

//#define  DRAW_BOTH

namespace FPGAViewer {

	inline 	void merge_items(Lines& origin, const Lines& new_items)
	{
		for (auto& item : new_items) origin.push_back(item);
	}

	void UserDesign::load(const string& file_name)
	{
		_design = new Design;
		_design_file_path = QString::fromStdString(file_name);

		CosFactory::set_factory(new COSRTFactory());
		_design->load("xml", file_name);
	}

	void UserDesign::clear()
	{
		hide_placement();
		show_route(false);
		clear_route();
		clear_modify();
		delete _design;
		_design = nullptr;
	}

	void UserDesign::clear_route()
	{
		if (!_route_added) return;
		for (auto& [name, route] : _net_map)
			delete route;

		_net_map.clear();
		_route_drawed = false;
		_route_added = false;
	}

	void UserDesign::clear_modify()
	{
		_net_chg_map.clear();
		_plc_map.clear();
	}

	void UserDesign::hide_placement()
	{
		if (_placement_drawed) {
			_arch->clear_placement();
			_parent->update_view();
			_placement_drawed = false;
		}
	}

	void UserDesign::show_placement()
	{
		if (_placement_drawed) return;

		Property<Point>& POS = create_property<Point>(INSTANCE, "position");
		Property<string>& SET_TYPE = create_temp_property<string>(INSTANCE, "set_type");

		for (Instance* inst : _design->top_module()->instances())
		{
			if (!inst->property_exist(POS)) continue;
			Point p = inst->property_value(POS); // get used position

			_arch->set_cluster_used(p);
			Point inst_pos = { p.x, p.y };

			// get set type
			string set_type = inst->property_value(SET_TYPE);

			if (Tile * p_tile_inst = _arch->get_inst_by_pos(inst_pos))
			{	// set tile used info
				p_tile_inst->set_used(true);

				// judge if the instance is IOB or SLICE
				string type = inst->down_module()->type();
				if (type != "SLICE" && type != "IOB") continue;
				bool movable = set_type == "" || set_type != "carry" && set_type != "lut6";
				_arch->set_cluster_movable(p, movable);
			}
		}
		_placement_drawed = true;
		_parent->update_view();
	}

	void UserDesign::add_route()
	{
		if (!_design || _route_added) return;

		// begin to add route and draw it.
		for (const Net* net : _design->top_module()->nets())
		{
			// draw pips
			auto rt_net = static_cast<const COSRTNet*>(net);
			Route* route_net = new Route(net->name());
			bool draw_from_port = true;
			for (const PIP* pip : rt_net->routing_pips())
			{
				Point pip_pos(pip->position());
				Tile* t_inst = _arch->get_inst_by_pos(pip_pos);
				if (!t_inst) continue;

				t_inst->add_used_gsb_ports_num();

				string from(pip->from());
				string to(pip->to());
				auto& f_ports = t_inst->type_ptr()->get_ports_by_GRM_net(from);
				auto& t_ports = t_inst->type_ptr()->get_ports_by_GRM_net(to);

				if (f_ports.size() >= 1 && t_ports.size() >= 1)
				{	// draw a switch line if it exists
					TilePorts grms = { f_ports[0], t_ports[0] };
					route_net->add_lines(umbrella_lines(grms, t_inst, RouteLine::Switch));
				}

				// add umbrella lines
				route_net->add_lines(umbrella_lines(f_ports, t_inst));
				route_net->add_lines(umbrella_lines(t_ports, t_inst));

				// stretch ports
				if (draw_from_port)
					for (auto port : f_ports)
						route_net->add_lines(tree_lines(port, pip_pos));

				for (auto port : t_ports)
					route_net->add_lines(tree_lines(port, pip_pos));
#ifndef DRAW_BOTH
				draw_from_port = false;
#endif
			} // end pip circle

			if (!route_net->is_empty())
			{	// add a route net
				_parent->add_net(route_net);
				//route_net->set_name(net->name());
				_net_map.insert({ net->name(), route_net });

				// add source sink info
				Property<Point>& POS = create_property<Point>(INSTANCE, "position");
				Property<double>& FALL_DELAY = create_property<double>(PIN, "fall_delay");
				Property<double>& RISE_DELAY = create_property<double>(PIN, "rise_delay");
				for (const Pin* pin : net->pins())
				{
					if (pin->is_mpin()) continue;
					const Instance* inst = pin->instance();
					if (const Point* pos = inst->property_ptr<Point>(POS))
					{
						if (pin->is_source()) {
							route_net->add_source(STAPin{ pin->name(), *pos });
							add_cluster_net_info(route_net, *pos);
						}
						else if (pin->is_sink()) {
							double fall_delay = pin->property_value(FALL_DELAY);
							double rise_delay = pin->property_value(RISE_DELAY);
							route_net->add_sink(STAPin{ pin->name(), *pos, rise_delay, fall_delay });
							add_cluster_net_info(route_net, *pos);
						}
					}
				}
			}
			// NEW! use net model to store net information
			_parent->addNetModel(net->name());
		} // end route net circle

		//calculate route resource usage and display it.
		Point _scale = _arch->scale();
		int x, y;
		Tile* _tile;
		for (x = 0; x < _scale.x; ++x)
		{
			for (y = 0; y < _scale.y; ++y)
			{
				_tile = _arch->get_inst_by_pos({ x,y });
				_tile->calculate_gsb_usage();
			}
		}

		int i = 0;
		for (auto [name, net] : _net_map)
			net->set_index(i++);

		_route_drawed = true;
		_route_added = true;
	}

	void UserDesign::show_route(bool show)
	{
		for (auto [name, net] : _net_map)
			net->setVisible(show);

		_route_drawed = show;
	}

	Lines UserDesign::umbrella_lines(const TilePorts& ports, const Tile* owner, RouteLine::type_t type)
	{
		Lines items;
		if (ports.size() < 2) return items;
		auto source = ports[0];
		for (auto ip = ports.begin() + 1; ip != ports.end(); ++ip)
			merge_items(items, draw_inner_line(source, *ip, owner, type));

		return items;
	}

	Lines UserDesign::tree_lines(const TilePort* root, const Point& pos)
	{
		Lines items;
		Tile* p_inst = _arch->get_inst_by_pos(pos);
		if (!p_inst) return items;

		// get neighbor port
		QString side;
		FDU::Point n_pos(pos); // n is short for neighbor
		switch (root->side)
		{
		case ARCH::LEFT:
			side = "right";
			--n_pos.y;
			break;
		case ARCH::RIGHT:
			side = "left";
			++n_pos.y;
			break;
		case ARCH::TOP:
			side = "bottom";
			--n_pos.x;
			break;
		case ARCH::BOTTOM:
			side = "top";
			++n_pos.x;
			break;
		default:
			ASSERT(0, "An error occurred during get neighbor port.");
			break;
		}

		if (n_pos.x >= _arch->scale().x ||
			n_pos.y >= _arch->scale().y ||
			n_pos.x < 0 ||
			n_pos.y < 0)
		{	// check n_pos legality
			return items;
		}

		Tile* p_n_inst = _arch->get_inst_by_pos(n_pos);
		if (!p_n_inst) return items;
		auto p_n_type = p_n_inst->type_ptr();

		/*
		 * such as left1 <=> right1
		 * ... fixed writing "side + QString::number(root->index)"
		 * needs updating when the name rule changed
		 */
		string n_port_name((side + QString::number(root->index)).toStdString());
		auto n_port = p_n_type->get_port_ptr(n_port_name);
		if (!n_port) return items;

		// line between neighbor tiles
		items.push_back({ RouteLine::Fixed,
			root->pos(TileInfo::PORT_SPACE, p_inst->rect(), p_inst->scenePos()),
			n_port->pos(TileInfo::PORT_SPACE, p_n_inst->rect(), p_n_inst->scenePos())
			});

		// stretch port
		auto& n_ports(p_n_type->get_ports_by_port(n_port_name));
		if (n_ports.empty()) return items;

		TilePorts n_all_ports;
		n_all_ports.push_back(n_port);
		for (auto port : n_ports) n_all_ports.push_back(port);
		merge_items(items, umbrella_lines(n_all_ports, p_n_inst));

		for (auto p_n_port : n_ports)	// stretching each port
			merge_items(items, tree_lines(p_n_port, p_n_inst->pos()));

		return items;
	}

	Lines UserDesign::draw_inner_line(const TilePort* p1, const TilePort* p2, const Tile* p_inst, RouteLine::type_t type)
	{
		Lines lines;
		if (!p_inst) return lines;

		QPointF p1_pos(p1->pos(TileInfo::PORT_SPACE, p_inst->rect(), p_inst->scenePos()));
		QPointF p2_pos(p2->pos(TileInfo::PORT_SPACE, p_inst->rect(), p_inst->scenePos()));

		if (p1->side != p2->side)
		{	// same side
			lines.push_back({ type, p1_pos, p2_pos });
			return lines;
		}

		double x, y;
		switch (p1->side)
		{	// different side
		case ARCH::LEFT:
			x = p_inst->scenePos().x() + (p_inst->type_ptr()->rect().width() / 3);
			y = (p1_pos.y() + p2_pos.y()) / 2;
			break;
		case ARCH::RIGHT:
			x = p_inst->scenePos().x() + 2 * (p_inst->type_ptr()->rect().width() / 3);
			y = (p1_pos.y() + p2_pos.y()) / 2;
			break;
		case ARCH::TOP:
			y = p_inst->scenePos().y() + (p_inst->type_ptr()->rect().height() / 3);
			x = (p1_pos.x() + p2_pos.x()) / 2;
			break;
		case ARCH::BOTTOM:
			y = p_inst->scenePos().y() + 2 * (p_inst->type_ptr()->rect().height() / 3);
			x = (p1_pos.x() + p2_pos.x()) / 2;
			break;
		default:
			ASSERT(0, "Unknown port dir while drawing inner lines.");
			break;
		}

		lines.push_back({ type, p1_pos, QPointF(x, y) });
		lines.push_back({ type, p2_pos, QPointF(x, y) });
		return lines;
	}

	void UserDesign::add_cluster_net_info(Route* route, Point pos)
	{	// gather cluster-net related info
		if (Cluster * cluster = _arch->get_cluster_by_pos(pos))
			cluster->add_route(route);
	}

	void UserDesign::save_placement(QString save_file_path)
	{
		if (!_design_is_modified) return;
		plc_map reverse; // now => origin 2 origin => now
		for (auto [to, from] : _plc_map)
			reverse.insert({ from, to });

		// design backup
		QFile file_finder;
		QString save_file_name(save_file_path);
		int file_idx = 0;
		while (file_finder.exists(save_file_name))
		{	// a.xml => a.xml[i].bak
			save_file_name = save_file_path + '[' + QString::number(++file_idx) + ']' + ".bak";
		}
		_design->save("xml", save_file_name.toStdString());

		// write placement modification
		Property<Point>& POS = create_property<Point>(INSTANCE, "position");
		for (Instance* inst : _design->top_module()->instances())
		{
			Point p = inst->property_value(POS); //get used position
			if (p == Point()) continue;

			auto it = reverse.find(p);
			if (it != reverse.end())
			{	// set new place
				inst->set_property(POS, it->second);
			}
		}

		for (Net* net : _design->top_module()->nets())
		{	// delete route info
			COSRTNet* rt_net = static_cast<COSRTNet*>(net);
			rt_net->clear_pips();
		}

		_design->save("xml", save_file_path.toStdString());
		_design_is_modified = false;
	}

	void UserDesign::save_rt_placement(QString save_file_path)
	{
		//if (!design_modified()) return;

		QFile file_finder;

		COS::Library* template_work_lib = _design->find_library("template_work_lib");
		ASSERT(template_work_lib, "save_rt_placement: do not support ols style design");

		COS::Module* iob = template_work_lib->find_module("iob");
		ASSERT(iob, "save_rt_placement: can not find iob");

		COS::Library* work_lib = _design->top_module()->library();

		if (file_finder.exists(save_file_path)) {
			auto back_file_name = save_file_path + ".bak";
			_design->save("xml", back_file_name.toStdString());
		}

		// write net addition
		int i = 0;
		for (auto [from, to] : _net_chg_map)
		{
			Net* net = _design->top_module()->find_net(from);
			if (!net) continue;

			QString iob_name = "";
			QString iob_inst_name = "";
			do {
				iob_inst_name = QString("signal_iob[") + QString::number(i) + ']';
				iob_name = QString("signal_iob__") + QString::number(i++) + QString("__");
			} while (work_lib->find_module(iob_name.toStdString()));// ensure no repeated name

			COS::Module* signal_iob = work_lib->create_module(iob_name.toStdString(), "IOB");
			signal_iob = iob->clone(iob_name.toStdString());

			COS::Instance* iob_inst = _design->top_module()->create_instance(iob_inst_name.toStdString(), signal_iob);

			COS::Property<Point>& POS = create_property<Point>(INSTANCE, "position");
			COS::Config& DRIVEATTRBOX = create_config("IOB", "DRIVEATTRBOX", "12");
			COS::Config& IOATTRBOX = create_config("IOB", "IOATTRBOX", "LVTTL");
			COS::Config& O1INV = create_config("IOB", "O1INV", "O1");
			COS::Config& OMUX = create_config("IOB", "OMUX", "O1");
			COS::Config& SLEW = create_config("IOB", "SLEW", "SLOW");

			iob_inst->set_property(POS, to);
			iob_inst->set_property(DRIVEATTRBOX, "12");
			iob_inst->set_property(IOATTRBOX, "LVTTL");
			iob_inst->set_property(O1INV, "O1");
			iob_inst->set_property(OMUX, "O1");
			iob_inst->set_property(SLEW, "SLOW");
			//FDU_LOG(DEBUG) << "STEP1!";
			COS::Pin* iob_pin = iob_inst->find_pin("OUT");//"01"???
			if (!iob_pin) continue;
			//FDU_LOG(DEBUG) << "STEP2!";
			if (iob_pin->is_connected()) iob_pin = iob_inst->find_pin("O2");
			if (!iob_pin) continue;
			//FDU_LOG(DEBUG) << "STEP3!";
			iob_pin->connect(net);
			//FDU_LOG(DEBUG) << "STEP4!";
			COS::Port* iob_port = _design->top_module()->create_port(iob_inst_name.toStdString(), COS::DirType::OUTPUT);
			COS::Net* iob_net = _design->top_module()->create_net(iob_inst_name.toStdString());
			iob_pin = iob_inst->find_pin("PAD");
			iob_pin->connect(iob_net);
			//FDU_LOG(DEBUG) << "STEP5!";
		}

		_design->save("xml", save_file_path.replace("_rt.xml", ".xml").toStdString());
		_save_file_path = save_file_path;
	}

	QStringList UserDesign::creat_route_list() const
	{
		QStringList rt_list;
		for (const auto& [name, net] : _net_map)
			rt_list.push_back(QString::fromStdString(name));
		return rt_list;
	}

	void UserDesign::cluster_moved(Point from, Point to)
	{
		auto it = _plc_map.find(from);
		if (it != _plc_map.end())
		{	// (from, origin) => (to, origin)
			Point origin(it->second);
			_plc_map.erase(it);
			_plc_map.insert({ to, origin });
		}
		else
		{	// insert (to, from)
			_plc_map.insert({ to, from });
		}
		_design_is_modified = true;
	}

	void UserDesign::net_moved(const std::string& net, Point to)
	{
		_net_chg_map.insert({ net, to });
		_design_is_modified = true;
	}

	void UserDesign::deselect_all()
	{
		for (auto [name, net] : _net_map) {
			if (net->is_selected()) net->set_selected(false);
			if (net->is_highlighted()) net->set_highlighted(false);
			net->update();
		}
	}

}