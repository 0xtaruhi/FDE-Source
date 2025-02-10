#ifndef SRC_GRAPHIC_ITEM_H
#define SRC_GRAPHIC_ITEM_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <array>
#include <string>
#include <map>

#include "arch/archlib.hpp"

namespace FPGAViewer {
	class	TileInfo;

namespace GraphicItemLib {

	using FDU::Point;

	using namespace ARCH;
	using namespace COS;

	class	Route;
	class	Cluster;
	class	Tile;

	class	Primitive;
	class	GRM;
	class	SliceBlock;
	class	PrimitivePin;
	class	PrimitiveNet;

	struct STAPin
	{
		string name = "";
		Point  pos = Point{};
		double rise_delay = 0.;
		double fall_delay = 0.;
	};

	typedef vector<STAPin>   Pins;

	const int TileType = 1;
	const int ClusterType = 2;
	const int NetType = 3;
	
	class GFactory // graphics factory
	{	// for future extension
	public:
		static Primitive*	makePrimitive(Instance* instance);
		static PrimitiveNet*	makePrimitiveNet(Net* net);
	};

	struct Colors : std::array<QColor, 4> {
		enum { basic, selected, highlighted, used };
		Colors(QColor cb, QColor cs, QColor ch, QColor cu = Qt::black)
			: std::array<QColor, 4>{ cb, cs, ch, cu }
		{}
	};

	struct RouteLine
	{
		enum type_t { Switch, Fixed } type;
		QPointF	p1,	p2;
	};

	using Lines = std::vector<RouteLine>;

	class Route : public QObject, public QGraphicsItem
	{
		Q_OBJECT

	public:
		Route(const string& name) : _name(name)
		{
			//setFlags(ItemIsSelectable);
			setZValue(1.);
			_btl = QPointF(99999, 99999);
			_bbr = QPointF(0, 0);
		}

		void set_selected(bool selected);
		void set_highlighted(bool highlighted)	{ _highlighted = highlighted; }
		void add_source(STAPin pin)				{ _sources.push_back(pin); }
		void add_sink(STAPin pin)				{ _sinks.push_back(pin); }
		void add_lines(const Lines &lines);
//		void set_name(const std::string& name)	{ _name = name; }
		void set_index(int index)				{ _index = index; }

		const string&	name()		{ return _name; }
		bool	is_highlighted()	{ return _highlighted; }
		bool	is_selected()		{ return _selected; }
		bool	is_empty()			{ return _lines.empty(); }
		const Pins&	get_sources()	{ return _sources; }
		const Pins&	get_sinks()		{ return _sinks; }

		QString STA_info() const;

		QRectF boundingRect() const;

		static bool SHOW_TIME_INFO;
		static bool HIDE_PLACE_INFO;
		static bool HIDE_UNSELECTED_LINE;
		static bool HIDE_UNHIGHLIGHTED_LINE;

		static Route* NetFrom;
		static bool NetMoveStarted;
		
	signals:
		void set_current(int);
		void show_sta_info(QString);
		void show_relative_cluster(const Pins&, const Pins&, bool);

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget /* = 0 */);
		void paint_line(QPainter *painter, const QStyleOptionGraphicsItem *option,
			const RouteLine& line, double pen_width = 0.);

		void mousePressEvent(QGraphicsSceneMouseEvent* event);

	private:
		string		_name;
		bool		_highlighted = false;
		bool		_selected    = false;

		Pins		_sources;
		Pins		_sinks;
		Lines		_lines;
		int			_index = -1;

		// bounding rect
		QPointF		_btl;	// top left
		QPointF		_bbr;	// bottom right

		bool		_relative_cluster_shown = false;
	};

	//////////////////////////////////////////////////////////////////////////
	// cluster
	class Cluster : public QObject, public QGraphicsItem
	{
		Q_OBJECT

		typedef std::map<string, Route*> route_map;

