#include "GraphicItem.h"
#include "Arch.h"
#include <cmath>
#include <stdexcept>
//#define  DONT_COMPARE_LINE
#define USE_RENDER
#define CHANGE_PEN_WIDTH

namespace FPGAViewer { namespace GraphicItemLib {

	const QRectF  Cluster::RECT   = QRectF(0., 0., 8, 8);
	const QPointF Cluster::OFFSET = QPointF(3, 3);
	bool Route::SHOW_TIME_INFO       = false;
	bool Route::HIDE_PLACE_INFO      = false;
	bool Route::HIDE_UNSELECTED_LINE = false;
	bool Route::HIDE_UNHIGHLIGHTED_LINE = false;
	const double Cluster::SPACE      = 3;

	Cluster* Cluster::MoveFrom = 0;
	bool  Cluster::MoveStarted = false;

	Route* Route::NetFrom = 0;
	bool Route::NetMoveStarted = false;

	const qreal Primitive::PIN_SPACE = 2;
	const int Primitive::PIN_MARGIN_IDX = 2;
	const qreal Primitive::MIN_WIDTH = 80;
	const int Primitive::LEVEL_UNSET = 65536;

///////////////////////////////////////////////////////////////////////////////
// GFactory
	Primitive* GFactory::makePrimitive(Instance *instance)
	{
		const string type = instance->down_module()->type();
		if (type == "GRM") {
			return new GRM(instance);
		} else if (type == "SLICE" || type == "IOB") {
			return new SliceBlock(instance);
		} else {
			return new Primitive(instance);
		}
	}

	PrimitiveNet* GFactory::makePrimitiveNet(Net* net) 
	{
		return 0;
		//return new PrimitiveNet(net);
	}

///////////////////////////////////////////////////////////////////////////////
// Tile
	Tile::Tile(const Point& pos, const TileInfo* type, const string& name)
		: _pos(pos), _type(type), _name(name), _used(false), _rect(type->rect())
	{
		setFlags(ItemIsSelectable);
		setToolTip(QString::fromStdString(type_name()));
		setZValue(9999);
	}
	string Tile::type_name() const { return _type->type(); }

	void Tile::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget /* = 0 */)
	{
		if ("CENTER" == type_name())
			mypaint(painter, option, Qt::white, Qt::blue, Qt::magenta, Qt::darkRed);
		else
			mypaint(painter, option, Qt::green, Qt::blue, Qt::blue, Qt::cyan);
	}

	void Tile::mypaint( QPainter *painter, const QStyleOptionGraphicsItem *option, 
		QColor basic_color, QColor selected_color, 
		QColor used_color, QColor highlighted_color,
		double pen_width /*= 0.*/ )
	{
		int a;
		QPen pen(basic_color);
		painter->setBrush(Qt::NoBrush);

		const double zoom_level = option->levelOfDetailFromTransform(painter->worldTransform());

		if (is_used()) { 
			pen.setColor(used_color);
			//if (zoom_level < 0.3) painter->setBrush(QBrush(used_color));
#ifdef CHANGE_PEN_WIDTH
			pen_width += 1.0;
#endif
		}

		/*if (_gsb_usage >= 0.0)
		{
			a = (int)(10 * _gsb_usage);
			switch (a)
			{
			case 1:
				painter->setBrush(QBrush(QColor(102, 255, 0)));
				break;
			case 2:
				painter->setBrush(QBrush(QColor(153, 255, 0)));
				break;
			case 3:
				painter->setBrush(QBrush(QColor(204, 255, 0)));
				break;
			case 4:
				painter->setBrush(QBrush(QColor(255, 255, 0)));
				break;
			case 5:
				painter->setBrush(QBrush(QColor(255, 204, 0)));
				break;
			case 6:
				painter->setBrush(QBrush(QColor(255, 153, 0)));
				break;
			case 7:
				painter->setBrush(QBrush(QColor(255, 102, 0)));
				break;
			case 8:
				painter->setBrush(QBrush(QColor(255, 51, 0)));
				break;
			case 9:
			case 10:
				painter->setBrush(QBrush(QColor(255, 0, 0)));
				break;
			default:
				painter->setBrush(QBrush(QColor(51, 255, 0)));
				break;
			}			
		}*/

		if (_gsb_usage >= 0.0)
		{
			a = (int)(255 * _gsb_usage);
			painter->setBrush(QBrush(QColor(255, 0, 0, a)));
		}

		if (option->state & QStyle::State_Selected) {
			pen.setColor(selected_color);
#ifdef CHANGE_PEN_WIDTH
			if (zoom_level > 0.7) pen_width += 0.5;
#endif
		}

		if (zoom_level > 0.6 && !Route::HIDE_PLACE_INFO) {
			//paintPorts(painter);
			paintTexts(painter);
		}

		pen.setWidthF(pen_width);
#ifdef USE_RENDER
		painter->setRenderHint(QPainter::Antialiasing, true);
#endif
		painter->setPen(pen);
		painter->drawRect(_rect);
	}

