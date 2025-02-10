#ifndef SRC_VIEW_WIDGET_H
#define SRC_VIEW_WIDGET_H

#include <string>
#include <QtGui>
#include <QtCore>
#include <QtWidgets>

#include "Arch.h"
#include "UserDesign.h"
#include "GraphicItem.h"
#include "MainView.h"
#include "map_utils.h"

Q_DECLARE_METATYPE(COS::Point);

namespace FPGAViewer {
	using std::string;
	using namespace ARCH;
	using namespace COS;
	using namespace GraphicItemLib;

	class ViewWidget : public QWidget
	{
		Q_OBJECT

		typedef map<string, Route*>	route_map;

	public:

		ViewWidget(QWidget *parent = 0);

		~ViewWidget();
		void		free_design();
		void		free_modify();

		void		load_arch(const string& file_name);
		void		load_design(const string& file_name);

		//void		draw_placement(bool draw = true);
		void		draw_route(bool draw = true);
		
		void		save_placement(QString save_file_path);
		void		save_rt_placement(QString save_file_path);
		void		bit_generate(QString cil_file_path, QString netlist_file_path = "");
		void		hide_unselected_line(bool hide);
		void		hide_unhighlighted_line(bool hide);

		// graphics view related
		void		update_view() const { _view->viewport()->update(); }
		void		full_scale()  const { _view->full_scale(); }
		void		zoom_in(QString item = "");
		void		zoom_out(QString item = "");

		void		select_route_by_name(const string& name, bool select);
		void		select_cluster_by_pos(const Point &pos, bool select);

		QStringList	creat_route_list() const { return _design->creat_route_list(); }
		bool		arch_is_ready()    const { return _arch->is_ready(); }
		bool		design_modified()  const { return _design->is_modified(); }
		const QString& design_path()   const { return _design->file_path(); }

		bool route_visible = false;

	public slots:
		void		addSignalTap();
	signals:
		void		design_is_modified(bool modified);
		void		design_route_is_modified(bool modified);
		void		draw_viewport(QRectF);
		void		set_current_net(int);
		void		emit_draw(bool);
		void		show_sta_info(QString);
		void		free_design_data();
		void		list_highlighted_net(QStringList);

		void		file_is_loading();
		void		file_is_loaded();
		void		add_progress(const string& output);
		void		set_status(const string& status);
		void		process_started();
		void		process_finished();

		void		deselect_all_nets();

		void		update_tile(const string& tileName);

	private slots:
		void		select_changed(string, string);
		void		cluster_moved(Point, Point);
		void		net_moved(const std::string &, Point);

		void		disp_stdout();
		void		disp_stderr();

		void		highlight_cluster(const Pins&, const Pins&, bool);
		void		highlight_route(route_map, bool);

		void		deselect_all() { _design->deselect_all(); }

		void		drag_rt_list(QListWidgetItem*, bool);

		void		route_finish_handler();
		
	private:

		friend class UserDesign;
		void		add_net(Route* net);
		void		addPadModel(Point clusterPos);
		void		addNetModel(const string& net_name);

	private:
		enum MyRoles {
			NetlistRole = Qt::UserRole + 1
		};

		std::unique_ptr<Arch> _arch;
		std::unique_ptr<UserDesign> _design;

		QGraphicsScene*		_scene;
		MainView*			_view;

		QProcess			_rt_process;
		QProcess			_bg_process;

		QSortFilterProxyModel*	_netProxy;
		QStandardItemModel*		_netModel;

		QSortFilterProxyModel*	_padProxy;
		QStandardItemModel*		_padModel;
	};


}

#endif // SRC_VIEW_WIDGET_H
