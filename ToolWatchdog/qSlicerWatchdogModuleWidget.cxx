/*==============================================================================

Program: 3D Slicer

Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

==============================================================================*/

// Qt includes
#include <QDebug>
#include <QMenu>
#include <QtGui>

// SlicerQt includes
#include "qSlicerWatchdogModuleWidget.h"
#include "ui_qSlicerWatchdogModuleWidget.h"

#include "vtkMRMLDisplayableNode.h"
#include "vtkSlicerWatchdogLogic.h"
#include "vtkMRMLWatchdogNode.h"

int TOOL_LABEL_COLUMN = 0;
int TOOL_NAME_COLUMN = 1;
int TOOL_SOUND_COLUMN = 2;
int TOOL_TIMESTAMP_COLUMN = 3;
int TOOL_COLUMNS = 4;

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ToolWatchdog
class qSlicerWatchdogModuleWidgetPrivate: public Ui_qSlicerWatchdogModuleWidget
{
  Q_DECLARE_PUBLIC( qSlicerWatchdogModuleWidget ); 

protected:
  qSlicerWatchdogModuleWidget* const q_ptr;
public:
  qSlicerWatchdogModuleWidgetPrivate( qSlicerWatchdogModuleWidget& object );
  vtkSlicerWatchdogLogic* logic() const;

  vtkWeakPointer<vtkMRMLWatchdogNode> WatchdogNode;
};

//-----------------------------------------------------------------------------
// qSlicerWatchdogModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerWatchdogModuleWidgetPrivate::qSlicerWatchdogModuleWidgetPrivate( qSlicerWatchdogModuleWidget& object )
: q_ptr( &object )
{
}

//-----------------------------------------------------------------------------
vtkSlicerWatchdogLogic* qSlicerWatchdogModuleWidgetPrivate::logic() const
{
  Q_Q( const qSlicerWatchdogModuleWidget );
  return vtkSlicerWatchdogLogic::SafeDownCast( q->logic() );
}

//-----------------------------------------------------------------------------
// qSlicerWatchdogModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerWatchdogModuleWidget::qSlicerWatchdogModuleWidget(QWidget* _parent)
: Superclass( _parent )
, d_ptr( new qSlicerWatchdogModuleWidgetPrivate ( *this ) )
{
  this->CurrentCellPosition[0]=0;
  this->CurrentCellPosition[1]=0;
}

