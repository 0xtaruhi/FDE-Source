#ifndef SRC_MY_VIEW_H
#define SRC_MY_VIEW_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <string>
#include "utils.h"

namespace FPGAViewer {

	using namespace std;
	using std::string;
	using FDU::Point;

	class MainView : public QGraphicsView
	{
		Q_OBJECT
	public:
		MainView(QWidget *parent = 0) : QGraphicsView(parent) { init_view(); }
		MainView(QGraphicsScene *scene, QWidget *parent = 0) : QGraphicsView(scene, parent), _one2one_matrix(matrix()) { init_view(); }
		//~MainView();

		void    full_scale()       { setMatrix(_reset_matrix); }
		void    init_scale();

	signals:
		void    viewport_changed(QRectF);
		void    cluster_moved(Point, Point);
		void	net_pulled(const string&, Point);
		void	deselect_all();

		void	tile_double_clicked(const string&);

	public slots:
		void    zoom_in(QGraphicsItem *net = 0);
		void    zoom_out(QGraphicsItem *net = 0);

	protected:
		void    mousePressEvent(QMouseEvent *event);
		void    mouseMoveEvent(QMouseEvent *event);
		void    mouseReleaseEvent(QMouseEvent *event);
		void	dragEnterEvent(QDragEnterEvent *event);
		void	dropEvent(QDropEvent *event);
		bool    viewportEvent(QEvent *event);

		void	mouseDoubleClickEvent(QMouseEvent * event);

	private:
		void	init_view();

		bool      _rubber_started;
		QPoint    _rubber_started_point;

		QGraphicsItem    *_last_rubber_rect;
		//QGraphicsItem    *_now_rubber_rect;

		QRectF    _zoom_rect;
		QMatrix   _reset_matrix;

		QMatrix   _one2one_matrix;

	};

}
#endif
