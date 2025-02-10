#include "ViewWidget.h"
#include <numeric>
#include <stdexcept>

namespace FPGAViewer {

	ViewWidget::ViewWidget(QWidget *parent /* = 0 */)
		: QWidget(parent), _arch(new Arch()), _design(new UserDesign(this, _arch.get())),
		  _scene(new QGraphicsScene(this)), _view(new MainView(_scene, this))
	{
		_scene->setBackgroundBrush( QColor(82, 82, 82) ); // dark gray
		QVBoxLayout* layout= new QVBoxLayout(this); // memory deallocated automatically
		layout->addWidget(_view);
		setLayout(layout);
		setAcceptDrops(true);

		connect(_view, SIGNAL(viewport_changed(QRectF)), this, SIGNAL(draw_viewport(QRectF)));
		connect(_view, SIGNAL(cluster_moved(Point, Point)), this, SLOT(cluster_moved(Point, Point)));
		connect(_view, SIGNAL(net_pulled(const string&, Point)), this, SLOT(net_moved(const std::string &, Point)));
		connect(_view, SIGNAL(deselect_all()), this, SLOT(deselect_all()));
		connect(_view, SIGNAL(tile_double_clicked(const string&)), this, SIGNAL(update_tile(const string&)));

		connect(&_rt_process, SIGNAL(readyReadStandardOutput()), this, SLOT(disp_stdout()));
		connect(&_rt_process, SIGNAL(readyReadStandardError()), this, SLOT(disp_stderr()));
		connect(&_bg_process, SIGNAL(readyReadStandardOutput()), this, SLOT(disp_stdout()));
		connect(&_bg_process, SIGNAL(readyReadStandardError()), this, SLOT(disp_stderr()));

		connect(&_rt_process, SIGNAL(started()), this, SIGNAL(process_started()));
		connect(&_rt_process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(route_finish_handler()));
	
		// NEW! use model to store information
		_netProxy = new QSortFilterProxyModel(this);
		_netModel = new QStandardItemModel(this);
		_netProxy->setSourceModel(_netModel);
		
		_padProxy = new QSortFilterProxyModel(this);
		_padModel = new QStandardItemModel(this);
		_padProxy->setSourceModel(_padModel);
	}

	ViewWidget::~ViewWidget()
	{
		free_design();
		_arch->clear();
	}

	void ViewWidget::load_arch(const string& file_name)
	{
		free_design();
		_arch->clear();
		_arch->load_file(file_name);

		emit add_progress("Getting tile detail information...");
		_arch->get_tile_detail_info();

		emit add_progress("Getting instance layout information...");
		_arch->get_instance_layout_info();

		//emit add_progress("Releasing arch information...");
		//FPGADesign::release(); // release arch info

		for (auto [pos, cluster] : _arch->_cluster_map) {
			connect(cluster, SIGNAL(show_relative_route(route_map, bool)),
				this, SLOT(highlight_route(route_map, bool)));

			addPadModel(pos);
		}

		emit add_progress("Drawing devices...");
		_arch->draw_device(_view);

		emit file_is_loaded();
	}

	void ViewWidget::load_design(const string& file_name)
	{
		_design->load(file_name);

		emit add_progress("Drawing placement...");
		_design->show_placement();

		emit add_progress("Loading Routes");
		_design->add_route();

		emit emit_draw(route_visible);
		emit file_is_loaded();
	}

	void ViewWidget::free_design()
	{
		_design->clear();
		emit free_design_data();
		emit design_is_modified(false);
		emit design_route_is_modified(false);
	}

	void ViewWidget::free_modify()
	{
		_design->clear_modify();
		emit design_is_modified(false);
		emit design_route_is_modified(false);
	}


	void ViewWidget::draw_route(bool draw/* = true*/)
	{
		_design->show_route(draw);
	}


	void ViewWidget::add_net(Route* net)
	{
		_scene->addItem(net);
		connect(net,	SIGNAL(set_current(int)),
				this,	SIGNAL(set_current_net(int)));
		connect(net,	SIGNAL(show_sta_info(QString)),
				this,	SIGNAL(show_sta_info(QString)));
		connect(net,	SIGNAL(show_relative_cluster(const Pins&, const Pins&, bool)),
				this,	SLOT(highlight_cluster(const Pins&, const Pins&, bool)));
	}