	void Tile::paintPorts(QPainter* painter)
	{
		QPen pen(Qt::white);
		painter->save();
		painter->setPen(pen);
		qreal width = _rect.width();
		qreal height = _rect.height();

		size_t size = _type->num_left() + TileInfo::OFFSET;
		for (size_t idx = TileInfo::OFFSET; idx < size; ++idx)
			painter->drawLine(QPointF(-1., idx * TileInfo::PORT_SPACE), 
							QPointF(0., idx * TileInfo::PORT_SPACE));

		size = _type->num_right() + TileInfo::OFFSET;
		for (size_t idx = TileInfo::OFFSET; idx < size; ++idx)
			painter->drawLine(QPointF(width, idx * TileInfo::PORT_SPACE), 
							QPointF(width + 1., idx * TileInfo::PORT_SPACE));

		size = _type->num_top() + TileInfo::OFFSET;
		for (size_t idx = TileInfo::OFFSET; idx < size; ++idx)
			painter->drawLine(QPointF(idx * TileInfo::PORT_SPACE, height + 1.),
							QPointF(idx * TileInfo::PORT_SPACE, height));

		size = _type->num_bot() + TileInfo::OFFSET;
		for (size_t idx = TileInfo::OFFSET; idx < size; ++idx)
			painter->drawLine(QPointF(idx * TileInfo::PORT_SPACE, 0.),
							QPointF(idx * TileInfo::PORT_SPACE, -1.));

		painter->restore();
	}

	void Tile::paintTexts(QPainter* painter)
	{
		painter->save();

		QFont font("Consolas", 6);
		font.setStyleStrategy(QFont::ForceOutline);
		painter->setFont(font);

		painter->setPen(QPen(Qt::yellow));
		painter->drawText(1., rect().height() - 1.,
							QString("(%1, %2) %3")
							.arg(pos().x)
							.arg(pos().y)
							.arg(QString::fromStdString(type_name())));

		painter->restore();
	}
	

	void Cluster::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget /* = 0 */)
	{
		if ("SLICE" == _type)
			mypaint(painter, option, Qt::blue, Qt::cyan, Qt::yellow, Qt::red);
		else if ("IOB" == _type)
			mypaint(painter, option, Qt::yellow, Qt::cyan, Qt::magenta, Qt::green);
	}

	void Cluster::mypaint( QPainter *painter, const QStyleOptionGraphicsItem *option, 
		QColor basic_color, QColor selected_color, 
		QColor used_color, QColor highlighted_color,
		double pen_width /*= 0.*/ )
	{
		const double zoom_level = option->levelOfDetailFromTransform(painter->worldTransform());
		if (zoom_level < 0.2) return;

		QPen pen(basic_color);
		if (option->state & QStyle::State_Selected) {
			pen.setColor(selected_color);
			pen_width += 2;
			if (!_relative_routes_shown) {
				emit show_relative_route(_routes, true);
				_relative_routes_shown = true;
			}
		}
		else {
			_relative_routes_shown = false;
		}

		if (zoom_level > 0.6 && !Route::HIDE_PLACE_INFO)
		{	// draw text
			painter->save();
			QFont font("Consolas", 3);
			font.setStyleStrategy(QFont::ForceOutline);
			painter->setFont(font);
			painter->setPen(QPen(Qt::yellow));
			if (!_pad.empty()) {
				painter->drawText(1., boundingRect().height() * 1.6,
								QString("%1")
								.arg(QString::fromStdString(_pad)));
			}
			painter->restore();
		}

		painter->setBrush(Qt::NoBrush);
		if (_used) {
			painter->setBrush(QBrush(used_color));
			pen.setColor(selected_color); 
			if (_highlighted) {
				painter->setBrush(QBrush(highlighted_color));
				pen_width += 1.2;
			} 
		}

		pen.setWidthF(pen_width);	
		painter->setPen(pen);
#ifdef USE_RENDER
		painter->setRenderHint(QPainter::Antialiasing, true);
#endif
		painter->drawRect(RECT);
	}

	QRectF Cluster::boundingRect() const
	{
		return RECT;
	}

	void Cluster::mousePressEvent( QGraphicsSceneMouseEvent *event )
	{
		QGraphicsItem::mousePressEvent(event);
		if (event->button() == Qt::LeftButton && !MoveStarted && _movable && _used) {
			MoveFrom    = this;
			MoveStarted = true;
		}
	}
