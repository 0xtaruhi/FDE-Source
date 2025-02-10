#include "Arch.h"
#include "MainView.h"
#include "GraphicItem.h"
#include <algorithm>		// max

namespace {
	const double TILE_SPACE_H = 25.;
	const double TILE_SPACE_V = 20.;
	const double SCENE_MARGIN = 30.;
}

namespace FPGAViewer {

	const double TileInfo::PORT_SPACE	= 0.35;
	const size_t TileInfo::MIN_LEN		= 10;
	const size_t TileInfo::OFFSET		= 2;

	TileInfo::TileInfo(const ArchCell* tile)
		: _tile(tile), _num_left(0), _num_right(0), _num_top(0), _num_bot(0), _slice_num(0)
	{
		get_types();
		create_pin_map();
		calc_dimension();

		for (const Net* net : _tile->nets())
			create_ports_map(net);
	}

	void TileInfo::get_types()
	{
		int grm_num = 0;

		for (const ArchInstance* inst : _tile->instances()) {
			if (inst->down_module()->type() == "SLICE") {
				++_slice_num;
				_slice_type = "SLICE";
			}
			else if (inst->down_module()->type() == "IOB") {
				++_slice_num;
				_slice_type = "IOB";
			}
			else if (inst->down_module()->type() == "GRM" || inst->down_module()->type() == "GSB") {
				++grm_num;
				_grm_type = inst->down_module()->name();
			}
		}
		//not support kind
		ASSERT(grm_num <= 1, "Too many GRMs / GSBs in one tile, not support now.");
	}

	void TileInfo::create_pin_map()
	{
		for (const Pin* pin : _tile->pins()) {
			auto a_port = static_cast<const ArchPort*>(pin->port());
			_pin_map[pin->name()] = { this, pin->index(), a_port->side() };
		}
	}

	void TileInfo::create_ports_map(const Net* net)
	{
		const string& net_name = net->name();
		vector<const Pin*> mpins, GRM_pins;

		for (const Pin* pin : net->pins()) {
			if (pin->is_mpin()) {
				// this net has pin connected to tile outside port
				mpins.push_back(pin);
			}
			else {
				auto inst = static_cast<const ArchInstance*>(pin->instance());
				ASSERTS(inst);
				string inst_type = inst->down_module()->type();
				if (inst_type == "GRM" || inst_type == "GSB") {
					// this net has connection to GRM port
					GRM_pins.push_back(pin);
				}
			}
		} // end pin analysis

		if (mpins.empty()) return;

		ASSERT(GRM_pins.size() <= 1, "mutiple grm pins: " + _tile->name() + "/" + net_name);

		if (!GRM_pins.empty()) {
			ASSERTS(!_grm_type.empty());
			// GRM net => tile ports
			// for each GRM net "a", create map : 
			//   "a" => tile ports connected with it
			for (auto mpin : mpins) {
				auto tp = get_port_ptr(mpin->name());
				ASSERTS(tp);
				_pos_map[net_name].push_back(tp);
			}
		}

		if (mpins.size() >= 2) {
			/*
			 * for each tile port "a", create map :
			 * "a" => tile ports connected with it
			 * suppose the size is n, the number of
			 * map will be "n * (n - 1) / 2"
			 */
			for (int i = 0, n = mpins.size(); i < n; ++i)
				for (int j = i + 1; j < n; ++j) 
				{
					auto& pname1 = mpins[i]->name();
					auto& pname2 = mpins[j]->name();
					auto tp1 = get_port_ptr(pname1);
					auto tp2 = get_port_ptr(pname2);
					ASSERTS(tp1 && tp2);
					_port_map[pname1].push_back(tp2);
					_port_map[pname2].push_back(tp1);
				}
		} // end map: tile port name => tile port name
	}

	void TileInfo::calc_dimension()
	{
		for (const ArchPort* port : _tile->ports()) {	// get ports number
			ASSERT(port->is_vector(), "Not vector port description! Please load the latest ARCH file!");
			switch (port->side()) {	// get vector size & dir
				case LEFT:   _num_left  = port->width(); break;
				case BOTTOM: _num_bot   = port->width(); break;
				case RIGHT:  _num_right = port->width(); break;
				case TOP:    _num_top   = port->width(); break;
				default:     break;
			}
		}
		auto width = std::max({ _num_top, _num_bot, MIN_LEN });
		auto height = std::max({ _num_left, _num_right, MIN_LEN });
		_rect = QRectF(0., 0., 
			(width + 2 * OFFSET) * PORT_SPACE, 
			(height + 2 * OFFSET) * PORT_SPACE);
	}

	int Arch::get_capacity(Instance* inst)
	{
		int num;
		Module* _tile = inst->down_module();
		Module* _sub_module;
		for (const Instance* _sub_inst : _tile->instances())
		{
			_sub_module = _sub_inst->down_module();
			if (_sub_module->type() == "GSB" || _sub_module->type() == "GRM")
			{
				num = _sub_module->num_ports();
				break;
			}
		}
		return num;
	}