	public:
		Cluster(const Point &pos, const string &type, const string &pad,
				bool used = false, bool movable = false, 
				bool highlighted = false, bool selected = false,
				bool relative_routes_shown = false)
			: _pos(pos), _type(type), _pad(pad),
			  _used(used), _movable(movable), 
			  _highlighted(highlighted), _selected(selected),
			  _relative_routes_shown(relative_routes_shown)
		{
			setFlags(ItemIsSelectable);
			setZValue(9999);
			setAcceptDrops(true);
		}

		string cluster_type()            const { return _type; }
		string pad()					 const { return _pad; }
		bool same_type(Cluster *other)   const { return cluster_type() == other->cluster_type(); }
		int  index()                     const { return _pos.z; }
		Point logic_pos()                const { return _pos; }
		bool is_movable()                const { return _movable; }
		bool is_used()                   const { return _used; }

		void set_used(bool used)				{ _used = used; }
		void set_movable(bool movable)			{ _movable = movable; }
		void set_selected(bool selected)		{ _selected = selected; }
		void set_highlighted(bool highlighted)	{ _highlighted = highlighted; }

		void add_route(Route* route) { _routes.insert({ route->name(), route }); }

		QRectF boundingRect() const;

		static const QRectF  RECT;
		static const QPointF OFFSET;
		static const double  SPACE;

		static Cluster *MoveFrom;
		static bool  MoveStarted;

		/*
		 * place info stored in map container, one for now place
		 * other for original place
		 * set container is put in view widget
		 */

	signals:
		void show_relative_route(route_map, bool);

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget /* = 0 */);

		// for placement modification
		void mousePressEvent(QGraphicsSceneMouseEvent *event);
		//void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

	private:
		void mypaint(QPainter *painter, const QStyleOptionGraphicsItem *option,
			QColor basic_color, QColor selected_color, 
			QColor used_color, QColor highlighted_color,
			double pen_width = 0.); // color define

		Point		_pos;
		string		_type;
		string		_pad;
		bool		_used;
		bool		_movable;
		bool		_selected;
		bool		_highlighted;
		route_map	_routes;   
		bool		_relative_routes_shown;
	};

	class Tile : public QGraphicsItem
	{
	public:
		enum { Type = UserType + TileType };

		Tile(const Point& pos, const TileInfo* type, const string& name);

		const Point& pos()    const { return _pos; }
		const TileInfo* type_ptr()  const { return _type; }
		string type_name()    const; // { return _type->type(); }
		const string& name()  const { return _name; }
		QRectF rect()         const { return _rect; }
		bool is_used()        const { return _used; }

		void set_used(bool used) { _used = used; }
		void set_rect(const QRectF &rect) { _rect = rect; }

		void set_gsb_ports_num(int num) { _gsb_ports_num = num; }
		void add_used_gsb_ports_num() { _used_gsb_ports_num++; }
		void calculate_gsb_usage()
		{
			if (_used_gsb_ports_num > 0)	
				_gsb_usage = (double)(_used_gsb_ports_num) / (double)(_gsb_ports_num);
		}

		int get_gsb_ports_num() const { return _gsb_ports_num; }
		int get_used_gsb_ports_num() const { return _used_gsb_ports_num; }

		int type()			const { return Type; }

	protected:
		QRectF boundingRect() const { return _rect; }
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	private:
		void mypaint(QPainter *painter, const QStyleOptionGraphicsItem *option,
			QColor basic_color, QColor selected_color, 
			QColor used_color, QColor highlighted_color, 
			double pen_width = 0.); 
		void paintPorts(QPainter *painter);
		void paintTexts(QPainter *painter);

		Point		_pos;
		const TileInfo*	_type;
		string		_name;
		QRectF		_rect;
		bool		_used;
		int			_gsb_ports_num = 0;
		int			_used_gsb_ports_num = 0;
		double		_gsb_usage = -1.0;
	};
