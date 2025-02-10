#ifndef VIEWER_H
#define VIEWER_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <QString>
#include <QProgressBar>

#include "log.h"

#include "ViewWidget.h"
#include "BirdWidget.h"
#include "NetWidget.h"
#include "SubArchWidget.h"


namespace FPGAViewer {

	using namespace COS;
	using namespace ARCH;

	struct Config 
	{
		double	tile_space_h;
		double	tile_space_v;
		double	scene_margin;

		double	cluster_side_length;
		double	cluster_offset;
		double	cluster_space;
	};

	class ChipViewer : public QMainWindow
	{
		Q_OBJECT

	public:
		explicit ChipViewer(QWidget* = 0);
		bool LoadArch(const string& file_name);
		bool LoadDesign(const string& file_name);

	private:
		void InitAction();
		void InitMenu();
		void InitToolBar();
		void InitStatusBar();
		void InitConnect();

		void initDock();
		void initView();

	private slots:
		void LoadArch();
		void LoadDesign();
		void Quit();
		void About();
		void ArgUsage();
		void FindNet();
		void FullScale()               { _viewWidget->full_scale(); }

		void zoom_in()                 { _viewWidget->zoom_in(_netWidget->current_net()); }
		void zoom_out()                { _viewWidget->zoom_out(_netWidget->current_net()); }

		void Save();
		void SaveAs();
		void RouteAndSave();
		void BitGen();
		void DesignModified(bool);
		void DesignRouteModified(bool);
		void DrawBirdRect(QRectF);
		void HideDock(bool);
		void HideUnselectedLine(bool hide)      { _viewWidget->hide_unselected_line(hide); }
		void HideUnhighlightedLine(bool hide)	{ _viewWidget->hide_unhighlighted_line(hide); }
		void DrawRoute(bool draw);
		void ShowPlacementInfo(bool);
		void ShowTimeInfo(bool);
		void FreeDockDesignData();

		void addProgressbar(const QString& output, short progress = 1);
		void addOutput(const QString& output)   { _logText->append("\n" + output); }
		void setStatus(const QString& status)	{ _lbStatus->setText(status); }
		void show_progressbar();
		void hide_progressbar();

		void disable_file_act();
		void enable_file_act();
		
	private:

		ViewWidget*		_viewWidget;
		SubArchWidget*	_subArchWidget;

		QString			_arch_file_name;
		QString			_design_file_name;

		QTextEdit*		_logText;
		QLabel*			_lbStatus;
		QProgressBar*	_lbProgress;

		//////////////////////////////////////////////////////////////////////////
		// for assist & bird view
		BirdWidget*		_birdWidget;
		NetWidget*		_netWidget;

		QDockWidget*	_birdDock;
		QDockWidget*	_netDock;
		QDockWidget*	_logDock;
		
		QMenu* _menuFile;
		QMenu* _menuView;
		QMenu* _menuWindow;
		QMenu* _menuAssist;
		QMenu* _menuHelp;

		QToolBar* _fileToolBar;
		QToolBar* _viewToolBar;
		QToolBar* _windowToolBar;
		QToolBar* _helpToolBar;
		QToolBar* _assistToolBar;

		//actions in menu_F
		QAction* _actOpen;
		QAction* _actLoadArch;
		QAction* _actSave;
		QAction* _actSaveAs;
		QAction* _actRTSave;
		QAction* _actBitGen;
		QAction* _actQuit;
		QAction* _actAddTap;

		//actions in menu_V
		QAction* _actZoomIn;
		QAction* _actZoomOut;
		QAction* _actFullScale;
		QAction* _actFind;

		//actions in menu_W
		QAction* _actHideDock;

		//action in menu_A
		QAction* _actHideUnselectedNet;
		QAction* _actHideUnHLtedNet;
		QAction* _actDrawNet;
		QAction* _actShowPlacementInfo;
		QAction* _actShowTimeInfo;

		//action in menu_H
		QAction* _actAbout;
		QAction* _actArgUsage;

		Config	_conf;
	};

}
#endif // VIEWER_H