/*
 	void Cluster::mouseReleaseEvent( QGraphicsSceneMouseEvent *event )
 	{
 		QGraphicsItem::mouseReleaseEvent(event);
 		if (event->button() == Qt::LeftButton && MoveStarted && !_used) {
 			MoveToPos = _pos;
 			if (MoveToPos != MoveFromPos)
 				emit cluster_moved(MoveFromPos, MoveToPos);
 		}
 		else
 			MoveStarted = false;
 	}
 */
	void Route::set_selected(bool selected)
	{
		_selected = selected;
		if (_selected && _index != -1) {
			emit set_current(_index);
			emit show_sta_info(STA_info());
		}
		
		emit show_relative_cluster(_sources, _sinks, _selected);
		update();
	}
 
	void Route::add_lines( const Lines &lines )
	{
		for (const auto& line : lines) {
			_lines.push_back(line);

			const qreal x1 = line.p1.x();
			const qreal x2 = line.p2.x();
			const qreal y1 = line.p1.y();
			const qreal y2 = line.p2.y();
			qreal lx = qMin(x1, x2);
			qreal rx = qMax(x1, x2);
			qreal ty = qMin(y1, y2);
			qreal by = qMax(y1, y2);

			_btl = QPointF(qMin(lx, _btl.x()), qMin(ty, _btl.y()));
			_bbr = QPointF(qMax(rx, _bbr.x()), qMax(by, _bbr.y()));
		}
	}

	QRectF Route::boundingRect() const
	{
		return QRectF(_btl.x(), _btl.y(),
					  _bbr.x() - _btl.x(), _bbr.y() - _btl.y());
	}

	void Route::paint(QPainter *painter, 
		const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		const double zoom_level = 
			option->levelOfDetailFromTransform(painter->worldTransform());
		
		if (zoom_level < 0.2) return;
		if ((Route::HIDE_UNSELECTED_LINE && !_selected)
				|| (Route::HIDE_UNHIGHLIGHTED_LINE && !_highlighted))
			return;

		for (const auto& line : _lines)
			paint_line(painter, option, line);
	}

	void Route::paint_line(QPainter *painter, const QStyleOptionGraphicsItem *option,
		const RouteLine& line, double pen_width)
	{
		static Colors colors[] = {
			{Qt::yellow, QColor(80, 255, 80), Qt::darkGreen},	// switch
			{Qt::cyan, Qt::red, Qt::magenta}					// fixed
		};
		const double zoom_level = 
			option->levelOfDetailFromTransform(painter->worldTransform());
		QPen pen(colors[line.type][Colors::basic]);
		if (_selected) {
			pen.setColor(colors[line.type][Colors::selected]);
			pen_width += 1.0;
#ifdef CHANGE_PEN_WIDTH
			if (zoom_level > 2.0)         pen_width += 0.8;
			else if (zoom_level > 1.0)    pen_width += 1.8;
			else                          pen_width += 3.0;
#endif
		}
		else if (_highlighted) {
			pen.setColor(colors[line.type][Colors::highlighted]);
			pen_width += 1.2;
		}
		pen.setWidthF(pen_width);
		painter->setPen(pen);

#ifdef USE_RENDER
		painter->setRenderHint(QPainter::Antialiasing, true);
#endif
		painter->drawLine(line.p1, line.p2);
	}

	void Route::mousePressEvent(QGraphicsSceneMouseEvent* event)
	{
		QGraphicsItem::mousePressEvent(event);
		if (event->button() == Qt::LeftButton) {
			set_selected(true);
			NetFrom = this;
			NetMoveStarted = true;
		}
	}

	QString Route::STA_info() const
	{
		QString sta_info;
		for (const auto& pin : _sources)
			sta_info += QString("Source(%1,%2): %3\n").arg(pin.pos.x).arg(pin.pos.y).arg(QString::fromStdString(pin.name));

		for (const auto& pin : _sinks) {
			sta_info += QString("Sink(%1,%2): %3\n").arg(pin.pos.x).arg(pin.pos.y).arg(QString::fromStdString(pin.name));
			if (SHOW_TIME_INFO) {
				sta_info += "    rise delay: " + QString::number(pin.rise_delay) + " ns\n";
				sta_info += "    fall delay: " + QString::number(pin.fall_delay) + " ns\n";
			}
		}
		return sta_info;
	}


