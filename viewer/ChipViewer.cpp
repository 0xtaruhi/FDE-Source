#include "ChipViewer.h"

namespace FPGAViewer {

	ChipViewer::ChipViewer(QWidget *parent) : QMainWindow(parent)
	{
		setGeometry(QApplication::desktop()->availableGeometry());
		showMaximized();
		setWindowState(Qt::WindowMaximized);
		setWindowTitle("Chip Viewer");
		setWindowIcon(QIcon(":images/viewer.bmp"));

		InitAction();
		InitMenu();
		InitToolBar();
		InitStatusBar();

		initView();
		initDock();

		InitConnect();
	}

	void ChipViewer::initView()
	{
		_viewWidget = new ViewWidget(this);
		setCentralWidget(_viewWidget);
		_viewWidget->setWindowState(_viewWidget->windowState() | Qt::WindowFullScreen);
	
		_subArchWidget = new SubArchWidget;
	}

	void ChipViewer::initDock()
	{
		_netWidget = new NetWidget(this);
		_netDock = new QDockWidget(tr("Net Information"), this);
		_netDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		_netDock->setWidget(_netWidget);
		addDockWidget(Qt::LeftDockWidgetArea, _netDock);

		_birdWidget = new BirdWidget(this);
		_birdDock = new QDockWidget(tr("Bird View"), this);
		_birdDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		_birdDock->setWidget(_birdWidget);
		addDockWidget(Qt::LeftDockWidgetArea, _birdDock);

		_logText = new QTextEdit(this);
		_logText->setReadOnly(true);
		_logDock = new QDockWidget(tr("Output and Logs"), this);
		_logDock->setWidget(_logText);
		_logDock->setAllowedAreas(Qt::AllDockWidgetAreas);
		addDockWidget(Qt::BottomDockWidgetArea, _logDock);
	}

	void ChipViewer::InitAction()
	{
		///////////////////////////////////////////
		// Menu File related
		_actOpen     = new QAction(QIcon(":images/open_design.png"), tr("&Open Design"),    this);
		_actLoadArch = new QAction(QIcon(":images/open_arch.png"),   tr("&Load Arch"),      this);
		_actQuit     = new QAction(QIcon(":images/quit.png"),        tr("&Quit"),           this);
		_actSave     = new QAction(QIcon(":images/save.png"),        tr("&Save"),           this);
		_actSaveAs   = new QAction(QIcon(":images/save_as.png"),     tr("Save As"),         this);
		_actRTSave   = new QAction(QIcon(":images/route_save.png"),  tr("&Route and Save"), this);
		_actBitGen	 = new QAction(QIcon(":images/bit_gen.png"),     tr("Generate &Bit Stream"), this);
		_actAddTap	 = new QAction(QIcon(""),                        tr("Add Signal Tap"),  this);
		_actOpen->setShortcut(QKeySequence::Open);
		_actQuit->setShortcut(QKeySequence::Close);
		_actSave->setShortcut(QKeySequence::Save);
		_actSaveAs->setShortcut(QKeySequence::SaveAs);
		_actLoadArch->setShortcut(QKeySequence(tr("Ctrl+L")));

		///////////////////////////////////////////
		// Menu View related
		_actZoomIn        = new QAction(QIcon(":images/zoom_in.png"),    tr("Zoom &In"),   this);
		_actZoomOut       = new QAction(QIcon(":images/zoom_out.png"),   tr("Zoom &Out"),  this);
		_actFullScale     = new QAction(QIcon(":images/full_scale.png"), tr("Full &View"), this);
		_actFind		  = new QAction(QIcon(":images/find.png"),       tr("Find Net"),   this);
		_actFind->setShortcut		  (QKeySequence(tr("Ctrl+F")));
		_actZoomIn->setShortcut       (QKeySequence(tr("Ctrl+Z")));
		_actZoomOut->setShortcut      (QKeySequence(tr("Ctrl+X")));
		_actFullScale->setShortcut    (QKeySequence(tr("F12")));

		///////////////////////////////////////////
		// Menu Window related
		_actHideDock = new QAction(tr("Hide &Dock"), this);
		_actHideDock->setShortcut(QKeySequence(tr("F5")));
		_actHideDock->setCheckable(true);
		_actHideDock->setChecked(false);

		///////////////////////////////////////////
		// Menu Assist related
		_actHideUnselectedNet = new QAction(QIcon(":images/hide_unselected.png"), tr("Hide Unselected Net"), this);
		_actHideUnselectedNet->setCheckable(true);
		_actHideUnselectedNet->setChecked(false);

		_actHideUnHLtedNet = new QAction(QIcon(":images/hide_unhighlighted.png"), tr("Hide Unhighlighted Net"), this);
		_actHideUnHLtedNet->setCheckable(true);
		_actHideUnHLtedNet->setChecked(false);

		_actShowPlacementInfo = new QAction(QIcon(":images/show_place_info.png"), tr("Show Placement Information"), this);
		_actShowPlacementInfo->setCheckable(true);
		_actShowPlacementInfo->setChecked(true);

		_actDrawNet = new QAction(QIcon(":images/draw_net.png"), tr("Draw Net"), this);
		_actDrawNet->setCheckable(true);
		_actDrawNet->setChecked(false);

		_actShowTimeInfo = new QAction(QIcon(":images/show_timing_info.png"), tr("Show Timing Information"), this);
		_actShowTimeInfo->setCheckable(true);
		_actShowTimeInfo->setChecked(false);

		///////////////////////////////////////////
		// Menu Help related
		_actAbout = new QAction(QIcon(":images/about.png"), tr("&About"), this);
		_actAbout->setShortcut(QKeySequence::HelpContents);

		_actArgUsage = new QAction(tr("Arguments &Usage"), this);
		_actArgUsage->setShortcut(QKeySequence(tr("F1")));

		///////////////////////////////////////////
		// Menu enable
		_actSave->setEnabled(false);
		_actOpen->setEnabled(false);
		_actRTSave->setEnabled(false);
	}

