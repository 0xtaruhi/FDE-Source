#include "SubArchWidget.h"

namespace FPGAViewer {

	const qreal SubArchView::MARGIN = 20.;
	const qreal SubArchView::V_SPACE = 5.;

///////////////////////////////////////////////////////////////////////////////
// SubArchView

	SubArchView::SubArchView(QGraphicsScene* scene, QWidget* parent)
		: QGraphicsView(scene, parent)
	{
		_showHierachyAction = new QAction(tr("Show inner structure..."), this);
		
		_cmenu = new QMenu(this);
		_cmenu->addAction(_showHierachyAction);
		_showHierachyAction->setEnabled(false);

		connect(_showHierachyAction,	SIGNAL(triggered(bool)),
			this,	SIGNAL(showInnerStructTriggered(bool)));
	}

	void SubArchView::contextMenuEvent(QContextMenuEvent* event)
	{
		QGraphicsItem* item = scene()->itemAt(mapToScene(event->pos()), QTransform());
		if (!item) return;

		if ((65536 < item->type()) && (item->type() < 65546)) {
			Primitive* centerPrimitive = qgraphicsitem_cast<Primitive *>(item);

			if (centerPrimitive->primitive()->down_module()->num_instances()) {
				_centerPrimitive = centerPrimitive;
				_showHierachyAction->setEnabled(true);
			} else {
				_showHierachyAction->setEnabled(false);
			}
			_cmenu->exec(event->globalPos());
		}
		event->accept();
	}

	void SubArchView::mouseDoubleClickEvent(QMouseEvent *event)
	{
		QGraphicsItem* item = scene()->itemAt(mapToScene(event->pos()), QTransform());
		if (!item) return;

		switch (item->type()) {
			case Primitive::Type:
			case GRM::Type:
			case SliceBlock::Type:
			case ModulePin::Type:
				emit instanceDoubleClicked(item);
		}
		event->accept();
	}

	void SubArchView::mousePressEvent(QMouseEvent *event)
	{
		scene()->update(sceneRect());

		QGraphicsItem* item = scene()->itemAt(mapToScene(event->pos()), QTransform());
		if (!item) return;

		switch (item->type()) {
			case PrimitiveNet::Type:	
				emit netClicked(qgraphicsitem_cast<PrimitiveNet *>(item)->net());
				break;
			case Primitive::Type:
			case GRM::Type:
			case SliceBlock::Type:
				_centerPrimitive = qgraphicsitem_cast<Primitive *>(item);
		}
		
	}
	
	void SubArchView::wheelEvent(QWheelEvent *event)
	{
		setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		if (event->delta() > 0) 
			scale(2., 2.);
		else 
			scale(0.5, 0.5);
		event->accept();
		update();
	}

///////////////////////////////////////////////////////////////////////////////
// SubArchWidget