///////////////////////////////////////////////////////////////////////////////
// Primitive

	Primitive::Primitive(Instance* primitive)
			: _primitive(primitive), _leftCnt(0), _rightCnt(0), _level(LEVEL_UNSET)
	{
		setFlag(QGraphicsItem::ItemIsSelectable);

		for (Pin* pin: _primitive->pins()) {
			PrimitivePin* ppin = new PrimitivePin(pin);
			_pinMap[pin] = ppin;
			addPin(ppin);
		}
		_rectf = QRectF(0., 0., 
			MIN_WIDTH, PIN_SPACE * (qMax(_leftCnt, _rightCnt) + PIN_MARGIN_IDX * 2));
	}

	Primitive::~Primitive()
	{
		while (!childItems().empty())
			delete qgraphicsitem_cast<PrimitivePin *>(childItems().takeFirst());
	}

	void Primitive::addPin(PrimitivePin* ppin)
	{
		ppin->setParentItem(this);

		switch (ppin->pin()->dir()) {
			case COS::INPUT:
			case COS::INOUT:
				ppin->setSide(PrimitivePin::LEFT);
				ppin->setSideIdx(_leftCnt++);
				break;
			case COS::OUTPUT:
				ppin->setSide(PrimitivePin::RIGHT);
				ppin->setSideIdx(_rightCnt++);
				break;
			default:
				string name = ppin->pin()->name();

				if (_primitive->down_module()->type() == "GRM") {
					if (ppin->pin()->net()->num_pins() > 1 &&
						!isModulePinConnected(ppin->pin())) {
						ppin->setSide(PrimitivePin::LEFT);
						ppin->setSideIdx(_leftCnt++);
					} else {
						ppin->setSide(PrimitivePin::RIGHT);
						ppin->setSideIdx(_rightCnt++);
					}
					return;
				}

				if ((name.length() > name.find("OUT")) || (name.length() > name.find("out"))) {
					ppin->setSide(PrimitivePin::RIGHT);
					ppin->setSideIdx(_rightCnt++);
				} else if ((name.length() > name.find("IN")) || (name.length() > name.find("in"))) {
					ppin->setSide(PrimitivePin::LEFT);
					ppin->setSideIdx(_leftCnt++);
				} else if (ppin->pin()->net()->num_pins() > 1 &&
					!isModulePinConnected(ppin->pin())) {
					ppin->setSide(PrimitivePin::LEFT);
					ppin->setSideIdx(_leftCnt++);
				} else {
					ppin->setSide(PrimitivePin::RIGHT);
					ppin->setSideIdx(_rightCnt++);
				}
		}
	}

	PrimitivePin* Primitive::findPin(Pin* pin)
	{
		auto iter = _pinMap.find(pin);
		return (_pinMap.end() == iter) ? NULL : iter->second;
	}

	void PrimitivePin::setSideIdx(int sideIdx, qreal width, qreal height)
	{
		_sideIdx = sideIdx;
		qreal x = width ? width : Primitive::MIN_WIDTH;

		switch (_side) {
			case LEFT:
				setPos(0., (_sideIdx + Primitive::PIN_MARGIN_IDX) * Primitive::PIN_SPACE);
				break;
			case RIGHT:
				setPos(x, (_sideIdx + Primitive::PIN_MARGIN_IDX) * Primitive::PIN_SPACE);
				break;
			case TOP:
				setPos((_sideIdx + Primitive::PIN_MARGIN_IDX) * Primitive::PIN_SPACE, 0.);
				break;
			case BOTTOM:
				setPos((_sideIdx + Primitive::PIN_MARGIN_IDX) * Primitive::PIN_SPACE, height);
				break;
			default:
				setPos(0, 0);
		}
	}
	
	void Primitive::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		QPen pen(Qt::white);
		if (isSelected()) {
			pen.setColor(Qt::yellow);
			pen.setCapStyle(Qt::RoundCap);
			pen.setWidth(1);
		}
		painter->setBrush(Qt::NoBrush);
		QFont font("Consolas", 4);
		font.setStyleStrategy(QFont::ForceOutline);
		painter->setFont(font);

