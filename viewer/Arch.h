#ifndef SRC_DATA_BASE_H
#define SRC_DATA_BASE_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <string>
#include <memory>
#include <map>
#include <stdexcept>
#include "arch/archlib.hpp"
#include "map_utils.h"


namespace FPGAViewer {
	namespace GraphicItemLib {
		class Tile;
		class Cluster;
	}
	using std::size_t;
	using std::string;
	using std::map;
	using std::vector;
	using FDU::Point;
	using namespace ARCH;
	using namespace COS;
	using GraphicItemLib::Tile;
	using GraphicItemLib::Cluster;

	class TileInfo;

	//////////////////////////////////////////////////////////////////////////
	// TilePort
	struct TilePort
	{
		const TileInfo* owner = nullptr;
		int             index = -1;
		SideType        side = ARCH::SIDE_IGNORE;

		QPointF  pos(double space, const QRectF& rect, const QPointF& base) const;
		SideType oppsite_side() const;
	};

	using TilePorts = vector<const TilePort*>;

	//////////////////////////////////////////////////////////////////////////
	// TileInfo
	class TileInfo : public boost::noncopyable
	{
		using port_map  = map<string, TilePort>;
		using ports_map = map<string, TilePorts>;

	public:
		static const double	PORT_SPACE;
		static const size_t	MIN_LEN;
		static const size_t	OFFSET;

		TileInfo(const ArchCell* tile);

		const size_t  slice_num()	const { return _slice_num;    }
		const string& type()		const { return _tile->name(); }
		const string& GRM_type()	const { return _grm_type;     }
		const string& slice_type()	const { return _slice_type;   }
		QRectF rect()				const { return _rect;         }
		size_t num_left()			const { return _num_left;     }
		size_t num_right()			const { return _num_right;    }
		size_t num_top()			const { return _num_top;      }
		size_t num_bot()			const { return _num_bot;      }

		const TilePorts& get_ports_by_GRM_net(const string& grm_net_name) const { return const_cast<ports_map&>(_pos_map)[grm_net_name]; }
		const TilePorts& get_ports_by_port(const string& port_name)       const { return const_cast<ports_map&>(_port_map)[port_name]; }
		const TilePort*  get_port_ptr(const string& pin_name) const { auto ptr = _pin_map.find(pin_name);  return (ptr != _pin_map.end()) ? &ptr->second : nullptr; }

	private:
		void get_types();
		void create_pin_map();
		void create_ports_map(const Net* net);
		void calc_dimension();

		const ArchCell* _tile;

		size_t _num_left;
		size_t _num_right;
		size_t _num_top;
		size_t _num_bot;

		QRectF _rect;

		size_t _slice_num;
		string _grm_type;
		string _slice_type;

		port_map   _pin_map;   // port net name => tile port which contain more detail info
		ports_map  _port_map;  // port net name => ptr of ports which are connected with it
		ports_map  _pos_map;   // GRM net name  => ptr of ports which are connected with it
	};

	
	inline QPointF TilePort::pos(double space, const QRectF &rect, const QPointF& base) const
	{
		switch (side) {
			case ARCH::LEFT:   return QPointF(0.,            index * space) + base;
			case ARCH::BOTTOM: return QPointF(index * space, rect.height()) + base;
			case ARCH::RIGHT:  return QPointF(rect.width(),  index * space) + base;
			case ARCH::TOP:    return QPointF(index * space, 0.)            + base;
			default:           ASSERT(0, "Unknown port side while calculating position.");
		}
	}
	 
	inline SideType TilePort::oppsite_side() const
	{
		switch (side) {
			case ARCH::LEFT:   return ARCH::RIGHT;
			case ARCH::BOTTOM: return ARCH::TOP;
			case ARCH::RIGHT:  return ARCH::LEFT;
			case ARCH::TOP:    return ARCH::BOTTOM;
			default:           return ARCH::SIDE_IGNORE;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Arch
	class ViewWidget;
	class MainView;

	class Arch {
	public:
		const QString& file_path() const { return _file_path; }
		bool is_ready() const { return _is_ready; }
		Point scale() const { return _scale; }

		string find_pad_by_position(Point pos) const {
			return _device->find_pad_by_position(pos);
		}
		const TileInfo* get_tile_type(const Instance* tile_inst) {
			return find_ptr(_tile_map, tile_inst->down_module());
		}
		Tile* get_inst_by_pos(Point pos) { return find_ptr(_inst_map, pos); }
		Cluster* get_cluster_by_pos(Point pos) { return find_ptr(_cluster_map, pos); }

		void set_cluster_used(Point pos, bool used = true);
		void set_cluster_movable(Point pos, bool movable);
		void clear_placement();

	private:
		const ARCH::FPGADesign* _device = nullptr;
		Point _scale;
		QString	_file_path;
		bool	_is_ready = false;

		map<const Module*, TileInfo> _tile_map;
		map<Point, Tile*>		_inst_map;
		map<Point, Cluster*>	_cluster_map;

		void load_file(const string& file_name);
		void clear();

		void get_tile_detail_info();
		void get_instance_layout_info();
		void draw_device(MainView* view);

		int get_capacity(Instance* inst);

//		friend void	ViewWidget::load_arch(const string&);
		friend class ViewWidget;
	};

}

#endif