	void ChipViewer::InitMenu()
	{
		_menuFile = new QMenu(tr("&File"), this);
		_menuFile->addAction(_actLoadArch);
		_menuFile->addAction(_actOpen);
		_menuFile->addAction(_actSave);
		_menuFile->addAction(_actSaveAs);
		_menuFile->addAction(_actRTSave);
		_menuFile->addAction(_actBitGen);
		_menuFile->addAction(_actAddTap);
		_menuFile->addSeparator();
		_menuFile->addAction(_actQuit);

		_menuView = new QMenu(tr("&View"), this);
		_menuView->addAction(_actFullScale);
		_menuView->addAction(_actZoomIn);
		_menuView->addAction(_actZoomOut);
		_menuView->addAction(_actFind);

		_menuWindow = new QMenu(tr("&Window"), this);
		_menuWindow->addAction(_actHideDock);

		_menuAssist = new QMenu(tr("&Assist"), this);
		_menuAssist->addAction(_actHideUnselectedNet);
		_menuAssist->addAction(_actHideUnHLtedNet);
		_menuAssist->addAction(_actShowPlacementInfo);
		_menuAssist->addAction(_actDrawNet);
		_menuAssist->addAction(_actShowTimeInfo);

		_menuHelp = new QMenu(tr("&Help"), this);
		_menuHelp->addAction(_actAbout);
		_menuHelp->addAction(_actArgUsage);

		menuBar()->addMenu(_menuFile);
		menuBar()->addMenu(_menuView);
		menuBar()->addMenu(_menuWindow);
		menuBar()->addMenu(_menuAssist);
		menuBar()->addMenu(_menuHelp);
	}

	void ChipViewer::InitToolBar()
	{
		_fileToolBar = new QToolBar(this);
		_fileToolBar->setAllowedAreas(Qt::AllToolBarAreas);
		_fileToolBar->setOrientation(Qt::Horizontal);
		_fileToolBar->addAction(_actLoadArch);
		_fileToolBar->addAction(_actOpen);
		_fileToolBar->addAction(_actSave);
		_fileToolBar->addAction(_actSaveAs);
		_fileToolBar->addAction(_actRTSave);
		_fileToolBar->addAction(_actBitGen);
		_fileToolBar->addAction(_actAddTap);

		_viewToolBar = new QToolBar(this);
		_viewToolBar->setAllowedAreas(Qt::AllToolBarAreas);
		_viewToolBar->setOrientation(Qt::Horizontal);
		_viewToolBar->addAction(_actFullScale);
		_viewToolBar->addAction(_actZoomIn);
		_viewToolBar->addAction(_actZoomOut);
		_viewToolBar->addAction(_actFind);

		_assistToolBar = new QToolBar(this);
		_assistToolBar->setAllowedAreas(Qt::AllToolBarAreas);
		_assistToolBar->setOrientation(Qt::Horizontal);
		_assistToolBar->addAction(_actHideUnselectedNet);
		_assistToolBar->addAction(_actHideUnHLtedNet);
		_assistToolBar->addAction(_actDrawNet);
		_assistToolBar->addAction(_actShowPlacementInfo);
		_assistToolBar->addAction(_actShowTimeInfo);

		_helpToolBar = new QToolBar(this);
		_helpToolBar->setAllowedAreas(Qt::AllToolBarAreas);
		_helpToolBar->setOrientation(Qt::Horizontal);
		_helpToolBar->addAction(_actAbout);

		addToolBar(Qt::TopToolBarArea, _fileToolBar);
		addToolBar(Qt::TopToolBarArea, _viewToolBar);
		addToolBar(Qt::TopToolBarArea, _assistToolBar);
		addToolBar(Qt::TopToolBarArea, _helpToolBar);
	}