#ifdef USE_RENDER
		painter->setRenderHint(QPainter::Antialiasing, true);
#endif
		painter->setPen(pen);

		painter->drawText(_rectf, Qt::AlignCenter | Qt::TextDontClip, 
			QString::fromStdString(_primitive->name()) + "(" +
			QString::fromStdString(_primitive->down_module()->type()) + ")");

		if (isSelected()) painter->setPen(Qt::cyan);
		painter->drawRect(_rectf);
	}

///////////////////////////////////////////////////////////////////////////////
// GRM

	GRM::GRM(Instance* primitive) : Primitive(primitive) 
	{ }

	void GRM::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		QPen pen(Qt::white);
		if (isSelected()) {
			pen.setColor(Qt::yellow);
			pen.setCapStyle(Qt::RoundCap);
			pen.setWidth(1);
		}
		painter->setBrush(Qt::NoBrush);
		QFont font("Consolas", 4);
		font.setStyleStrategy(QFont::ForceOutline);
		painter->setFont(font);

#ifdef USE_RENDER
		painter->setRenderHint(QPainter::Antialiasing, true);
#endif
		painter->setPen(pen);

		painter->drawText(_rectf, Qt::AlignCenter | Qt::TextDontClip, 
			QString::fromStdString(_primitive->name()) + "\n(" +
			QString::fromStdString(_primitive->down_module()->type()) + ")");

		if (isSelected()) painter->setPen(Qt::cyan);
		painter->drawRect(_rectf);
	}

///////////////////////////////////////////////////////////////////////////////
// SliceBlock
	SliceBlock::SliceBlock(Instance* primitive)
		: Primitive(primitive) 
	{}

	void SliceBlock::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		QPen pen(Qt::white);
		if (isSelected()) {
			pen.setColor(Qt::yellow);
			pen.setCapStyle(Qt::RoundCap);
			pen.setWidth(1);
		}
		painter->setBrush(Qt::NoBrush);
		QFont font("Consolas", 4);
		font.setStyleStrategy(QFont::ForceOutline);
		painter->setFont(font);

#ifdef USE_RENDER
		painter->setRenderHint(QPainter::Antialiasing, true);
#endif
		painter->setPen(pen);

		painter->drawText(_rectf, Qt::AlignCenter | Qt::TextDontClip, 
			QString::fromStdString(_primitive->name()) + "\n(" +
			QString::fromStdString(_primitive->down_module()->type()) + ")");

		if (isSelected()) painter->setPen(Qt::cyan);
		painter->drawRect(_rectf);
	}

///////////////////////////////////////////////////////////////////////////////
// PrimitivePin
	PrimitivePin::PrimitivePin(Pin* pin, Side side, int sideIdx)
		: _pin(pin), _side(side), _sideIdx(sideIdx), _hovered(false)
	{
		_rectf = QRectF();
		updateBoundingRect();
		setAcceptHoverEvents(true);
		setFlag(QGraphicsItem::ItemIsSelectable);
		_textItem = new QGraphicsTextItem(QString::fromStdString(_pin->name()));
		_textItem->setDefaultTextColor(Qt::yellow);
		_textItem->setPos(4, -2);
		_textItem->setFont(QFont("Consolas", 2));
		_textItem->setParentItem(this);
		_textItem->hide();
	}

	void PrimitivePin::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		painter->setBrush(Qt::NoBrush);

#ifdef USE_RENDER
		painter->setRenderHint(QPainter::Antialiasing, true);
