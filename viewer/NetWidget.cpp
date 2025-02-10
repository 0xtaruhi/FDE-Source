#include "NetWidget.h"

namespace FPGAViewer {

	void RouteList::set_net_list(const QStringList& nets)
	{
		clear(); // clear all first
		// set a empty list will delete the route list

		for (QString net : nets)
			QListWidgetItem* item = new QListWidgetItem(net, this);
	}

	void RouteList::currentChanged(const QModelIndex& current, const QModelIndex& previous)
	{
		QListWidget::currentChanged(current, previous);
		string cur(current.data().toString().toStdString());
		string pre(previous.data().toString().toStdString());
		emit select_changed(cur, pre);
	}

	void RouteList::mouseReleaseEvent(QMouseEvent* event)
	{
		QListWidget::mouseReleaseEvent(event);
		emit rt_list_drag(0, false);
		//string cur(currentItem()->text().toStdString());
		//emit select_changed(cur, "");
	}

	void RouteList::mousePressEvent(QMouseEvent* event)
	{
		QListWidget::mousePressEvent(event);
		if (event->button() == Qt::LeftButton)
		{
			QListWidgetItem* item = currentItem();
			if (!item) return;
			emit rt_list_drag(item, true);

			QDrag* drag = new QDrag(this);
			QMimeData* mimeData = new QMimeData;

			mimeData->setText(item->text());
			drag->setMimeData(mimeData);

			Qt::DropAction dropAction = drag->exec();
		}
	}

	void RouteList::set_selected_route(int row)
	{
		setCurrentRow(row);
	}

	HRouteList::HRouteList(QWidget* parent) : RouteList(parent) {}

	void HRouteList::set_net_list(const QStringList& nets)
	{
		clear(); // clear all first
		// set a empty list will delete the route list

		for (QString net : nets)
			QListWidgetItem* item = new QListWidgetItem(net, this);
	}

	NetWidget::NetWidget(QWidget *parent)
	: QWidget(parent)
	{
		_txt_net_info            = new QTextEdit(this);
		_net_list                = new RouteList(this);
		_net_highlighted_list	 = new HRouteList(this);

		_txt_net_info->setReadOnly(true);

		QVBoxLayout *layout = new QVBoxLayout;
		_netlist_tab = new QTabWidget;
		_netlist_tab->addTab(_net_list, "All");
		_netlist_tab->addTab(_net_highlighted_list, "Highlighted");
		layout->addWidget(_netlist_tab);
		layout->addWidget(_txt_net_info);
		setLayout(layout);

		init_connection();
	}

	void NetWidget::init_connection()
	{
		connect(_net_highlighted_list,	SIGNAL(rt_list_drag(QListWidgetItem*, bool)), this, SIGNAL(rt_list_drag(QListWidgetItem*, bool)));
		connect(_net_highlighted_list,	SIGNAL(select_changed(string, string)), this, SIGNAL(select_changed(string, string)));
		connect(_net_list,				SIGNAL(rt_list_drag(QListWidgetItem*, bool)), this, SIGNAL(rt_list_drag(QListWidgetItem*, bool)));
		connect(_net_list,				SIGNAL(select_changed(string, string)), this, SIGNAL(select_changed(string, string)));
	}

	QString NetWidget::current_net() const
	{
		QListWidgetItem *item = _net_list->currentItem();
		return item == 0 ? "" : item->text();
	}

	void NetWidget::find_net(QString net)
	{
		if (net == _find_net_name) {
			if (_found_nets.isEmpty())
				_found_nets = _net_list->findItems(net, Qt::MatchContains);
			else {
				if (!_found_nets.isEmpty())
					_net_list->setCurrentItem(_found_nets.takeFirst());
			}
		}
		else {
			_find_net_name = net;
			_found_nets = _net_list->findItems(net, Qt::MatchContains);
			if (!_found_nets.isEmpty())
				_net_list->setCurrentItem(_found_nets.takeFirst());
			else {
				QMessageBox msg;
				msg.setText("No net found.");
				msg.exec();
			}
		}
	}
}