#ifndef SRC_NET_WIDGET_H
#define SRC_NET_WIDGET_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <string>

namespace FPGAViewer {
	using std::string;

	class RouteList : public QListWidget
	{
		Q_OBJECT

	public:
		RouteList(QWidget* parent = 0) : QListWidget(parent) {}

		void set_net_list(const QStringList& nets);
		void set_selected_route(int row);

	protected:
		void currentChanged(const QModelIndex& current, const QModelIndex& previous);
		void mouseReleaseEvent(QMouseEvent* event);
		void mousePressEvent(QMouseEvent* event);

	signals:
		void select_changed(string current, string previous);
		void rt_list_drag(QListWidgetItem* item, bool begin);
	};

	class HRouteList : public RouteList
	{
	public:
		HRouteList(QWidget* parent = 0);

		void set_net_list(const QStringList& nets);
		void set_h_net_list(const QStringList& nets);
	};


	class NetWidget : public QWidget
	{
		Q_OBJECT

	public:
		NetWidget(QWidget *parent = 0);

		QString  current_net()              const;
		QString  find_net_name()		    const {return _find_net_name; }

	public slots:
		void set_nets_list(const QStringList &nets)		{ _net_list->set_net_list(nets); }
		void set_h_nets_list(const QStringList &h_nets)	{ _net_highlighted_list->set_net_list(h_nets); }
		void set_sta_info(QString info)					{ _txt_net_info->setPlainText(info); }
		void set_selected_route(int row)				{ _net_list->set_selected_route(row); }
		void deselect_all_nets()						{ _net_highlighted_list->clear(); }
		void find_net(QString net);

	signals:
		void select_changed(string current, string previous);
		void rt_list_drag(QListWidgetItem* item, bool begin);

	private:

		void		init_connection();

		QTabWidget*	_netlist_tab;

		RouteList*	_net_list;
		HRouteList*	_net_highlighted_list;
		QTextEdit*	_txt_net_info;

		string		_selected_net_name;

		QString		_find_net_name;
		QList<QListWidgetItem *>	_found_nets;
	};

}
#endif