#endif
		if (_hovered || isSelected()) {
			painter->setPen(Qt::yellow);
			_textItem->show();
		} else {
			painter->setPen(Qt::white);
			_textItem->hide();
		}

		switch (_side) {
			case BOTTOM:
				painter->drawLine(0, 2, 0, 0);
				break;
			case RIGHT:
				painter->drawLine(2, 0, 0, 0);
				break;
			case TOP:
				painter->drawLine(0, -2, 0, 0);
				break;
			case LEFT:
			default:
				painter->drawLine(-2, 0, 0, 0);
		}
		
	}

	void PrimitivePin::updateBoundingRect()
	{
		switch (_side) {
			case BOTTOM:
				_rectf.setRect(-0.4, -0.4, 0.8, 2.8);
				break;
			case RIGHT:
				_rectf.setRect(-0.4, -0.4, 2.8, 0.8);
				break;
			case TOP:
				_rectf.setRect(-0.4, -1.2, 0.8, 2.8);
				break;
			case LEFT:
				_rectf.setRect(-1.4, -0.4, 2.8, 0.8);
				break;
			default:
				_rectf.setRect(0, 0, 0, 0);
		}
	}

	void PrimitivePin::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
	{
		_hovered = true;
		update();
	}

	void PrimitivePin::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
	{
		_hovered = false;
		update();
	}

///////////////////////////////////////////////////////////////////////////////
// PrimitiveNet
	
	PrimitiveNet::PrimitiveNet(Net* net)
		: _net(net)
	{
		_rectf = QRectF();
		setFlag(QGraphicsItem::ItemIsSelectable);
	}

	void PrimitiveNet::addTerminal(PrimitivePin* ppin)
	{
		_connectedPpins.push_back(ppin);
		updateBoundingRect();
	}

	void PrimitiveNet::addCorner(QPoint p)
	{
		_corner.push_back(p);
		updateBoundingRect();
	}

	void PrimitiveNet::updateBoundingRect()
	{
		if (_corner.size() <= 1) {
			int maxY = -9999;
			int minY = 9999;
			int maxX = -9999;
			int minX = 9999;
			for (PrimitivePin* ppin: _connectedPpins) {
				QPointF pos = ppin->mapToScene(0, 0);
				if (pos.y() > maxY) maxY = pos.y();
				if (pos.y() < minY) minY = pos.y();
				if (pos.x() > maxX) maxX = pos.x();
				if (pos.x() < minX) minX = pos.x();
			}
			_rectf.setTopLeft(QPointF(minX, minY));
			_rectf.setBottomRight(QPointF(maxX, maxY));
		}
		updateShape();
	}

	void PrimitiveNet::updateShape()
	{
		_path = QPainterPath();
		if (_corner.size() == 1) {
			int x = _corner.front().x();
			_path.addRect(x - 0.5, _rectf.top() - 0.2,
				1, _rectf.bottom() - _rectf.top());

			for (PrimitivePin* ppin: _connectedPpins) {
				QPointF ppos = ppin->mapToScene(0, 0);
				_path.addRect(ppos.x() - 0.2, ppos.y() - 0.5,
					x - ppos.x(), 1);
			}
		}
		else if (_corner.empty()) {
			if (_connectedPpins.empty()) return;
			bool first = true;
			for (vector<PrimitivePin *>::const_iterator iter = _connectedPpins.begin();
				iter != _connectedPpins.end(); ++iter)
				for (vector<PrimitivePin *>::const_iterator innerIter = iter + 1;
					innerIter != _connectedPpins.end(); ++innerIter) {
					QPointF p1 = (*iter)->mapToScene(0, 0);
					QPointF p2 = (*innerIter)->mapToScene(0, 0);
					_path.moveTo(p1.x(), p1.y() - 0.5);
					_path.lineTo(p2.x(), p2.y() - 0.5);
					_path.lineTo(p2.x(), p2.y() + 0.5);
					_path.lineTo(p1.x(), p1.y() + 0.5);
					_path.lineTo(p1.x(), p1.y() - 0.5);
				}
		}
	}

	void PrimitiveNet::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		QPen pen;
		if (isSelected()) {
			pen.setColor(Qt::white);
			pen.setCapStyle(Qt::RoundCap);
			pen.setWidth(1);
		} else {
			pen.setColor(Qt::green);
			pen.setCapStyle(Qt::RoundCap);
		}
		painter->setPen(pen);

		if (_corner.size() == 1) {
			if (_connectedPpins.size() < 2) return;

			int x = _corner.front().x();
			painter->drawLine(x, _rectf.top(), x, _rectf.bottom());

			for (PrimitivePin* ppin: _connectedPpins) {
				QPointF ppos = ppin->mapToScene(0, 0);
				painter->drawLine(ppos, QPointF(x, ppos.y()));
				if ((ppos.y() == _rectf.top()) || (ppos.y() == _rectf.bottom()))
					continue;
				painter->save();
				pen.setWidth(1);
				painter->setPen(pen);
				painter->drawPoint(x, ppos.y());
				painter->restore();
			}
		} else if (_corner.empty()) {
			for (vector<PrimitivePin *>::const_iterator iter = _connectedPpins.begin();
				iter != _connectedPpins.end(); ++iter)
				for (vector<PrimitivePin *>::const_iterator innerIter = iter + 1;
					innerIter != _connectedPpins.end(); ++innerIter)
					painter->drawLine((*iter)->mapToScene(0, 0), (*innerIter)->mapToScene(0, 0));
		}
	}