	void ViewWidget::save_placement(QString save_file_path)
	{
		_design->save_placement(save_file_path);
		emit design_is_modified(false);
	}

	void ViewWidget::save_rt_placement(QString save_file_path)
	{
		_design->save_rt_placement(save_file_path);

		QStringList args = {
			save_file_path,
			_arch->file_path(),
			"-o", save_file_path.replace(".xml", "_rt.xml"),
			"-e"
		};

		_rt_process.start("./route.exe", args);

		//QMessageBox msgBox;
		//msgBox.setText("Generate Bit Stream?");
		//msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		//msgBox.setDefaultButton(QMessageBox::Yes);
		//int ret = msgBox.exec();
		//if (ret == QMessageBox::No) return;
		//setCursor(Qt::WaitCursor);
		//_rt_process.waitForFinished();
		//unsetCursor();
		//bit_generate(_cil_file_path, save_file_path);
	}

	void ViewWidget::bit_generate(QString cil_file_path, QString netlist_file_path)
	{
		QString nl_file;
		if (netlist_file_path.isEmpty())
			nl_file = _design->file_path();
		else
			nl_file = netlist_file_path;

		QStringList args = {
			"-a", _arch->file_path(),
			"-c", cil_file_path,
			"-n", nl_file,
			"-b", nl_file.replace(".xml", ".bit"),
			"-e"
		};

		_rt_process.waitForFinished();
		_bg_process.start("./bitgen.exe", args);
	}

	void ViewWidget::hide_unselected_line(bool hide)
	{
		Route::HIDE_UNSELECTED_LINE = hide;
		update_view();
	}

	void ViewWidget::hide_unhighlighted_line(bool hide)
	{
		Route::HIDE_UNHIGHLIGHTED_LINE = hide;
		update_view();
	}

	void ViewWidget::select_changed(string cur, string pre)
	{
		if (pre.empty())
			deselect_all();
		else
			select_route_by_name(pre, false);

		select_route_by_name(cur, true);
	}

	void ViewWidget::select_route_by_name(const string& name, bool select)
	{
		if (Route* net = _design->get_route_by_name(name)) {
			net->set_selected(select);
			net->update();
		}
	}

	void ViewWidget::select_cluster_by_pos(const Point &pos, bool select)
	{
		if (Cluster * cluster = _arch->get_cluster_by_pos(pos)) {
			cluster->set_selected(select);
			cluster->update();
		}
	}

	void ViewWidget::cluster_moved(Point from, Point to)
	{
		_design->cluster_moved(from, to);
		emit design_is_modified(true);
	}

	void ViewWidget::net_moved(const std::string& net, Point to)
	{
		if (net.empty()) return;

		Cluster* moveTo = _arch->get_cluster_by_pos(to);
		ASSERTS(moveTo);
		moveTo->set_movable(true);
		moveTo->set_used(true);
		_view->viewport()->update();

		_design->net_moved(net, to);
		emit design_route_is_modified(true);
	}

	void ViewWidget::disp_stdout()
	{
		QString info(_rt_process.readAllStandardOutput() + 
					_bg_process.readAllStandardOutput());
		emit add_progress(info.toStdString());
	}

	void ViewWidget::disp_stderr()
	{
		QString info(_rt_process.readAllStandardError() + 
					_bg_process.readAllStandardError());
		emit add_progress(info.toStdString());
	}

	void ViewWidget::highlight_cluster(const Pins& sources, const Pins& sinks, bool selected)
	{
		for (const auto& pin : sources) {
			if (Cluster * cluster = _arch->get_cluster_by_pos(pin.pos))
				cluster->set_highlighted(selected);
		}

		for (const auto& pin : sinks) {
			if (Cluster* cluster = _arch->get_cluster_by_pos(pin.pos))
				cluster->set_highlighted(selected);
		}
	}

