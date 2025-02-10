#ifndef SRC_USER_DESIGN_H
#define SRC_USER_DESIGN_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <string>
#include <vector>
#include <map>
#include "map_utils.h"
#include "rtnetlist.hpp"
#include "GraphicItem.h"


namespace FPGAViewer {

	using std::size_t;
	using std::string;
	using std::map;
	using std::vector;
	using FDU::Point;
	using namespace COS;
	using namespace GraphicItemLib;

	class ViewWidget;
	class Arch;
	class TilePort;
	using TilePorts = vector<const TilePort*>;;

	class UserDesign
	{
	public:
		typedef map<Point, Point>	plc_map;
		typedef map<string, Route*>	net_map;
		typedef map<string, Point>	net_chg_map;

		UserDesign(ViewWidget* parent, Arch* arch)
			: _parent(parent), _arch(arch)
		{}

		const QString& file_path() const { return _design_file_path; }
		const QString& save_path() const { return _save_file_path; }
		const bool is_modified()   const { return _design_is_modified; }

		void load(const string& file_name);
		void add_route();
		void clear_route();
		void clear_modify();
		void clear();

		void show_placement();
		void hide_placement();
		void show_route(bool show);
		void deselect_all();
		void cluster_moved(Point from, Point to);
		void net_moved(const std::string& net, Point to);

		void save_placement(QString save_file_path);
		void save_rt_placement(QString save_file_path);

		Route* get_route_by_name(const string& name) { return find_ptr(_net_map, name); }
		QStringList	creat_route_list() const;

	private:
		Lines	draw_inner_line(const TilePort* p1, const TilePort* p2, const Tile* p_inst, RouteLine::type_t type = RouteLine::Fixed); //draw lines inside tile
		Lines	umbrella_lines(const TilePorts& ports, const Tile* owner, RouteLine::type_t type = RouteLine::Fixed);
		Lines	tree_lines(const TilePort* root, const Point& pos);
		void	add_cluster_net_info(Route* route, Point pos);

		ViewWidget* _parent;
		Arch* _arch;
		COS::Design* _design = nullptr;

		net_map		_net_map;
		plc_map		_plc_map;
		net_chg_map	_net_chg_map;

		bool		_placement_drawed = false;
		bool		_route_drawed = false;
		bool		_route_added = false;
		bool		_design_is_modified = false;
		QString		_design_file_path;
		QString		_save_file_path;

	};

}

#endif