	void ChipViewer::InitStatusBar()
	{
		QHBoxLayout* layout = new QHBoxLayout(statusBar());
		
		_lbStatus = new QLabel(tr("Ready"));
		_lbStatus->setMinimumWidth(800);

		_lbProgress = new QProgressBar(this);
		_lbProgress->setRange(0, 10);
		_lbProgress->setMaximumWidth(300);

		statusBar()->addWidget(_lbStatus, 1);
		statusBar()->addWidget(_lbProgress, 0);

		show_progressbar();
		hide_progressbar();
	}

	void ChipViewer::InitConnect()
	{
		connect(_actOpen,            SIGNAL(triggered()), this, SLOT(LoadDesign()));
		connect(_actLoadArch,        SIGNAL(triggered()), this, SLOT(LoadArch()));
		connect(_actSave,            SIGNAL(triggered()), this, SLOT(Save()));
		connect(_actSaveAs,          SIGNAL(triggered()), this, SLOT(SaveAs()));
		connect(_actRTSave,			 SIGNAL(triggered()), this, SLOT(RouteAndSave()));
		connect(_actBitGen,			 SIGNAL(triggered()), this, SLOT(BitGen()));
		connect(_actAddTap,			 SIGNAL(triggered()), _viewWidget, SLOT(addSignalTap()));
		connect(_actQuit,            SIGNAL(triggered()), this, SLOT(Quit()));
		connect(_actFullScale,       SIGNAL(triggered()), this, SLOT(FullScale()));
		connect(_actFind,			 SIGNAL(triggered()), this, SLOT(FindNet()));
		connect(_actZoomIn,          SIGNAL(triggered()), this, SLOT(zoom_in()));
		connect(_actZoomOut,         SIGNAL(triggered()), this, SLOT(zoom_out()));
		connect(_actAbout,           SIGNAL(triggered()), this, SLOT(About()));
		connect(_actArgUsage,        SIGNAL(triggered()), this, SLOT(ArgUsage()));
		
		connect(_actHideDock,        SIGNAL(toggled(bool)), this, SLOT(HideDock(bool)));

		connect(_actHideUnselectedNet,	SIGNAL(toggled(bool)), this, SLOT(HideUnselectedLine(bool)));
		connect(_actHideUnHLtedNet,		SIGNAL(toggled(bool)), this, SLOT(HideUnhighlightedLine(bool)));
		connect(_actDrawNet,			SIGNAL(toggled(bool)), this, SLOT(DrawRoute(bool)));
		connect(_actShowPlacementInfo,	SIGNAL(toggled(bool)), this, SLOT(ShowPlacementInfo(bool)));
		connect(_actShowTimeInfo,		SIGNAL(toggled(bool)), this, SLOT(ShowTimeInfo(bool)));

		connect(_viewWidget, SIGNAL(free_design_data()),            this, SLOT(FreeDockDesignData()));
		connect(_viewWidget, SIGNAL(design_is_modified(bool)),      this, SLOT(DesignModified(bool)));
		connect(_viewWidget, SIGNAL(design_route_is_modified(bool)),this, SLOT(DesignRouteModified(bool)));
		connect(_viewWidget, SIGNAL(draw_viewport(QRectF)),         this, SLOT(DrawBirdRect(QRectF)));
		connect(_viewWidget, SIGNAL(emit_draw(bool)),               this, SLOT(DrawRoute(bool)));
		
		connect(_viewWidget, SIGNAL(set_current_net(int)), 	 _netWidget, SLOT(set_selected_route(int)));
		connect(_viewWidget, SIGNAL(show_sta_info(QString)), _netWidget, SLOT(set_sta_info(QString)));
		connect(_viewWidget, SIGNAL(list_highlighted_net(QStringList)), _netWidget, SLOT(set_h_nets_list(QStringList)));
		connect(_viewWidget, SIGNAL(deselect_all_nets()),    _netWidget, SLOT(deselect_all_nets()));

		connect(_viewWidget, SIGNAL(update_tile(const string&)), _subArchWidget, SLOT(updateModule(const string&)));

		connect(_viewWidget, SIGNAL(process_started()),  this, SLOT(disable_file_act()));
		connect(_viewWidget, SIGNAL(process_finished()), this, SLOT(enable_file_act()));
		connect(_netWidget, SIGNAL(select_changed(string, string)),       _viewWidget, SLOT(select_changed(string, string)));
		connect(_netWidget, SIGNAL(rt_list_drag(QListWidgetItem*, bool)), _viewWidget, SLOT(drag_rt_list(QListWidgetItem*, bool)));

		connect(_viewWidget, SIGNAL(add_progress(const string&)), this, SLOT(addProgressbar(const string&)));
		connect(_viewWidget, SIGNAL(file_is_loaded()),            this, SLOT(hide_progressbar()));
		connect(_viewWidget, SIGNAL(file_is_loading()),           this, SLOT(show_progressbar()));
	}

