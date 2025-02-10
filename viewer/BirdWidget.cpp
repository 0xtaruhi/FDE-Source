#include "BirdWidget.h"


namespace FPGAViewer {

	const double BirdWidget::BIRD_WIDTH  = 190.;
	const double BirdWidget::BIRD_HEIGHT = 130.;
	const QRectF BirdWidget::BIRD_RECT   = QRectF(-5., -5., BIRD_WIDTH + 5., BIRD_HEIGHT + 5.);

	BirdWidget::BirdWidget(QWidget *parent)
	: QWidget(parent), _figure(nullptr), _viewport_rect(nullptr)
	{
		_bird_scene = new QGraphicsScene(this);
		_bird_scene->setSceneRect(BIRD_RECT);
		_bird_scene->setBackgroundBrush(Qt::black);

		_bird_view = new QGraphicsView(_bird_scene, this);
		QVBoxLayout *layout = new QVBoxLayout;
		layout->addWidget(_bird_view);
		setLayout(layout);

		draw_figure(QRectF(0., 0., BIRD_WIDTH, BIRD_HEIGHT));
	}

	void BirdWidget::draw_figure(QRectF figure_rect)
	{
		delete_item(_figure);
		_figure = _bird_scene->addRect(figure_rect, QPen(Qt::blue));
		_bird_view->viewport()->update();
	}

	void BirdWidget::draw_viewport_rect(QRectF bird_rect)
	{
		delete_item(_viewport_rect);
		_viewport_rect = _bird_scene->addRect(bird_rect, QPen(Qt::yellow));
		_bird_view->viewport()->update();
	}

	BirdWidget::~BirdWidget()
	{
		delete_item(_viewport_rect);
		delete_item(_figure);
	}

	void BirdWidget::delete_item(QGraphicsRectItem *item)
	{
		if (item) {
			_bird_scene->removeItem(item);
			delete item;
			item = nullptr;
		}
	}

}