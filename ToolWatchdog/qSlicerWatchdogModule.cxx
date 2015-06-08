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
#include <QTimer>
#include <QtPlugin>

#include "qSlicerCoreApplication.h"

// Watchdog Logic includes
#include <vtkSlicerWatchdogLogic.h>
#include "vtkMRMLSliceViewDisplayableManagerFactory.h"
#include "vtkMRMLThreeDViewDisplayableManagerFactory.h" 

// Watchdog includes
#include "qSlicerWatchdogModule.h"
#include "qSlicerWatchdogModuleWidget.h"
#include "vtkMRMLWatchdogDisplayableManager2D.h"
#include "vtkMRMLWatchdogDisplayableManager3D.h"

#include "vtkMRMLWatchdogNode.h"

static const double UPDATE_WATCHDOG_NODES_PERIOD_SEC = 0.2;

//-----------------------------------------------------------------------------
Q_EXPORT_PLUGIN2(qSlicerWatchdogModule, qSlicerWatchdogModule);

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ToolWatchdog
class qSlicerWatchdogModulePrivate
{
public:
  qSlicerWatchdogModulePrivate();
  ~qSlicerWatchdogModulePrivate();
  QTimer UpdateAllWatchdogNodesTimer;
};

//-----------------------------------------------------------------------------
// qSlicerWatchdogModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerWatchdogModulePrivate::qSlicerWatchdogModulePrivate()
{
}

//-----------------------------------------------------------------------------
qSlicerWatchdogModulePrivate::~qSlicerWatchdogModulePrivate()
{
}

//-----------------------------------------------------------------------------
// qSlicerWatchdogModule methods

//-----------------------------------------------------------------------------
qSlicerWatchdogModule::qSlicerWatchdogModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerWatchdogModulePrivate)
{
  Q_D(qSlicerWatchdogModule);
  connect(&d->UpdateAllWatchdogNodesTimer, SIGNAL(timeout()), this, SLOT(updateAllWatchdogNodes()));
  vtkMRMLScene * scene = qSlicerCoreApplication::application()->mrmlScene();
  if (scene)
    {
    // Need to listen for any new watchdog nodes being added to start/stop timer
    this->qvtkConnect(scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(onNodeAddedEvent(vtkObject*,vtkObject*)));
    this->qvtkConnect(scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(onNodeRemovedEvent(vtkObject*,vtkObject*)));
    }
}

//-----------------------------------------------------------------------------
qSlicerWatchdogModule::~qSlicerWatchdogModule()
{
}

//-----------------------------------------------------------------------------
QString qSlicerWatchdogModule::helpText() const
{
  return "For help on how to use this module visit: <a href='http://www.slicerigt.org/'>SlicerIGT</a>";
}

//-----------------------------------------------------------------------------
QString qSlicerWatchdogModule::acknowledgementText() const
{
  return "This work was was funded by Cancer Care Ontario and the Ontario Consortium for Adaptive Interventions in Radiation Oncology (OCAIRO)";
}

//-----------------------------------------------------------------------------
QStringList qSlicerWatchdogModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Jaime Garcia-Guevara (Queen's University)");
  moduleContributors << QString("Andras Lasso (Queen's University)");
  moduleContributors << QString("Tamas Ungi (Queen's University)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerWatchdogModule::icon() const
{
  return QIcon(":/Icons/Watchdog.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerWatchdogModule::categories() const
{
  return QStringList() << "IGT";
}

//-----------------------------------------------------------------------------
QStringList qSlicerWatchdogModule::dependencies() const
{
  return QStringList();
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModule::setup()
{
  this->Superclass::setup();
  
  // TODO: use a single displayable manager - there is probably no need for separate 2d/3d

  // Use the displayable manager class to make sure the the containing library is loaded
  vtkSmartPointer<vtkMRMLWatchdogDisplayableManager2D> dm2d=vtkSmartPointer<vtkMRMLWatchdogDisplayableManager2D>::New();
  vtkSmartPointer<vtkMRMLWatchdogDisplayableManager3D> dm3d=vtkSmartPointer<vtkMRMLWatchdogDisplayableManager3D>::New();

  // Register displayable managers
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLWatchdogDisplayableManager2D");
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLWatchdogDisplayableManager3D"); 
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModule::setMRMLScene(vtkMRMLScene* _mrmlScene)
{
  this->Superclass::setMRMLScene(_mrmlScene);
  Q_D(qSlicerWatchdogModule);
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerWatchdogModule::createWidgetRepresentation()
{
  Q_D(qSlicerWatchdogModule);
  qSlicerWatchdogModuleWidget * watchdogWidget = new qSlicerWatchdogModuleWidget;
  return watchdogWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerWatchdogModule::createLogic()
{
  return vtkSlicerWatchdogLogic::New();
}

// --------------------------------------------------------------------------
void qSlicerWatchdogModule::onNodeAddedEvent(vtkObject*, vtkObject* node)
{
  Q_D(qSlicerWatchdogModule);

  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast(node);
  if (watchdogNode)
    {
    // If the timer is not active
    if (!d->UpdateAllWatchdogNodesTimer.isActive())
      {
      d->UpdateAllWatchdogNodesTimer.start(UPDATE_WATCHDOG_NODES_PERIOD_SEC*1000.0);
      }
    }
}

// --------------------------------------------------------------------------
void qSlicerWatchdogModule::onNodeRemovedEvent(vtkObject*, vtkObject* node)
{
  Q_D(qSlicerWatchdogModule);

  vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast(node);
  if (watchdogNode)
    {
    // If the timer is active
    if (d->UpdateAllWatchdogNodesTimer.isActive())
      {
      // Check if there is any other sequence browser node left in the Scene
      vtkMRMLScene * scene = qSlicerCoreApplication::application()->mrmlScene();
      if (scene)
        {
        std::vector<vtkMRMLNode *> nodes;
        this->mrmlScene()->GetNodesByClass("vtkMRMLWatchdogNode", nodes);
        if (nodes.size() == 0)
          {
          // The last sequence browser was removed
          d->UpdateAllWatchdogNodesTimer.stop();
          }
        }
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerWatchdogModule::updateAllWatchdogNodes()
{
  vtkSlicerWatchdogLogic* watchdogLogic = vtkSlicerWatchdogLogic::SafeDownCast(this->Superclass::logic());
  if (watchdogLogic)
    {
    watchdogLogic->UpdateAllWatchdogNodes();
    }
}