	void ChipViewer::About()
	{
		QMessageBox::about(this, tr("About Chip Viewer"), 
			tr("<h2>Chip Viewer 1.1</h2>"
			"<p>Copyright &copy; 2011-2019 Fudan University"
			"<p>Chip Viewer is a application which "
			"demonstrates the result of placement and routing."));
	}

	void ChipViewer::LoadArch()
	{
		QString file_name = QFileDialog::getOpenFileName(this, tr("Open ARCH file"),
			".", tr("ARCH file(*.xml)\n"
			"All (*.*)"));
		if (!file_name.isEmpty()) {
			_arch_file_name = file_name;
			if ( LoadArch(file_name.toStdString()) ) {
				_actOpen->setEnabled(true);
				setStatus(QString("Ready\t-\t%1").arg(_arch_file_name));
			}
		}
	}

	void ChipViewer::LoadDesign()
	{
		QString file_name = QFileDialog::getOpenFileName(this, tr("Open PR file"),
			".", tr("PR file(*.xml)\n"
			"All (*.*)"));

//		_assist_widget->init_check();
		if (!file_name.isEmpty()) {
			_design_file_name = file_name;
			if ( LoadDesign(file_name.toStdString()) ) {
				setWindowTitle("Chip Viewer - " + file_name);
				setStatus(QString("Ready\t-\t%1").arg(_arch_file_name));
			}
		}
	}

	bool ChipViewer::LoadArch(const std::string &file_name)
	{
		show_progressbar();

//		_assist_widget->init_check();
		QString error_msg = "An error occurred during loading arch file.\nDetails:\n";
		setCursor(Qt::WaitCursor);
		//try
		//{
			addProgressbar("Loading arch file...");
			_viewWidget->load_arch(file_name);
		//}
		/*catch (std::runtime_error &e)
		{
			QMessageBox::warning(this, "Chip Viewer",
				error_msg + QString::fromStdString(e.what()));
			unsetCursor();
			return false;
		}
		catch (...)
		{
			QMessageBox::warning(this, "Chip Viewer",
				"Unknown error occurred.");
			unsetCursor();
			return false;
		}*/

		setWindowTitle("Chip Viewer");
		unsetCursor();
		return true;
	}

	bool ChipViewer::LoadDesign(const std::string &file_name)
	{
		if (!_viewWidget->arch_is_ready()) return false;

		show_progressbar();

		QString error_msg = "An error occurred during loading design file.\nDetails:\n";
		setCursor(Qt::WaitCursor);
		try
		{
			addProgressbar("Freeing design...");
			_viewWidget->free_design();
			
			addProgressbar("Loading design file...");
			_viewWidget->load_design(file_name);
		}
		catch (std::runtime_error &e)
		{
			QMessageBox::warning(this, "Chip Viewer",
				error_msg + QString::fromStdString(e.what()));
			unsetCursor();
			return false;
		}
		catch (...)
		{
			QMessageBox::warning(this, "Chip Viewer",
				"Unknown error occurred.");
			unsetCursor();
			return false;
		}
		
		unsetCursor();
		return true;
	}

	void ChipViewer::Save()
	{
		auto& file_path = _viewWidget->design_path();

		if (!file_path.isEmpty())
			_viewWidget->save_placement(file_path);
	}

	void ChipViewer::SaveAs()
	{
		QString file_path = QFileDialog::getSaveFileName(this, 
			"Save placement modification", ".", "XML files (*.xml)");

		if (!file_path.isEmpty())
			_viewWidget->save_placement(file_path);
	}