///////////////////////////////////////////////////////////////////////////////
// Primitive
	class Primitive : public QGraphicsItem
	{
	public:
		enum { Type = UserType + 1 };

		static const qreal	PIN_SPACE;
		static const int	PIN_MARGIN_IDX;
		static const qreal	MIN_WIDTH;
		static const int	LEVEL_UNSET;

		Primitive(Instance* primitive);

		~Primitive();

		void	addPin(PrimitivePin* ppin);
		void	setLevel(int level)		{ _level = level; }
		PrimitivePin*	findPin(Pin* pin);

		QRectF	boundingRect() const	{ return _rectf; }
		int		type() const			{ return Type; }
		Instance*	primitive()	const	{ return _primitive; }
		int		level() const			{ return _level; }

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

		QRectF			_rectf;
		Instance*		_primitive;
		unsigned int	_leftCnt;
		unsigned int	_rightCnt;

		int				_level;
		std::map<Pin*, PrimitivePin*>	_pinMap;
	};

///////////////////////////////////////////////////////////////////////////////
// GRM
	class GRM : public Primitive
	{
	public:
		enum { Type = UserType + 2 };

		GRM(Instance* primitive);

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
	};


///////////////////////////////////////////////////////////////////////////////
// SliceBlock (IOB or Slice)
	class SliceBlock : public Primitive
	{
	public:
		enum { Type = UserType + 3 };

		SliceBlock(Instance* primitive);

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
	};

///////////////////////////////////////////////////////////////////////////////
// ModulePin
	class ModulePin : public QGraphicsItem
	{
	public:
		enum Dir { INPUT, OUTPUT, IO };
		enum { Type = UserType + 11 };

		ModulePin(Pin* pin);

		void	setLevel(int level)		{ _level = level; }

		QRectF	boundingRect() const	{ return _rectf; }
		PrimitivePin*	pin() const		{ return _pin; }
		int		type() const			{ return Type; }
		int		level() const			{ return _level; }
		Dir		dir() const				{ return _dir; }

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	private:
		PrimitivePin*		_pin;
		QRectF				_rectf;
		Dir					_dir;
		int					_level;
		QGraphicsTextItem*	_textItem;
	};

///////////////////////////////////////////////////////////////////////////////
// PrimitivePin
	class PrimitivePin : public QGraphicsItem
	{
	public:
		enum Side { IGNORESIDE = 0, LEFT, BOTTOM, RIGHT, TOP };
		PrimitivePin(Pin* pin, Side side = IGNORESIDE, int sideIdx = 0);
		enum { Type = UserType + 9 };

		QRectF	boundingRect() const	{ return _rectf; }

		void	setSide(Side side)		{ _side = side; updateBoundingRect(); }
		void	setSideIdx(int sideIdx, qreal width = 0, qreal height = 0);
		Pin*	pin() const				{ return _pin; }
		Side	side() const			{ return _side; }
		int		type() const			{ return Type; }
		int		sideIndex() const		{ return _sideIdx; }

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
		void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
		void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

	private:
		void updateBoundingRect();

	private:
		Pin*				_pin;
		QRectF				_rectf;
		Side				_side;
		int					_sideIdx;
		bool				_hovered;
		QGraphicsTextItem*	_textItem;
	};

///////////////////////////////////////////////////////////////////////////////
// PrimitiveNet
	class PrimitiveNet : public QGraphicsItem
	{
	public:
		enum { Type = UserType + 10 };

		PrimitiveNet(Net* net);

		void	addTerminal(PrimitivePin* ppin);
		void	addCorner(QPoint p);
		void	clearCorner()			{ _corner.clear(); }

		vector<PrimitivePin *>	pins()	{ return _connectedPpins; }
		QRectF	boundingRect() const	{ return _rectf; }
		Net*	net()					{ return _net; }
		int		type() const			{ return Type; }

		void	updateShape();

	protected:
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
		QPainterPath shape () const		{ return _path; }
		//void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
		//void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

	private:
		void	updateBoundingRect();

	private:
		Net*					_net;
		QRectF					_rectf;
		vector<PrimitivePin *>	_connectedPpins;
		vector<QPoint>			_corner;
		QPainterPath			_path;
	};

///////////////////////////////////////////////////////////////////////////////
// Utilities
	inline bool isModulePinConnected(Pin* pin)
	{
		for (Pin* relativePin: pin->net()->pins())
			if (relativePin->is_mpin()) return true;
		return false;
	}
}}

#endif //GRAPHIC_ITEM_H