///////////////////////////////////////////////////////////////////////////////
// ModulePin
	ModulePin::ModulePin(Pin* pin)
		: _dir(IO), _level(Primitive::LEVEL_UNSET)
	{
		_rectf = QRectF();
		setFlag(QGraphicsItem::ItemIsSelectable);
		_pin = new PrimitivePin(pin);
		_pin->setParentItem(this);
		_pin->hide();

		_textItem = new QGraphicsTextItem(QString::fromStdString(_pin->pin()->name()));
		_textItem->setDefaultTextColor(Qt::yellow);
		_textItem->setPos(4, -2);
		_textItem->setFont(QFont("Consolas", 2));
		_textItem->setParentItem(this);
		_textItem->hide();

		switch (pin->dir()) {
			case COS::INPUT:
				_dir = INPUT;
				_rectf.setRect(-5, -0.5, 5, 1);
				break;
			case COS::OUTPUT:
				_dir = OUTPUT;
				_rectf.setRect(0, -0.5, 5, 1);
				break;
			default:
				_dir = IO;
				_rectf.setRect(-2.5, -0.5, 5, 1);
		}
	}

	void ModulePin::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
	{
		QPen pen;
		if (isSelected()) {
			pen.setColor(Qt::yellow);
			pen.setCapStyle(Qt::RoundCap);
			pen.setWidth(1);

			_textItem->show();
		} else {
			pen.setColor(Qt::white);
			_textItem->hide();
		}
		painter->setPen(pen);

		switch (_dir) {
			case INPUT:
				painter->drawLine(QLineF(0,		0,		-0.5,	-0.5));
				painter->drawLine(QLineF(-0.5,	-0.5,	-5,		-0.5));
				painter->drawLine(QLineF(-5,	-0.5,	-5,		0.5));
				painter->drawLine(QLineF(-5,	0.5,	-0.5,	0.5));
				painter->drawLine(QLineF(-0.5,	0.5,	0,		0));
				break;
			case OUTPUT:
				painter->drawLine(QLineF(0,		0,		0.5,	-0.5));
				painter->drawLine(QLineF(0.5,	-0.5,	5,		-0.5));
				painter->drawLine(QLineF(5,		-0.5,	5,		0.5));
				painter->drawLine(QLineF(5,		0.5,	0.5,	0.5));
				painter->drawLine(QLineF(0.5,	0.5,	0,		0));
				break;
			default:
				painter->drawLine(QLineF(-2,	-0.5,	2,		-0.5));
				painter->drawLine(QLineF(-2,	0.5,	2,		0.5));
				painter->drawLine(QLineF(-2.5,	0,		-2,		0.5));
				painter->drawLine(QLineF(-2.5,	0,		-2,		-0.5));
				painter->drawLine(QLineF(2.5,	0,		2,		0.5));
				painter->drawLine(QLineF(2.5,	0,		2,		-0.5));
				painter->drawLine(QLineF(2.5,	0,		-2.5,	0));
		}
	}
}}