	void ChipViewer::RouteAndSave()
	{
		QString file_path = QFileDialog::getSaveFileName(this, 
			"Save routed placement modification", ".", "XML files (*.xml)");

		//QString file_path(QString::fromStdString(_viewWidget->design_path()));

		if (!file_path.isEmpty())
			_viewWidget->save_rt_placement(file_path);
	}

	void ChipViewer::BitGen()
	{
		QString cil_path = QFileDialog::getOpenFileName(this,
			"Choose the CIL file", ".", "XML files (*.xml)");
		if (!cil_path.isEmpty())
			_viewWidget->bit_generate(cil_path);
	}

	void ChipViewer::DesignModified(bool modified)
	{
		setWindowTitle("Chip Viewer - " + _viewWidget->design_path() + (modified ? "*" : ""));
		_actSave->setEnabled(modified);
	}

	void ChipViewer::DesignRouteModified(bool modified)
	{
		setWindowTitle("Chip Viewer - " + _viewWidget->design_path() + (modified ? "*" : ""));
		_actRTSave->setEnabled(modified);
	}

	void ChipViewer::Quit()
	{
		if (_viewWidget->design_modified()) {
			int response = QMessageBox::warning(this, tr("Chip Viewer"), 
				tr("The placement has been modified.\nDo you want to save your changes?"),
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

			if (response == QMessageBox::Cancel) return;
			else if (response == QMessageBox::Yes) Save();
		}

		close();
	}

	void ChipViewer::DrawBirdRect(QRectF bird_rect)
	{
		if (_viewWidget->arch_is_ready())
			_birdWidget->draw_viewport_rect(bird_rect);
	}

	void ChipViewer::HideDock(bool hide)
	{
		_birdDock->setVisible(!hide);
		_netDock->setVisible(!hide);
		_logDock->setVisible(!hide);
	}

	void ChipViewer::DrawRoute(bool draw)
	{
		setCursor(Qt::WaitCursor);

		setStatus(QString("%1 routing...").arg(draw ? "Draw": "Delete"));
		_viewWidget->draw_route(draw);
		setStatus(QString("Ready\t-\t%1").arg(_arch_file_name));
		_netWidget->set_nets_list(_viewWidget->creat_route_list());

		unsetCursor();
	}

	void ChipViewer::ShowTimeInfo(bool show)
	{
		Route::SHOW_TIME_INFO = show;

		//////////////////////////////////////////////////////////////////////////
		// update STA info show
		_viewWidget->select_route_by_name(_netWidget->current_net().toStdString(), true);
	}

	void ChipViewer::FreeDockDesignData()
	{
		_netWidget->set_nets_list(QStringList{});
		_netWidget->set_sta_info("");
	}

	void ChipViewer::ShowPlacementInfo(bool show)
	{
		Route::HIDE_PLACE_INFO = !show;
		_viewWidget->update_view();
	}

	void ChipViewer::addProgressbar(const QString& output, short progress)
	{
		addOutput(output);
		_lbProgress->setValue(_lbProgress->value() + progress);
	}

	void ChipViewer::hide_progressbar()
	{
		_lbProgress->setVisible(false);
	}

	void ChipViewer::show_progressbar()
	{
		_lbProgress->setVisible(true);
		_lbProgress->setValue(0);
	}

	void ChipViewer::disable_file_act()
	{
		_actOpen->setEnabled(false);
		_actSave->setEnabled(false);
		_actOpen->setEnabled(false);
		_actRTSave->setEnabled(false);
		_actBitGen->setEnabled(false);
	}

	void ChipViewer::enable_file_act()
	{
		_actOpen->setEnabled(true);
		_actSave->setEnabled(true);
		_actOpen->setEnabled(true);
		_actRTSave->setEnabled(true);
		_actBitGen->setEnabled(true);
	}

	void ChipViewer::ArgUsage()
	{
		QMessageBox::about(this, tr("Arguments Usage"), 
			tr("--help, -h:\tpresent help usage\n"
			   "--arch, -a:\tset arch file path\n"
			   "--design, -d:\tset design file path\n"));
	}

	void ChipViewer::FindNet()
	{
		bool ok;
		QString text =  QInputDialog::getText(this, tr("QInputDialog::getText()"),
												tr("Find Net:"), QLineEdit::Normal,
												_netWidget->find_net_name(), &ok);
		if (ok && !text.isEmpty())
			_netWidget->find_net(text);
	}
}
