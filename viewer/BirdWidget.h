#ifndef SRC_BIRD_WIDGET_H
#define SRC_BIRD_WIDGET_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <QObject>

namespace FPGAViewer {

	class BirdWidget : public QWidget {

		Q_OBJECT

	public:
		BirdWidget(QWidget *parent = 0);
		~BirdWidget();

		static const double BIRD_WIDTH;
		static const double BIRD_HEIGHT;
		static const QRectF BIRD_RECT;

	public slots:
		void draw_figure(QRectF);
		void draw_viewport_rect(QRectF);	

	private:

		void delete_item(QGraphicsRectItem*);

		QGraphicsView*     _bird_view;
		QGraphicsScene*    _bird_scene;

		QGraphicsRectItem*     _viewport_rect; // show the current viewport area
		QGraphicsRectItem*     _figure;        // show the figure of the chip
	};

}

#endif