#ifndef SUBARCH_WIDGET_H
#define SUBARCH_WIDGET_H

#include <QtGui>
#include <QtCore>
#include <QtWidgets>
#include <list>

#include "netlist.hpp"
#include "arch/archlib.hpp"
#include "matrix.h"
#include "GraphicItem.h"

namespace FPGAViewer {

	using namespace std;
	using namespace ARCH;
	using namespace COS;
	using namespace GraphicItemLib;
	using std::vector;

//	extern const ARCH::FPGADesign*	device;
//	extern COS::Design*			design;

///////////////////////////////////////////////////////////////////////////////
// SubArchView

	class SubArchView : public QGraphicsView
	{
		Q_OBJECT
			
	public:
		static const qreal MARGIN;
		static const qreal V_SPACE;

		SubArchView(QGraphicsScene *scene, QWidget *parent = 0);

		Primitive* centerPrimitive() const	{ return _centerPrimitive; }

	signals:
		void	showInnerStructTriggered(bool);
		void	instanceDoubleClicked(QGraphicsItem *);
		void	netClicked(COS::Net *);

	protected:
		void	contextMenuEvent(QContextMenuEvent* event);
		void	wheelEvent(QWheelEvent * event);
		void	mouseDoubleClickEvent(QMouseEvent * event);
		void	mousePressEvent(QMouseEvent * event);

	private:
		QMenu*		_cmenu;
		QAction*	_showHierachyAction;

		Primitive*	_centerPrimitive;
	};

///////////////////////////////////////////////////////////////////////////////
// SubArchWidget

	class SubArchWidget : public QWidget
	{
		Q_OBJECT
		
	public:
		static const int LEVEL_UNSET;
		SubArchWidget(QWidget *parent = 0);

		~SubArchWidget() {};

	public slots:
		void	updateModule(const string& tileName);
		void	updateModule(const Instance* instance);
		void	updateNet(COS::Net* net);

		void	resetInstance(bool reset = true);
		void	resetInstance(Instance* inst);
		void	focusInstance(Instance* inst);
		void	focusInstance(Pin* pin);
		void	updateInstance(QGraphicsItem* item);
		void	filter(QString netName);

		void	onNetCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
		void	onInstCurrentChanged(const QModelIndex& current, const QModelIndex& previous);

	private:
		void	cleanAll();
		void	clearItems();
		void	clearModels();
		void	fitView();

		void	cleanView();

		void	manualArrangeTileNets(Primitive* prim);

	private:
		enum MyRoles {
			NetlistRole = Qt::UserRole + 1
		};

		SubArchView*			_view;
		QGraphicsScene*			_scene;

		QLineEdit*				_searchNetText;
		
		QTreeView*				_netList;
		QTreeView*				_instList;
		
		QTextEdit*				_infoText;

		QSortFilterProxyModel*	_netProxy;
		QStandardItemModel*		_netModel;

		QSortFilterProxyModel*	_instProxy;
		QStandardItemModel*		_instModel;

		map<int, vector<QGraphicsItem*>>	_levelItemsMap;
		map<Object*, QGraphicsItem*>		_objPrimMap;
		map<Net*, PrimitiveNet*>			_netPnetMap;
		map<Instance*, Point>				_instancePosMap;
	};
	
///////////////////////////////////////////////////////////////////////////////
// Router

/*	class Router
	{
	public:
		static int ucLeftEdgeRoute(list<PrimitiveNet *>& pnets, int leftOrig);
	};*/
}

#endif