	void Arch::load_file(const string& file_name)
	{
		_file_path = QString::fromStdString(file_name);
		_device = FPGADesign::loadArchLib(file_name);
	}

	void Arch::clear()
	{
		//ASSERT(!design, "design need to be released");
		_is_ready = false;
		//free_design();

		for (auto [k, v] : _cluster_map) delete v;
		_cluster_map.clear();
		for (auto [k, v] : _inst_map) delete v;
		_inst_map.clear();
		_tile_map.clear();
	}

	void Arch::get_tile_detail_info()
	{
		const Library* tile_lib = _device->find_library("tile");
		ASSERT(tile_lib, "get_tile_detail_info: cannot find the 'tile' lib in ARCH file.");

		for (const Module* m : tile_lib->modules())
			_tile_map.try_emplace(m, static_cast<const ArchCell*>(m));
	}

	void Arch::get_instance_layout_info()
	{
		int num;
		_scale = _device->scale();
		for (Instance* inst : _device->top_module()->instances())
		{	// get tile type
			auto p_type = get_tile_type(inst);
			ASSERTS(p_type);
			// add tile
			Point logic_pos = static_cast<ArchInstance*>(inst)->logic_pos();
			Tile* p_inst = new Tile(logic_pos, p_type, inst->name());
			_inst_map.insert({ logic_pos, p_inst });
			num = get_capacity(inst);

			for (int i = 0; i < p_inst->type_ptr()->slice_num(); ++i)
			{	// add cluster as its children item
				Point cluster_pos(logic_pos.x, logic_pos.y, i);
				Cluster* cluster_inst =
					new Cluster(cluster_pos, p_type->slice_type(), _device->find_pad_by_position(cluster_pos));

				// Cluster *clu_ptr = cluster_inst.get();
				// bool ok = connect(clu_ptr, SIGNAL(cluster_moved(Point, Point)), this, SLOT(cluster_moved(Point, Point)));
				_cluster_map.insert({ cluster_pos, cluster_inst });
				cluster_inst->setPos(Cluster::OFFSET + QPointF(i * (Cluster::RECT.width() + Cluster::SPACE), 0.));
				cluster_inst->setParentItem(p_inst);
			}
			p_inst->set_gsb_ports_num(num);
		}
	}

	void Arch::draw_device(MainView* view)
	{
		QGraphicsScene* scene = view->scene();

		vector<double> size_v(_scale.x, 0.);
		vector<double> size_h(_scale.y, 0.);

		for (int x = 0; x < _scale.x; ++x)
			for (int y = 0; y < _scale.y; ++y)
			{	// update size_v & size_h info
				if (Tile * p_inst = get_inst_by_pos({ x,y })) {
					QRectF rect = p_inst->type_ptr()->rect();
					if (rect.width() > size_h[y]) size_h[y] = rect.width();
					if (rect.height() > size_v[x]) size_v[x] = rect.height();
				}
			}

		double scene_x = 0., scene_y = 0.;
		for (int x = 0; x < _scale.x; ++x) {
			scene_x = 0.;
			for (int y = 0; y < _scale.y; ++y) {
				if (Tile * p_inst = get_inst_by_pos({ x,y })) {
					// update tile dimension
					p_inst->set_rect(QRectF(0., 0., size_h[y], size_v[x]));
					//add item
					p_inst->setPos(scene_x, scene_y);
					// _scene->addItem(p_inst.get());
					// element_type * temp_type = p_inst.get();
					scene->addItem(p_inst);
				}
				scene_x += size_h[y] + TILE_SPACE_H;
			}
			scene_y += size_v[x] + TILE_SPACE_V;
		}

		//update scene rect
		view->setSceneRect(QRectF(-SCENE_MARGIN, -SCENE_MARGIN,
			scene_x - TILE_SPACE_H + 2 * SCENE_MARGIN,
			scene_y - TILE_SPACE_V + 2 * SCENE_MARGIN));

		view->showMaximized();
		view->init_scale();

		_is_ready = true;
	}

	void Arch::clear_placement()
	{
		for (auto [pos, tile] : _inst_map)
			tile->set_used(false);

		for (auto [pos, cluster] : _cluster_map) {
			cluster->set_used(false);
			cluster->set_movable(false);
		}
	}

	void Arch::set_cluster_used(Point pos, bool used)
	{
		if (Cluster * cluster = get_cluster_by_pos(pos))
			cluster->set_used(used);
	}

	void Arch::set_cluster_movable(Point pos, bool movable)
	{
		if (Cluster * cluster = get_cluster_by_pos(pos))
			cluster->set_movable(movable);
	}

}