	void ViewWidget::highlight_route(route_map routes, bool selected)
	{
		deselect_all();

		QStringList rt_list;

		for (auto it = routes.begin(); it != routes.end(); ++it)
		{
			if (Route* route = _design->get_route_by_name(it->second->name())) {
				route->set_highlighted(selected);
				route->update();
				rt_list.push_back(QString::fromStdString(route->name()));
			}
		}
		if (rt_list.isEmpty() || !selected) return;
		emit list_highlighted_net(rt_list);
	}

	void ViewWidget::drag_rt_list(QListWidgetItem* item, bool begin)
	{
		Route::NetMoveStarted = begin;
		Route::NetFrom = item ? _design->get_route_by_name(item->text().toStdString()) : nullptr;
	}

	void ViewWidget::route_finish_handler()
	{
		QMessageBox msgBox;
		msgBox.setText("Generate Bit Stream?");
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		msgBox.setDefaultButton(QMessageBox::Yes);
		int ret = msgBox.exec();
		if (ret == QMessageBox::No) return;

		QString cil_file_path = _arch->file_path();
		cil_file_path.replace("_arch.xml", "_cil.xml");

		QFile file_finder;
		if (!file_finder.exists(cil_file_path)) {
			cil_file_path = QFileDialog::getOpenFileName(this,
			"Choose the CIL file", ".", "XML files (*.xml)");
		}

		if (!cil_file_path.isEmpty())
			bit_generate(cil_file_path, _design->save_path());

		emit process_finished();
	}

	void ViewWidget::zoom_in(QString item)
	{
		_view->zoom_in(_design->get_route_by_name(item.toStdString()));
	}

	void ViewWidget::zoom_out(QString item)
	{
		_view->zoom_out(_design->get_route_by_name(item.toStdString()));
	}

	void ViewWidget::addSignalTap()
	{
		QDialog tapDialog(this);

		QLabel* netLbl = new QLabel(tr("NET to Observe: "));
		QLabel* iobLbl = new QLabel(tr("PAD to view the signal: "));

		QComboBox* netComb = new QComboBox;
		netComb->setEditable(true);
		_netProxy->sort(0);
		netComb->setModel(_netProxy);
		QComboBox* iobComb = new QComboBox;
		iobComb->setEditable(true);
		_padProxy->sort(0);
		iobComb->setModel(_padProxy);

		netLbl->setBuddy(netComb);
		iobLbl->setBuddy(netComb);

		QDialogButtonBox* buttonBox = new QDialogButtonBox(
			QDialogButtonBox::Ok |
			QDialogButtonBox::Cancel);

		connect(buttonBox,	SIGNAL(accepted()),	&tapDialog, SLOT(accept()));
		connect(buttonBox,	SIGNAL(rejected()),	&tapDialog, SLOT(reject()));

		QGridLayout* glayout = new QGridLayout;
		glayout->addWidget(netLbl, 0, 0);
		glayout->addWidget(netComb, 0, 1);
		glayout->addWidget(iobLbl, 1, 0);
		glayout->addWidget(iobComb, 1, 1);
		
		QVBoxLayout* vlayout = new QVBoxLayout;
		vlayout->addLayout(glayout);
		vlayout->addWidget(buttonBox);

		tapDialog.setLayout(vlayout);
		tapDialog.setWindowTitle(tr("Add SignalTap"));

		if (tapDialog.exec()) {
			net_moved(netComb->currentText().toStdString(),
				iobComb->itemData(iobComb->currentIndex(), NetlistRole).value<Point>());
		}
	}

	void ViewWidget::addPadModel(Point clusterPos)
	{
		string padname = _arch->find_pad_by_position(clusterPos);
		if (padname.empty()) return;

//		FDU_LOG(DEBUG) << "PAD ADDED!" << padname;
		QStandardItem* padItem = new QStandardItem(QString::fromStdString(padname));
			padItem->setData(QVariant::fromValue(clusterPos), NetlistRole);
			_padModel->appendRow(padItem);
	}

	void ViewWidget::addNetModel(const string& net_name)
	{
		QStandardItem* netItem = new QStandardItem(QString::fromStdString(net_name));
		//netItem->setData(QVariant::fromValue((void*)net), NetlistRole);
		_netModel->appendRow(netItem);
	}

}
