#include "MainView.h"
#include "BirdWidget.h"
#include "GraphicItem.h"
#include "SubArchWidget.h"

namespace FPGAViewer {
	
	using namespace GraphicItemLib;

	bool MainView::viewportEvent(QEvent *event)
	{
		bool return_val = QGraphicsView::viewportEvent(event);
		QRectF draw_rect(mapToScene(rect()).boundingRect());
		double w_time = scene()->sceneRect().width() / BirdWidget::BIRD_WIDTH;
		double h_time = scene()->sceneRect().height() / BirdWidget::BIRD_HEIGHT;
		double top(draw_rect.top() / h_time);
		double left(draw_rect.left() / w_time);
		double height(draw_rect.height() / h_time);
		double width(draw_rect.width() / w_time);

		// draw bird-view rect
		emit viewport_changed(QRectF(left, top, width, height));
		return return_val;
	}

	void MainView::mousePressEvent(QMouseEvent *event)
	{
		if (event->button() == Qt::RightButton) {
			if (!_rubber_started) { 
				_zoom_rect.setTopLeft(event->pos());
				_rubber_started_point = event->pos();
			}
			_zoom_rect.setBottomRight(event->pos());
			_rubber_started = true;
			setCursor(Qt::CrossCursor);
		}
		else
			QGraphicsView::mousePressEvent(event);
	}

	// for show of zoom rect
	void MainView::mouseMoveEvent(QMouseEvent *event)
	{
		if (!_rubber_started) return;

		QGraphicsScene *scene = this->scene();
		if (_last_rubber_rect) {
			scene->removeItem(_last_rubber_rect);
			delete _last_rubber_rect;
			_last_rubber_rect = 0;
		}
		QPointF top_left = mapToScene(_rubber_started_point);
		QPointF bot_right = mapToScene(event->pos());
		_last_rubber_rect = scene->addRect(QRectF(top_left, bot_right), QPen(Qt::yellow));
		this->viewport()->update();
	}

	void MainView::mouseReleaseEvent(QMouseEvent *event)
	{
		if (event->button() == Qt::RightButton && _rubber_started)
		{
			_zoom_rect.setBottomRight(event->pos());
			unsetCursor();
			// handle zoom selection
			_zoom_rect = _zoom_rect.normalized();
			if (_zoom_rect.height() < 5. || _zoom_rect.width() < 5.) return;
			QPointF center_pos((_zoom_rect.left() + _zoom_rect.right()) / 2.0,
				(_zoom_rect.top() + _zoom_rect.bottom())/ 2.0);

			centerOn(mapToScene(center_pos.toPoint()));
			double zoom_scale = qMin(height() / _zoom_rect.height(), width() / _zoom_rect.width());
			scale(zoom_scale, zoom_scale);
			// end zoom
		}
		else
		{
			QGraphicsView::mouseReleaseEvent(event);
			if (event->button() == Qt::LeftButton && 
				(Cluster::MoveStarted || Route::NetMoveStarted) )
			{
				QPointF new_pos(mapToScene(event->pos()));
				if (Cluster *MoveTo = dynamic_cast<Cluster*>(scene()->itemAt(new_pos, QTransform())))
				{
					Cluster *MoveFrom = Cluster::MoveFrom;
					if (MoveFrom &&
						MoveTo != MoveFrom &&
						!MoveTo->is_used() &&
						MoveTo->same_type(MoveFrom))
					{
						MoveTo->set_movable(true);
						MoveTo->set_used(true);
						MoveFrom->set_used(false);
						MoveFrom->set_movable(false);
						emit cluster_moved(MoveFrom->logic_pos(), MoveTo->logic_pos());
						viewport()->update();
					}
					Route* NetFrom = Route::NetFrom;
					if (NetFrom && 
						!MoveTo->is_used() &&
						(MoveTo->cluster_type() == "IOB") &&
						!(MoveTo->pad()).empty())
					{
						MoveTo->set_movable(true);
						MoveTo->set_used(true);
						string net = NetFrom->name();
						emit net_pulled(NetFrom->name(), MoveTo->logic_pos());
						viewport()->update();
					}
					else if ((MoveTo->cluster_type() == "IOB") && 
							 (MoveTo->pad()).empty())
					{
						QMessageBox msgBox;
						msgBox.setText("This cluster is unreachable from outside the package.");
						msgBox.exec();
					}
				}
			}
			else
				emit deselect_all();

			Cluster::MoveStarted = false;
			Cluster::MoveFrom    = 0;

			Route::NetMoveStarted = false;
			Route::NetFrom = 0;
		}

		QGraphicsScene *scene = this->scene();
		if (_last_rubber_rect != 0)
		{	// delete the zoom rect when mouse released
			scene->removeItem(_last_rubber_rect);
			delete _last_rubber_rect;
			_last_rubber_rect = 0;
		}

		_rubber_started = false;
	}