//-----------------------------------------------------------------------------
qSlicerWatchdogModuleWidget::~qSlicerWatchdogModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::setup()
{
  Q_D(qSlicerWatchdogModuleWidget);

  d->setupUi(this);
  this->Superclass::setup();

  this->setMRMLScene( d->logic()->GetMRMLScene() );

  connect( d->ModuleNodeComboBox, SIGNAL( currentNodeChanged( vtkMRMLNode* ) ), this, SLOT( onWatchdogNodeSelectionChanged() ) );

  connect( d->ToolComboBox, SIGNAL( currentNodeChanged( vtkMRMLNode* ) ), this, SLOT( updateWidget() ) );

  connect( d->ToolBarVisibilityCheckBox, SIGNAL( toggled(bool) ), this, SLOT( onToolBarVisibilityChanged(bool) ) );
  d->ToolBarVisibilityCheckBox->setChecked(true);

  connect( d->AddToolButton, SIGNAL( clicked() ), this, SLOT( onAddToolNode() ) );
  connect( d->DeleteToolButton, SIGNAL( clicked() ), this, SLOT( onRemoveToolNode()) );
  d->DeleteToolButton->setIcon( QIcon( ":/Icons/MarkupsDelete.png" ) );
  connect( d->UpToolButton, SIGNAL( clicked() ), this, SLOT( onUpButtonClicked()) );
  d->UpToolButton->setIcon( QIcon( ":/Icons/MarkupsMoveUp.png" ) );
  connect( d->DownToolButton, SIGNAL( clicked() ), this, SLOT( onDownButtonClicked()) );
  d->DownToolButton->setIcon( QIcon( ":/Icons/MarkupsMoveDown.png" ) );

  QStringList MarkupsTableHeaders;
  MarkupsTableHeaders << "Label" << "Name" << "Sound" << "Status";
  d->ToolsTableWidget->setColumnCount( TOOL_COLUMNS );
  d->ToolsTableWidget->setHorizontalHeaderLabels( MarkupsTableHeaders );
  d->ToolsTableWidget->horizontalHeader()->setResizeMode( QHeaderView::Stretch );

  connect(d->ToolsTableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem *)), this, SLOT( onTableItemDoubleClicked() ));
  d->ToolsTableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
  connect( d->ToolsTableWidget, SIGNAL( customContextMenuRequested(const QPoint&) ), this, SLOT( onToolsTableContextMenu(const QPoint&) ) );



  this->updateFromMRMLNode();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::enter()
{
  Q_D(qSlicerWatchdogModuleWidget);
  if ( this->mrmlScene() == NULL )
  {
    qCritical() << "Invalid scene!";
    return;
  }

  // Create a module MRML node if there is none in the scene.
  vtkMRMLNode* node = this->mrmlScene()->GetNthNodeByClass(0, "vtkMRMLWatchdogNode");
  if ( node == NULL )
  {
    vtkSmartPointer< vtkMRMLWatchdogNode > newNode = vtkSmartPointer< vtkMRMLWatchdogNode >::New();
    this->mrmlScene()->AddNode( newNode );
  }

  node = this->mrmlScene()->GetNthNodeByClass( 0, "vtkMRMLWatchdogNode" );
  if ( node == NULL )
  {
    qCritical( "Failed to create module node" );
    return;
  }

  // For convenience, select a default module.
  if ( d->ModuleNodeComboBox->currentNode() == NULL )
  {
    d->ModuleNodeComboBox->setCurrentNodeID( node->GetID() );
  }

  this->Superclass::enter();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::setMRMLScene( vtkMRMLScene* scene )
{
  Q_D( qSlicerWatchdogModuleWidget );
  this->Superclass::setMRMLScene( scene );
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onSceneImportedEvent()
{
  this->enter();
  this->updateWidget();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onCurrentCellChanged(int currentRow, int currentColumn)
{
  if(this->CurrentCellPosition[0]!=currentRow && this->CurrentCellPosition[1]!=currentColumn)
  {
    return;
  }
  Q_D( qSlicerWatchdogModuleWidget );
  if ( d->WatchdogNode == NULL )
  {
    qCritical( "Selected node not a valid module node" );
    return;
  }

  std::string label = d->ToolsTableWidget->item(currentRow,currentColumn)->text().toStdString();
  d->WatchdogNode->SetWatchedNodeDisplayLabel(currentRow, label.c_str());
  disconnect( d->ToolsTableWidget, SIGNAL( cellChanged( int , int ) ), this, SLOT( onCurrentCellChanged( int, int ) ) );
  this->updateWidget();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onTableItemDoubleClicked()
{
  Q_D( qSlicerWatchdogModuleWidget );
  if(d->ToolsTableWidget->currentColumn()!=0)
  {
    return;
  }
  this->CurrentCellPosition[0]=d->ToolsTableWidget->currentRow();
  this->CurrentCellPosition[1]=d->ToolsTableWidget->currentColumn();
  connect( d->ToolsTableWidget, SIGNAL( cellChanged( int , int ) ), this, SLOT( onCurrentCellChanged( int , int ) ) );
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onWatchdogNodeSelectionChanged()
{
  Q_D( qSlicerWatchdogModuleWidget );
  vtkMRMLWatchdogNode* selectedWatchdogNode = vtkMRMLWatchdogNode::SafeDownCast( d->ModuleNodeComboBox->currentNode() );
  qvtkReconnect(d->WatchdogNode, selectedWatchdogNode, vtkCommand::ModifiedEvent, this, SLOT(onWatchdogNodeModified()));
  d->WatchdogNode = selectedWatchdogNode;
  //this->onWatchdogNodeModified();
  this->updateFromMRMLNode();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::updateFromMRMLNode()
{
  Q_D( qSlicerWatchdogModuleWidget );
  if ( d->WatchdogNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    return;
  }
  d->ToolComboBox->setEnabled( true );
  updateWidget();
}

//-----------------------------------------------------------------------------
void  qSlicerWatchdogModuleWidget::onDownButtonClicked()
{
  Q_D( qSlicerWatchdogModuleWidget);
  if ( d->WatchdogNode == NULL )
  {
    return;
  }
  int currentTool = d->ToolsTableWidget->currentRow();
  if ( currentTool < d->WatchdogNode->GetNumberOfWatchedNodes()- 1 && currentTool>=0 )
  {
    d->WatchdogNode->SwapWatchedNodes( currentTool, currentTool + 1 );
  }
  updateWidget();
}

//-----------------------------------------------------------------------------
void  qSlicerWatchdogModuleWidget::onUpButtonClicked()
{
  Q_D( qSlicerWatchdogModuleWidget);
  if ( d->WatchdogNode == NULL )
  {
    return;
  }
  int currentTool = d->ToolsTableWidget->currentRow();
  if ( currentTool > 0 )
  {
    d->WatchdogNode->SwapWatchedNodes( currentTool, currentTool - 1 );
  }
  updateWidget();
}

//-----------------------------------------------------------------------------
void  qSlicerWatchdogModuleWidget::onRemoveToolNode()
{
  Q_D( qSlicerWatchdogModuleWidget);

  QItemSelectionModel* selectionModel = d->ToolsTableWidget->selectionModel();
  std::vector< int > deleteNodes;
  // Need to find selected before removing because removing automatically refreshes the table
  for ( int i = 0; i < d->ToolsTableWidget->rowCount(); i++ )
  {
    if ( selectionModel->rowIntersectsSelection( i, d->ToolsTableWidget->rootIndex() ) )
    {
      deleteNodes.push_back( i );
    }
  }
  if ( d->WatchdogNode == NULL )
  {
    return;
  }
  for ( int i = deleteNodes.size() - 1; i >= 0; i-- )
  {
    d->WatchdogNode->RemoveWatchedNode(deleteNodes.at( i ));
  }
  this->updateWidget();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onAddToolNode( )
{
  Q_D(qSlicerWatchdogModuleWidget);
  if ( d->WatchdogNode == NULL )
  {
    return;
  }
  vtkMRMLNode* currentToolNode = d->ToolComboBox->currentNode();
  if ( currentToolNode  == NULL )
  {
    return;
  }
  d->WatchdogNode->AddWatchedNode(currentToolNode);
  this->updateWidget();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onToolsTableContextMenu(const QPoint& position)
{
  Q_D(qSlicerWatchdogModuleWidget);

  QPoint globalPosition = d->ToolsTableWidget->viewport()->mapToGlobal( position );

  QMenu* transformsMenu = new QMenu( d->ToolsTableWidget );
  QAction* activateAction = new QAction( "Make list active", transformsMenu );
  QAction* deleteAction = new QAction( "Delete highlighted row", transformsMenu );
  QAction* upAction = new QAction( "Move current element up", transformsMenu );
  QAction* downAction = new QAction( "Move current row down", transformsMenu );

  transformsMenu->addAction( activateAction );
  transformsMenu->addAction( deleteAction );
  transformsMenu->addAction( upAction );
  transformsMenu->addAction( downAction );

  QAction* selectedAction = transformsMenu->exec( globalPosition );

  if ( d->WatchdogNode == NULL )
  {
    return;
  }

  if ( selectedAction == deleteAction )
  {
    QItemSelectionModel* selectionModel = d->ToolsTableWidget->selectionModel();
    std::vector< int > deleteNodes;
    // Need to find selected before removing because removing automatically refreshes the table
    for ( int i = 0; i < d->ToolsTableWidget->rowCount(); i++ )
    {
      if ( selectionModel->rowIntersectsSelection( i, d->ToolsTableWidget->rootIndex() ) )
      {
        deleteNodes.push_back( i );
      }
    }
    //Traversing this way should be more efficient and correct
    QString toolKey;
    for ( int i = deleteNodes.size() - 1; i >= 0; i-- )
    {
      d->WatchdogNode->RemoveWatchedNode(deleteNodes.at( i ));
    }
  }
  int currentTool = d->ToolsTableWidget->currentRow();
  if ( selectedAction == upAction )
  {
    if ( currentTool > 0 )
    {
      d->WatchdogNode->SwapWatchedNodes( currentTool, currentTool - 1 );
    }
  }

  if ( selectedAction == downAction )
  {
    if ( currentTool < d->WatchdogNode->GetNumberOfWatchedNodes()- 1 )
    {
      d->WatchdogNode->SwapWatchedNodes( currentTool, currentTool + 1 );
    }
  }
  this->updateWidget();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::updateWidget()
{
  Q_D(qSlicerWatchdogModuleWidget);
  if ( d->WatchdogNode == NULL )
  {
    d->ToolComboBox->setCurrentNodeID( "" );
    d->ToolComboBox->setEnabled( false );
    d->ToolsTableWidget->clearContents();
    d->ToolsTableWidget->setRowCount( 0 );
    return;
  }
  vtkMRMLDisplayableNode* currentToolNode = vtkMRMLDisplayableNode::SafeDownCast( d->ToolComboBox->currentNode() );
  d->AddToolButton->blockSignals( true );
  if ( currentToolNode == NULL )
  {
    d->AddToolButton->setChecked( Qt::Unchecked );
    // This will ensure that we refresh the widget next time we move to a non-null widget (since there is guaranteed to be a modified status of larger than zero)
    //return;
    d->AddToolButton->setEnabled(false);
  }
  else
  {
    // Only allow adding the selected node if not watched already
    if (d->WatchdogNode->GetWatchedNodeIndex(currentToolNode)<0)
    {
      d->AddToolButton->setEnabled(true);
    }
    else
    {
      d->AddToolButton->setEnabled(false);
    }
  }
  d->AddToolButton->blockSignals( false );

  // Update the fiducials table
  d->ToolsTableWidget->blockSignals( true );

  int numberOfWatchedNodes = d->WatchdogNode->GetNumberOfWatchedNodes();
  if (numberOfWatchedNodes > d->ToolsTableWidget->rowCount())
  {
    // Add lines to the table
    int rowStartIndex = d->ToolsTableWidget->rowCount();
    d->ToolsTableWidget->setRowCount( numberOfWatchedNodes );
    for (int rowIndex = rowStartIndex; rowIndex < numberOfWatchedNodes; rowIndex++)
    {
      QTableWidgetItem* nameItem = new QTableWidgetItem();
      QTableWidgetItem* labelItem = new QTableWidgetItem();
      QTableWidgetItem* lastElapsedTimeStatus = new QTableWidgetItem();
      lastElapsedTimeStatus->setTextAlignment(Qt::AlignCenter);
      QCheckBox *pCheckBox = new QCheckBox();
      pCheckBox->setStyleSheet("margin-left:50%; margin-right:50%;");
      connect(pCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onSoundCheckBoxStateChanged(int)));
      d->ToolsTableWidget->setItem( rowIndex, TOOL_NAME_COLUMN, nameItem );
      d->ToolsTableWidget->setItem( rowIndex, TOOL_LABEL_COLUMN, labelItem );
      d->ToolsTableWidget->setCellWidget(rowIndex, TOOL_SOUND_COLUMN, pCheckBox);
      d->ToolsTableWidget->setItem( rowIndex, TOOL_TIMESTAMP_COLUMN, lastElapsedTimeStatus );
    }
  }
  else if (numberOfWatchedNodes < d->ToolsTableWidget->rowCount())
  {
    // Remove lines from the table
    d->ToolsTableWidget->setRowCount( numberOfWatchedNodes );
  }

  /*for (int row=0; row<d->ToolsTableWidget->rowCount(); row++)
  {
    QCheckBox *soundCheckBox = dynamic_cast <QCheckBox *> (d->ToolsTableWidget->cellWidget(row,TOOL_SOUND_COLUMN));
    disconnect(soundCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onSoundCheckBoxStateChanged(int)));
  }
  */

  for (int watchedNodeIndex = 0; watchedNodeIndex < numberOfWatchedNodes; watchedNodeIndex++ )
  {
    vtkMRMLNode* watchedNode = d->WatchdogNode->GetWatchedNode(watchedNodeIndex);
    if (watchedNode==NULL)
    {
      qWarning("Invalid watched node found, cannot update watched node table row");
      continue;
    }
    
    d->ToolsTableWidget->item(watchedNodeIndex, TOOL_NAME_COLUMN)->setText(watchedNode->GetName());
    d->ToolsTableWidget->item(watchedNodeIndex, TOOL_LABEL_COLUMN)->setText(d->WatchdogNode->GetWatchedNodeDisplayLabel(watchedNodeIndex));

    QCheckBox *pCheckBox = qobject_cast<QCheckBox*>(d->ToolsTableWidget->cellWidget(watchedNodeIndex, TOOL_SOUND_COLUMN));
    if (pCheckBox)
    {
      pCheckBox->setCheckState(d->WatchdogNode->GetWatchedNodePlaySound(watchedNodeIndex) ? Qt::Checked : Qt::Unchecked);
      pCheckBox->setAccessibleName(QString::number(watchedNodeIndex));
    }

    if(d->WatchdogNode->GetWatchedNodeUpToDate(watchedNodeIndex))
    {
      d->ToolsTableWidget->item( watchedNodeIndex, TOOL_TIMESTAMP_COLUMN)->setText("valid");
      d->ToolsTableWidget->item( watchedNodeIndex, TOOL_TIMESTAMP_COLUMN)->setBackground(QBrush(QColor(45,224,90)));
    }
    else
    {
      d->ToolsTableWidget->item( watchedNodeIndex, TOOL_TIMESTAMP_COLUMN)->setBackground(Qt::red);
      d->ToolsTableWidget->item( watchedNodeIndex, TOOL_TIMESTAMP_COLUMN)->setText("invalid");
    }
  }

  d->ToolsTableWidget->blockSignals( false );
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onSoundCheckBoxStateChanged(int state)
{
  Q_D(qSlicerWatchdogModuleWidget);
  if ( d->WatchdogNode == NULL )
  {
    return;
  }
  QCheckBox *pCheckBox = dynamic_cast <QCheckBox *>(QObject::sender());
  int cbRow = pCheckBox->accessibleName().toInt();
  if(cbRow>=0&&cbRow<d->WatchdogNode->GetNumberOfWatchedNodes())
  {
    d->WatchdogNode->SetWatchedNodePlaySound(cbRow, state != Qt::Unchecked);
  }
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onToolBarVisibilityChanged( bool visible )
{
  Q_D(qSlicerWatchdogModuleWidget);
  if ( d->WatchdogNode == NULL )
  {
    return;
  }
  d->WatchdogNode->SetVisible(visible);
}


//-----------------------------------------------------------------------------
void qSlicerWatchdogModuleWidget::onWatchdogNodeModified()
{
  updateWidget();
}