	SubArchWidget::SubArchWidget(QWidget *parent)
		: QWidget(parent)
	{
		_scene = new QGraphicsScene(this);
		_scene->setBackgroundBrush(Qt::black);
		_view	= new SubArchView(_scene, this);
		_view->ensureVisible(QRectF(0, 0, 0, 0));
		_view->show();
		QSizePolicy policy = _view->sizePolicy();
		policy.setHorizontalPolicy(QSizePolicy::Expanding);
		_view->setSizePolicy(policy);

		_searchNetText = new QLineEdit();
		_searchNetText->setPlaceholderText(QString("Look for Net..."));

		_infoText = new QTextEdit;
		_infoText->setFont(QFont("Consolas"));
		_infoText->setTextInteractionFlags(Qt::TextSelectableByMouse);

		_netProxy = new QSortFilterProxyModel;
		_netModel = new QStandardItemModel;
		_netProxy->setSourceModel(_netModel);
		_netModel->setHeaderData(0, Qt::Horizontal, "Nets");

		_netList = new QTreeView;
		_netList->setModel(_netProxy);
		_netList->setAlternatingRowColors(true);

		_instProxy = new QSortFilterProxyModel;
		_instModel = new QStandardItemModel;
		_instProxy->setSourceModel(_instModel);
		_instModel->setHeaderData(0, Qt::Horizontal, "Instances");
		_instModel->setHeaderData(1, Qt::Horizontal, "Pins");

		_instList = new QTreeView;
		_instList->setModel(_instProxy);
		_instList->setAlternatingRowColors(true);

		QTabWidget* tabWidget = new QTabWidget;
		tabWidget->addTab(_instList, "Instance Information");
		tabWidget->addTab(_netList, "Nets Information");

		QSplitter* leftSplitter= new QSplitter(Qt::Vertical);
		leftSplitter->addWidget(tabWidget);
		leftSplitter->addWidget(_infoText);

		QVBoxLayout* vlayout = new QVBoxLayout;
		vlayout->addWidget(_searchNetText);
		vlayout->addWidget(leftSplitter);

		QWidget* leftWidget = new QWidget;
		leftWidget->setLayout(vlayout);
		leftWidget->resize(_netList->sizeHint().width(), tabWidget->sizeHint().height());
		policy = leftWidget->sizePolicy();
		policy.setHorizontalPolicy(QSizePolicy::Maximum);
		leftWidget->setSizePolicy(policy);

		QSplitter* splitter= new QSplitter;
		splitter->addWidget(leftWidget);
		splitter->addWidget(_view);

		QVBoxLayout* layout = new QVBoxLayout;
		layout->addWidget(splitter);
		setLayout(layout);

		connect(_view,	SIGNAL(netClicked(COS::Net *)),
			this,	SLOT(updateNet(COS::Net *)));
		connect(_view,	SIGNAL(showInnerStructTriggered(bool)),
			this,	SLOT(resetInstance(bool)));
		connect(_view,	SIGNAL(instanceDoubleClicked(QGraphicsItem *)),
			this,	SLOT(updateInstance(QGraphicsItem *)));

		connect(_searchNetText,	SIGNAL(textChanged(QString)), 
			this,	SLOT(filter(QString)));
		connect(_netList->selectionModel(),		SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)),
			this,	SLOT(onNetCurrentChanged(const QModelIndex&, const QModelIndex&)));
		connect(_instList->selectionModel(),	SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)),
			this,	SLOT(onInstCurrentChanged(const QModelIndex&, const QModelIndex&)));
		
		resize(maximumSize());
	}

	void SubArchWidget::updateModule(const Instance* instance)
	{
		cleanAll();

		Module* mod = instance->down_module();
		setWindowTitle(QString("%1 (%2)")
			.arg(QString::fromStdString(instance->name()))
			.arg(QString::fromStdString(mod->name())));

		qreal vOffset = SubArchView::MARGIN;
		Point instancePos;
		instancePos.z = 0;

		for (Net* net: mod->nets()) {
			PrimitiveNet* pnet = new PrimitiveNet(net);

			_scene->addItem(pnet);

			QStandardItem* netItem = new QStandardItem(QString::fromStdString(net->name()));
			netItem->setData(QVariant::fromValue((void*)net), NetlistRole);
			_netModel->appendRow(netItem);

			_netPnetMap[net] = pnet;
		}

		for (Instance* inst: mod->instances()) {
			Primitive* prim = GFactory::makePrimitive(inst);

			if ("GRM" == inst->down_module()->type() || "GSB" == inst->down_module()->type()) {
				instancePos.x = SubArchView::MARGIN + 3 * Primitive::MIN_WIDTH;
				instancePos.y = SubArchView::MARGIN;
				//prim->setPos(SubArchView::MARGIN + 3 * Primitive::MIN_WIDTH, SubArchView::MARGIN);
			} else {
				instancePos.x = SubArchView::MARGIN;
				instancePos.y = vOffset;
				//prim->setPos(SubArchView::MARGIN, vOffset);
				vOffset += prim->boundingRect().height() * 2;
			}
			prim->setPos(instancePos.x, instancePos.y);
			_instancePosMap[inst] = instancePos;
			_scene->addItem(prim);

			QStandardItem* instItem = new QStandardItem(QString::fromStdString(inst->name()));
			instItem->setData(QVariant::fromValue((void*)inst), NetlistRole);
			_instModel->appendRow(instItem);

			for (Pin* pin: inst->pins()) {
				QStandardItem* pinItem = new QStandardItem(QString::fromStdString(pin->name()));
				pinItem->setData(QVariant::fromValue((void*)pin), NetlistRole);
				instItem->appendRow(pinItem);
			}

			_objPrimMap[inst] = prim;
		}

		for (map<Object*, QGraphicsItem *>::const_iterator iter = _objPrimMap.begin();
			iter != _objPrimMap.end(); ++iter) {
			Primitive* prim = dynamic_cast<Primitive*>(iter->second);
			if (prim->primitive()->down_module()->type() == "GRM" || prim->primitive()->down_module()->type() == "GSB") continue;
			manualArrangeTileNets(prim);
		}

		show();
		fitView();
	}

	void SubArchWidget::manualArrangeTileNets(Primitive* prim)
	{
		FDU_LOG(DEBUG) << "prim: " << prim->primitive()->name();
		for (QGraphicsItem* item: prim->childItems()) {
			if (item->type() != PrimitivePin::Type) continue;

			PrimitivePin* ppin = dynamic_cast<PrimitivePin *>(item);

			if (_netPnetMap.find(ppin->pin()->net()) == _netPnetMap.end())	continue;
			PrimitiveNet* pnet = _netPnetMap[ppin->pin()->net()];
			pnet->clearCorner();
			pnet->addTerminal(ppin);

			for (Pin* pin: pnet->net()->pins()) {
				if (pin == ppin->pin()) continue;

				if (_objPrimMap.find(pin->instance()) == _objPrimMap.end())	continue;
				//if (_objPrimMap[pin->instance()]->type() != GRM::Type)
				//	continue;

				PrimitivePin* connectedPpin = dynamic_cast<Primitive *>(_objPrimMap[pin->instance()])->findPin(pin);
				pnet->addTerminal(connectedPpin);
				// move it to the same y as another original ppin
				qreal newY = connectedPpin->parentItem()->mapFromScene(0, ppin->mapToScene(0, 0).y()).y();

				if (ppin->side() != PrimitivePin::LEFT) {
					connectedPpin->setPos(QPointF(connectedPpin->x(), newY));
				} else {
					connectedPpin->setPos(QPointF(connectedPpin->x(), newY + ppin->parentItem()->boundingRect().height()));
				}
			}
			if (ppin->side() == PrimitivePin::LEFT)	pnet->addCorner(QPoint(ppin->mapToScene(0, 0).x() - ppin->sideIndex() * 2 - 6, 0));
			pnet->updateShape();
		}
	}

	void SubArchWidget::updateModule(const string& tileName)
	{
		const Library* archlib = ARCH::FPGADesign::instance()->find_library("arch");
		ASSERT(archlib, "Failed to find Library named \"arch\"");

		for (const Module* mod: archlib->modules()) {
			// only get the first module, actually, and then return
			// which is the fpga module
			
			if (mod->type() != "fpga") continue; 
			const Instance* tile = mod->find_instance(tileName);
			ASSERT(tile, "Failed to find tile " << tileName);

			if (tile->down_module()->library()->name() == "tile")
				updateModule(tile);
			else 
				resetInstance(tile);
			return;
		}
	}

	void SubArchWidget::resetInstance(bool reset)
	{
		resetInstance(_view->centerPrimitive()->primitive());
	}

	void SubArchWidget::resetInstance(Instance* inst) {
		cleanView();

		Module* mod = inst->down_module();

		QStandardItem* modItem = new QStandardItem(QString::fromStdString(mod->name()));
		modItem->setData(QVariant::fromValue((void*)mod), NetlistRole);
		_instModel->appendRow(modItem);

		for (Instance* inst: mod->instances()) {
			QStandardItem* instItem = new QStandardItem(QString::fromStdString(inst->name()));
			instItem->setData(QVariant::fromValue((void*)inst), NetlistRole);
			modItem->appendRow(instItem);

			for (Pin* pin: inst->pins()) {
				QStandardItem* pinItem = new QStandardItem(QString::fromStdString(pin->name()));
				pinItem->setData(QVariant::fromValue((void*)pin), NetlistRole);
				instItem->appendRow(pinItem);
			}
		}

		for (Pin* pin : mod->pins()) {
			QStandardItem* pinItem = new QStandardItem(QString("[MPIN] ") + QString::fromStdString(pin->name()));
			pinItem->setData(QVariant::fromValue((void*)pin), NetlistRole);
			modItem->appendRow(pinItem);
		}

		for (Net* net : mod->nets()) {
			QStandardItem* netItem = new QStandardItem(QString::fromStdString(net->name()));
			netItem->setData(QVariant::fromValue((void*)net), NetlistRole);
			_netModel->appendRow(netItem);
		}

		return;
	}

	void SubArchWidget::focusInstance(Instance* inst)
	{
		clearItems();

		Primitive* prim = GFactory::makePrimitive(inst);
		prim->setLevel(0);
		_levelItemsMap[0].push_back(prim);
		_objPrimMap[inst] = prim;

		_scene->addItem(prim);

		fitView();
	}

	void SubArchWidget::focusInstance(Pin* pin)
	{
		clearItems();

		ModulePin* mpin = new ModulePin(pin);
		mpin->setLevel(0);
		_levelItemsMap[0].push_back(mpin);
		_objPrimMap[pin] = mpin;

		_scene->addItem(mpin);

		fitView();
	}

	void SubArchWidget::updateInstance(QGraphicsItem *item)
	{
		//int level;
		int levelOffset;
		Point pos;
		Primitive* prim;
		Instance* inst;
		Module* mod = NULL;
		vector<Primitive*> relPrims;
		map<int, vector<ModulePin*>> _mpinsMap;
		map<ModulePin*, PrimitivePin*> _mpincMap;
		switch (item->type()) {
			case Primitive::Type:
			case GRM::Type:
			case SliceBlock::Type:
				prim = qgraphicsitem_cast<Primitive*>(item);
				//level = prim->level();
				mod = prim->primitive()->down_module();
				relPrims.push_back(prim);
				break;
			case ModulePin::Type:
				//level = qgraphicsitem_cast<ModulePin *>(item)->level();
				break;
			default:
				FDU_LOG(WARN) << "Type doesn't comply. Instance update will not continue.";
				return;
		}

		// find new pres and sucs
		for (QGraphicsItem* item: item->childItems()) {
			if (item->type() != PrimitivePin::Type) continue;

			PrimitivePin* ppin = qgraphicsitem_cast<PrimitivePin *>(item);

			Pin* pin = ppin->pin();
			bool isFanout = (ppin->side() == PrimitivePin::RIGHT);
			bool isToAdd = false;

			PrimitiveNet* pnet;
			if (_netPnetMap.find(pin->net()) == _netPnetMap.end()) {
				pnet = new PrimitiveNet(pin->net());
			} else {
				pnet = _netPnetMap[pin->net()];
			}
			
			for (Pin* connectedPin: pin->net()->pins()) {
				if (connectedPin == ppin->pin()) continue;

				if (connectedPin->is_mpin()) {
					if (_objPrimMap.find(connectedPin) != _objPrimMap.end()) {
						pnet->addTerminal(dynamic_cast<ModulePin *>(_objPrimMap[connectedPin])->pin());
						isToAdd = true;
						// TODO: gap and loop
						continue;
					}

					ModulePin* mpin = new ModulePin(connectedPin);
					if ((isFanout && (mpin->dir() == ModulePin::OUTPUT)) ||	(!isFanout && (mpin->dir() == ModulePin::INPUT)) ) {
						delete mpin;
						continue;
					}
					_scene->addItem(mpin);

					levelOffset = isFanout ? 1 : -1;
					isToAdd = true;

					//_levelItemsMap[level + levelOffset].push_back(mpin);
					_mpinsMap[levelOffset].push_back(mpin);
					_mpincMap[mpin] = ppin;
					//mpin->setLevel(level + levelOffset);
					_objPrimMap[connectedPin] = mpin;

					pnet->addTerminal(mpin->pin());

					continue;
				}

				if (_objPrimMap.find(connectedPin->instance()) != _objPrimMap.end()) {
					PrimitivePin* ppinToAdd = dynamic_cast<Primitive *>(_objPrimMap[connectedPin->instance()])->findPin(connectedPin);
					
					pnet->addTerminal(ppinToAdd);
					isToAdd = true;
					// TODO: process gap and loop -- instance already created.
					continue;
				}

				prim = GFactory::makePrimitive(connectedPin->instance());
				PrimitivePin* ppinToAdd = prim->findPin(connectedPin);

				if ((isFanout && (ppinToAdd->side() == PrimitivePin::RIGHT)) ||	(!isFanout && (ppinToAdd->side() != PrimitivePin::RIGHT)) ) {
					delete prim;
					continue;
				}

				//int levelOffset = isFanout ? 1 : -1;
				isToAdd = true;

				_scene->addItem(prim);
				relPrims.push_back(prim);

				//_levelItemsMap[level + levelOffset].push_back(prim);
				//prim->setLevel(level + levelOffset);
				_objPrimMap[connectedPin->instance()] = prim;

				pnet->addTerminal(ppinToAdd);
			}

			if (isToAdd) {
				_scene->addItem(pnet);
				_netPnetMap[pin->net()] = pnet;
				pnet->addTerminal(ppin);
			} else 
				delete pnet;
		}

		vector<Primitive*>::iterator it;
		for (it = relPrims.begin(); it != relPrims.end(); ++it) {
			prim = *it;
			inst = prim->primitive();
			pos = _instancePosMap[inst];
			prim->setPos(pos.x, pos.y);
		}

		for (int i = -1; i <= 1; i += 2) {
			pos.x = (i < 0) ? 0 : (SubArchView::MARGIN + 6 * Primitive::MIN_WIDTH);
			for (ModulePin* mpin : _mpinsMap[i]) {
				pos.y = _instancePosMap[_mpincMap[mpin]->pin()->instance()].y + _mpincMap[mpin]->y();
				mpin->setPos(pos.x, pos.y);
			}
		}		

		// collect nets to route.
		vector<Primitive*>::reverse_iterator rit;
		for (rit = relPrims.rbegin(); rit != relPrims.rend(); ++rit) {
			prim = *rit;
			if (prim->primitive()->down_module()->type() == "GRM" || prim->primitive()->down_module()->type() == "GSB")	continue;
			for (QGraphicsItem* childItem : prim->childItems()) {
				if (childItem->type() != PrimitivePin::Type) {
					FDU_LOG(WARN) << "Primitive has NON-PRIMITIVEPIN item! Ignored when collecting nets to route";
					continue;
				}

				PrimitivePin* ppin = qgraphicsitem_cast<PrimitivePin*>(childItem);

				if (_netPnetMap.find(ppin->pin()->net()) == _netPnetMap.end())	continue;

				PrimitiveNet* pnet = _netPnetMap[ppin->pin()->net()];
				pnet->clearCorner();
				pnet->addTerminal(ppin);

				for (Pin* pin : pnet->net()->pins()) {
					if (pin == ppin->pin()) continue;

					if (_objPrimMap.find(pin->instance()) == _objPrimMap.end())	continue;

					PrimitivePin* connectedPpin = dynamic_cast<Primitive*>(_objPrimMap[pin->instance()])->findPin(pin);
					pnet->addTerminal(connectedPpin);
					// move it to the same y as another original ppin
					qreal newY = connectedPpin->parentItem()->mapFromScene(0, ppin->mapToScene(0, 0).y()).y();

					if (ppin->side() != PrimitivePin::LEFT) {
						connectedPpin->setPos(QPointF(connectedPpin->x(), newY));
					}
					else {
						connectedPpin->setPos(QPointF(connectedPpin->x(), newY + ppin->parentItem()->boundingRect().height()));
					}
				}
				if (ppin->side() == PrimitivePin::LEFT)	pnet->addCorner(QPoint(ppin->mapToScene(0, 0).x() - ppin->sideIndex() * 2 - 6, 0));
				if (rit == relPrims.rend() - 1)	pnet->updateShape();
			}
		}

		fitView();
	}

	void SubArchWidget::updateNet(COS::Net* net)
	{
		QList<QStandardItem *> selectedNet = 
			_netModel->findItems(QString::fromStdString(net->name()));

		if (!(selectedNet.empty())) {
			QModelIndex index = selectedNet.first()->index();

			if (index.isValid()) {
				QItemSelectionModel* selectModel = _netList->selectionModel();
				_netList->selectionModel()->select(index, 
					QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | 
					QItemSelectionModel::Current);
				_netList->scrollTo(_netProxy->mapFromSource(index), QAbstractItemView::PositionAtTop);
				_netList->setCurrentIndex(_netProxy->mapFromSource(index));
			}
		} 
	}

	void SubArchWidget::filter(QString pattern)
	{
		_netProxy->setFilterWildcard(pattern);
		_instProxy->setFilterWildcard(pattern);
	}

	void SubArchWidget::onNetCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
	{
		if (current.isValid()) {
			Net* net = (Net*)current.data(NetlistRole).value<void*>();

			_infoText->clear();
			QString info;
			info.append(QString("NET NAME:   %1\n")
				.arg(QString::fromStdString(net->name())));

			for (Pin* pin: net->pins()) {
				QString extra;
				if (pin->instance()) 
					extra.append(QString("\n   - of ") + QString::fromStdString(pin->instance()->name()));
				if (pin->is_mpin())
					extra.append(" (Module Pin)");
				info.append(QString(" - PortRef:  %1 %2\n")
					.arg(QString::fromStdString(pin->name()))
					.arg(extra));
			}
			_infoText->setPlainText(info);

			if (_netPnetMap.end() != _netPnetMap.find(net))
				_netPnetMap[net]->setSelected(true);
		}

		if (previous.isValid()) {
			Net* net = (Net*)previous.data(NetlistRole).value<void*>();
			if (_netPnetMap.end() != _netPnetMap.find(net))
				_netPnetMap[net]->setSelected(false);
		}
	}

	void SubArchWidget::onInstCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
	{
		if (current.isValid()) {
			Object* obj = (Object*)current.data(NetlistRole).value<void*>();
			QString info;

			Instance* inst = NULL;
			if (obj->class_id() == COS::PIN) {
				Pin* pin = dynamic_cast<Pin *>(obj);

				info.append(QString("PIN NAME: %1")
					.arg(QString::fromStdString(pin->name())));
				if (pin->is_mpin())
					info.append(" (MODULE PIN)\n");
				else 
					info.append("\n");

				if (pin->net())
					info.append(QString("CONNECTED BY: %1\n")
						.arg(QString::fromStdString(pin->net()->name())));

				inst = pin->instance();
				if (pin->is_mpin())
					focusInstance(pin);
			} else if (obj->class_id() == COS::MODULE) {
				Module* mod = dynamic_cast<Module *>(obj);

				info.append(QString("MODULE NAME: %1\n")
					.arg(QString::fromStdString(mod->name())));

			} else if (obj->class_id() == COS::INSTANCE) {
				inst = dynamic_cast<Instance *>(obj);
				focusInstance(inst);
			}

			if (inst) 
				info.append(QString("INSTANCE NAME: %1\n  MODULE NAME: %2\n         TYPE: %3\n")
					.arg(QString::fromStdString(inst->name()))
					.arg(QString::fromStdString(inst->down_module()->name()))
					.arg(QString::fromStdString(inst->down_module()->type())));

			_infoText->setPlainText(info);
		}

		if (previous.isValid()) {
			// do nothing...
		}
	}

	void SubArchWidget::cleanAll()
	{
		_scene->clear();

		_levelItemsMap.clear();
		_objPrimMap.clear();
		_netPnetMap.clear();
		
		_netModel->clear();
		_instModel->clear();
	}

	void SubArchWidget::cleanView()
	{
		_scene->clear();

		_levelItemsMap.clear();
		_objPrimMap.clear();
		
		_netModel->clear();
		_instModel->clear();
	}

	void SubArchWidget::clearItems()
	{
		_scene->clear();

		_levelItemsMap.clear();
		_objPrimMap.clear();
		_netPnetMap.clear();
	}
	
	void SubArchWidget::clearModels()
	{
		_netModel->clear();
		_instModel->clear();
	}

	void SubArchWidget::fitView()
	{
		_view->setSceneRect(_view->scene()->itemsBoundingRect().adjusted(-100, -100, 100, 100));
		_view->fitInView(_view->sceneRect(), Qt::KeepAspectRatio);	
	}

}