	void MainView::dragEnterEvent(QDragEnterEvent *event)
	{
		if (event->mimeData()->hasFormat("text/plain"))
			event->acceptProposedAction();
	}

	void MainView::dropEvent(QDropEvent *event)
	{
		QPointF new_pos(mapToScene(event->pos()));
		if (Cluster *MoveTo = dynamic_cast<Cluster*>(scene()->itemAt(new_pos, QTransform())))
		{
			Route* NetFrom = Route::NetFrom;
			if (NetFrom && 
				!MoveTo->is_used() &&
				(MoveTo->cluster_type() == "IOB") &&
				!(MoveTo->pad()).empty())
			{
				MoveTo->set_movable(true);
				MoveTo->set_used(true);
				string net = NetFrom->name();
				emit net_pulled(NetFrom->name(), MoveTo->logic_pos());
				viewport()->update();
			}
			else if ((MoveTo->cluster_type() == "IOB") && 
					 (MoveTo->pad()).empty())
			{
				QMessageBox msgBox;
				msgBox.setText("This cluster is unreachable from outside the package.");
				msgBox.exec();
			}
			Route::NetMoveStarted = false;
			Route::NetFrom = 0;
		}
	}

	void MainView::mouseDoubleClickEvent(QMouseEvent *event)
	{
		if (QGraphicsItem *item = itemAt(event->pos())) {
			switch (item->type()) {
				case (Tile::Type):
					emit tile_double_clicked(qgraphicsitem_cast<Tile *>(item)->name());
					break;
			}
		}
	}

	void MainView::init_scale() 
	{
		QRectF scene_rect(scene()->sceneRect());
		
		/* I use the width() & height() instead of sizeHint() before,
		 * the problem is that the width() are not valid since the GUI 
		 * hasn't appear when using command line model
		 * This can be resolved
		 * by using sizeHint().width() & sizeHint().height()
		 */
		double view_width = sizeHint().width();
		double view_height = sizeHint().height();
		double full_scale = qMin(view_width / scene_rect.width(), view_height / scene_rect.height());

		setMatrix(_one2one_matrix);       // reset to 1:1 view-scene
		scale(full_scale, full_scale);
		_reset_matrix = matrix();
	}

	void MainView::init_view()
	{
		setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
		setOptimizationFlag(QGraphicsView::DontSavePainterState);
		_rubber_started = false;
		_last_rubber_rect = 0;
	}

	void MainView::zoom_in(QGraphicsItem *net)
	{
		scale(2, 2);
		QList<QGraphicsItem *> items = scene()->selectedItems();
		if (items.count() == 1) {
			QGraphicsItem *item = items.takeFirst();
			centerOn(item->scenePos() + item->boundingRect().center());
			return;
		}
		if (net)
			centerOn(net->scenePos() + net->boundingRect().center());
	}

	void MainView::zoom_out(QGraphicsItem *net)
	{
		scale(0.5, 0.5);
		QList<QGraphicsItem *> items = scene()->selectedItems();
		if (items.count() == 1) {
			QGraphicsItem *item = items.takeFirst();
			centerOn(item->scenePos() + item->boundingRect().center());
			return;
		}
		if (net)
			centerOn(net->scenePos() + net->boundingRect().center());
	}
}
