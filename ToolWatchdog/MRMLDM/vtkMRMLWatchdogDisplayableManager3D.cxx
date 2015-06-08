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

  This file was originally developed by Andras Lasso and Franklin King at
  PerkLab, Queen's University and was supported through the Applied Cancer
  Research Unit program of Cancer Care Ontario with funds provided by the
  Ontario Ministry of Health and Long-Term Care.

==============================================================================*/


// MRMLDisplayableManager includes
#include "vtkMRMLWatchdogDisplayableManager3D.h"

#include "vtkSlicerWatchdogLogic.h"

// MRML includes
#include <vtkEventBroker.h>
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLWatchdogDisplayNode.h>
#include <vtkMRMLWatchdogNode.h>
#include <vtkMRMLViewNode.h>

// VTK includes
#include <vtkColorTransferFunction.h>
#include <vtkDataSetAttributes.h>
#include <vtkLookupTable.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

//---------------------------------------------------------------------------
vtkStandardNewMacro ( vtkMRMLWatchdogDisplayableManager3D );

//---------------------------------------------------------------------------
class vtkMRMLWatchdogDisplayableManager3D::vtkInternal
{
public:

  vtkInternal(vtkMRMLWatchdogDisplayableManager3D* external);
  ~vtkInternal();

  struct Pipeline
    {
    vtkSmartPointer<vtkActor> Actor;
    vtkSmartPointer<vtkPolyData> InputPolyData;
    };

  typedef std::map < vtkMRMLWatchdogDisplayNode*, const Pipeline* > PipelinesCacheType;
  PipelinesCacheType DisplayPipelines;

  typedef std::map < vtkMRMLWatchdogNode*, std::set< vtkMRMLWatchdogDisplayNode* > > WatchdogToDisplayCacheType;
  WatchdogToDisplayCacheType WatchdogToDisplayNodes;

  // Watchdogs
  void AddWatchdogNode(vtkMRMLWatchdogNode* displayableNode);
  void RemoveWatchdogNode(vtkMRMLWatchdogNode* displayableNode);
  void UpdateDisplayableWatchdogs(vtkMRMLWatchdogNode *node);

  // Display Nodes
  void AddDisplayNode(vtkMRMLWatchdogNode*, vtkMRMLWatchdogDisplayNode*);
  void UpdateDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode);
  void UpdateDisplayNodePipeline(vtkMRMLWatchdogDisplayNode*, const Pipeline*);
  void RemoveDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode);
  void SetWatchdogDisplayProperty(vtkMRMLWatchdogDisplayNode *displayNode, vtkActor* actor);

  // Observations
  void AddObservations(vtkMRMLWatchdogNode* node);
  void RemoveObservations(vtkMRMLWatchdogNode* node);
  bool IsNodeObserved(vtkMRMLWatchdogNode* node);

  // Helper functions
  bool IsVisible(vtkMRMLWatchdogDisplayNode* displayNode);
  bool UseDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode);
  bool UseDisplayableNode(vtkMRMLWatchdogNode* node);
  void ClearDisplayableNodes();

private:
  vtkMRMLWatchdogDisplayableManager3D* External;
  bool AddingWatchdogNode;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager3D::vtkInternal::vtkInternal(vtkMRMLWatchdogDisplayableManager3D * external)
: External(external)
, AddingWatchdogNode(false)
{
}

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager3D::vtkInternal::~vtkInternal()
{
  this->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager3D::vtkInternal::UseDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode)
{
   // allow annotations to appear only in designated viewers
  if (displayNode && !displayNode->IsDisplayableInView(this->External->GetMRMLViewNode()->GetID()))
    {
    return false;
    }

  // Check whether DisplayNode should be shown in this view
  bool use = displayNode && displayNode->IsA("vtkMRMLWatchdogDisplayNode");

  return use;
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager3D::vtkInternal::IsVisible(vtkMRMLWatchdogDisplayNode* displayNode)
{
  return displayNode && (displayNode->GetVisibility() != 0);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::AddWatchdogNode(vtkMRMLWatchdogNode* node)
{
  if (this->AddingWatchdogNode)
    {
    return;
    }
  // Check if node should be used
  if (!this->UseDisplayableNode(node))
    {
    return;
    }

  this->AddingWatchdogNode = true;
  // Add Display Nodes
  int nnodes = node->GetNumberOfDisplayNodes();

  this->AddObservations(node);

  for (int i=0; i<nnodes; i++)
    {
    vtkMRMLWatchdogDisplayNode *dnode = vtkMRMLWatchdogDisplayNode::SafeDownCast(node->GetNthDisplayNode(i));
    if ( this->UseDisplayNode(dnode) )
      {
      this->WatchdogToDisplayNodes[node].insert(dnode);
      this->AddDisplayNode( node, dnode );
      }
    }
  this->AddingWatchdogNode = false;
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::RemoveWatchdogNode(vtkMRMLWatchdogNode* node)
{
  if (!node)
    {
    return;
    }
  vtkInternal::WatchdogToDisplayCacheType::iterator displayableIt =
    this->WatchdogToDisplayNodes.find(node);
  if(displayableIt == this->WatchdogToDisplayNodes.end())
    {
    return;
    }

  std::set<vtkMRMLWatchdogDisplayNode *> dnodes = displayableIt->second;
  std::set<vtkMRMLWatchdogDisplayNode *>::iterator diter;
  for ( diter = dnodes.begin(); diter != dnodes.end(); ++diter)
    {
    this->RemoveDisplayNode(*diter);
    }
  this->RemoveObservations(node);
  this->WatchdogToDisplayNodes.erase(displayableIt);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::UpdateDisplayableWatchdogs(vtkMRMLWatchdogNode* mNode)
{
  // Update the pipeline for all tracked DisplayableNode

  PipelinesCacheType::iterator pipelinesIter;
  std::set< vtkMRMLWatchdogDisplayNode* > displayNodes = this->WatchdogToDisplayNodes[mNode];
  std::set< vtkMRMLWatchdogDisplayNode* >::iterator dnodesIter;
  for ( dnodesIter = displayNodes.begin(); dnodesIter != displayNodes.end(); dnodesIter++ )
    {
    if ( ((pipelinesIter = this->DisplayPipelines.find(*dnodesIter)) != this->DisplayPipelines.end()) )
      {
      this->UpdateDisplayNodePipeline(pipelinesIter->first, pipelinesIter->second);
      }
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::RemoveDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode)
{
  PipelinesCacheType::iterator actorsIt = this->DisplayPipelines.find(displayNode);
  if(actorsIt == this->DisplayPipelines.end())
    {
    return;
    }
  const Pipeline* pipeline = actorsIt->second;
  this->External->GetRenderer()->RemoveActor(pipeline->Actor);
  delete pipeline;
  this->DisplayPipelines.erase(actorsIt);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::AddDisplayNode(vtkMRMLWatchdogNode* mNode, vtkMRMLWatchdogDisplayNode* displayNode)
{
  if (!mNode || !displayNode)
    {
    return;
    }

  // Do not add the display node if it is already associated with a pipeline object.
  // This happens when a watchdog node already associated with a display node
  // is copied into an other (using vtkMRMLNode::Copy()) and is added to the scene afterwards.
  // Related issue are #3428 and #2608
  PipelinesCacheType::iterator it;
  it = this->DisplayPipelines.find(displayNode);
  if (it != this->DisplayPipelines.end())
    {
    return;
    }

  // Create pipeline
  Pipeline* pipeline = new Pipeline();

  pipeline->Actor = vtkSmartPointer<vtkActor>::New();
  vtkNew<vtkPolyDataMapper> mapper;
  pipeline->Actor->SetMapper(mapper.GetPointer());
  pipeline->Actor->SetVisibility(false);
  pipeline->InputPolyData = vtkSmartPointer<vtkPolyData>::New();
#if VTK_MAJOR_VERSION <= 5
  mapper->SetInput(pipeline->InputPolyData);
#else
  mapper->SetInputData(pipeline->InputPolyData);
#endif

  // Add actor to Renderer and local cache
  this->External->GetRenderer()->AddActor( pipeline->Actor );
  this->DisplayPipelines.insert( std::make_pair(displayNode, pipeline) );

  // Update cached matrices. Calls UpdateDisplayNodePipeline
  this->UpdateDisplayableWatchdogs(mNode);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::UpdateDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode)
{
  // If the DisplayNode already exists, just update.
  //   otherwise, add as new node

  if (!displayNode)
    {
    return;
    }
  PipelinesCacheType::iterator it;
  it = this->DisplayPipelines.find(displayNode);
  if (it != this->DisplayPipelines.end())
    {
    this->UpdateDisplayNodePipeline(displayNode, it->second);
    }
  else
    {
    this->AddWatchdogNode( vtkMRMLWatchdogNode::SafeDownCast(displayNode->GetDisplayableNode()) );
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::UpdateDisplayNodePipeline(vtkMRMLWatchdogDisplayNode* displayNode, const Pipeline* pipeline)
{
  // Sets visibility, set pipeline polydata input, update color
  //   calculate and set pipeline watchdogs.

  if (!displayNode || !pipeline)
    {
    return;
    }

  vtkMRMLWatchdogNode* watchdogNode=vtkMRMLWatchdogNode::SafeDownCast(displayNode->GetDisplayableNode());
  if (watchdogNode==NULL)
    {
    pipeline->Actor->SetVisibility(false);
    return;
    }

/*  vtkMRMLNode* regionNode=displayNode->GetRegionNode();
  if (regionNode==NULL)
    {
    pipeline->Actor->SetVisibility(false);
    return;
    }
    */
  // Update visibility
  bool visible = this->IsVisible(displayNode);
  pipeline->Actor->SetVisibility(visible);
  if (!visible)
    {
    return;
    }

/*
if (!vtkSlicerWatchdogLogic::GetVisualization3d(pipeline->InputPolyData, displayNode, regionNode))
  {
    vtkWarningWithObjectMacro(displayNode, "Failed to show watchdog in 3D: unsupported ROI type");
    pipeline->Actor->SetVisibility(false);
    return;
  }
*/
  if (pipeline->InputPolyData->GetNumberOfPoints()==0)
    {
    pipeline->Actor->SetVisibility(false);
    return;
    }

  // Update pipeline actor
  this->SetWatchdogDisplayProperty(displayNode, pipeline->Actor);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::AddObservations(vtkMRMLWatchdogNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  if (!broker->GetObservationExist(node, vtkCommand::ModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() ))
    {
    broker->AddObservation(node, vtkCommand::ModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() );
    }
  if (!broker->GetObservationExist(node, vtkMRMLDisplayableNode::DisplayModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() ))
    {
    broker->AddObservation(node, vtkMRMLDisplayableNode::DisplayModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() );
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::RemoveObservations(vtkMRMLWatchdogNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkEventBroker::ObservationVector observations;
  observations = broker->GetObservations(node, vtkCommand::ModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() );
  broker->RemoveObservations(observations);
  observations = broker->GetObservations(node, vtkMRMLDisplayableNode::DisplayModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() );
  broker->RemoveObservations(observations);
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager3D::vtkInternal::IsNodeObserved(vtkMRMLWatchdogNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkCollection* observations = broker->GetObservationsForSubject(node);
  if (observations->GetNumberOfItems() > 0)
    {
    return true;
    }
  else
    {
    return false;
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::ClearDisplayableNodes()
{
  while(this->WatchdogToDisplayNodes.size() > 0)
    {
    this->RemoveWatchdogNode(this->WatchdogToDisplayNodes.begin()->first);
    }
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager3D::vtkInternal::UseDisplayableNode(vtkMRMLWatchdogNode* node)
{
  bool use = node && node->IsA("vtkMRMLWatchdogNode");
  return use;
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::vtkInternal::SetWatchdogDisplayProperty(vtkMRMLWatchdogDisplayNode *displayNode, vtkActor* actor)
{
  bool visible = this->IsVisible(displayNode);
  actor->SetVisibility(visible);

  vtkMapper* mapper=actor->GetMapper();

  mapper->SetScalarVisibility(false);

  actor->GetProperty()->SetRepresentation(displayNode->GetRepresentation());
  actor->GetProperty()->SetPointSize(displayNode->GetPointSize());
  actor->GetProperty()->SetLineWidth(displayNode->GetLineWidth());
  actor->GetProperty()->SetLighting(displayNode->GetLighting());
  actor->GetProperty()->SetInterpolation(displayNode->GetInterpolation());
  actor->GetProperty()->SetShading(displayNode->GetShading());
  actor->GetProperty()->SetFrontfaceCulling(displayNode->GetFrontfaceCulling());
  actor->GetProperty()->SetBackfaceCulling(displayNode->GetBackfaceCulling());

  if (displayNode->GetSelected())
    {
    actor->GetProperty()->SetColor(displayNode->GetSelectedColor());
    actor->GetProperty()->SetAmbient(displayNode->GetSelectedAmbient());
    actor->GetProperty()->SetSpecular(displayNode->GetSelectedSpecular());
    }
  else
    {
    actor->GetProperty()->SetColor(displayNode->GetColor());
    actor->GetProperty()->SetAmbient(displayNode->GetAmbient());
    actor->GetProperty()->SetSpecular(displayNode->GetSpecular());
    }
  actor->GetProperty()->SetOpacity(displayNode->GetOpacity());
  actor->GetProperty()->SetDiffuse(displayNode->GetDiffuse());
  actor->GetProperty()->SetSpecularPower(displayNode->GetPower());
  actor->GetProperty()->SetEdgeVisibility(displayNode->GetEdgeVisibility());
  actor->GetProperty()->SetEdgeColor(displayNode->GetEdgeColor());

  actor->SetTexture(0);
}

//---------------------------------------------------------------------------
// vtkMRMLWatchdogDisplayableManager3D methods

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager3D::vtkMRMLWatchdogDisplayableManager3D()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager3D::~vtkMRMLWatchdogDisplayableManager3D()
{
  delete this->Internal;
  this->Internal=NULL;
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::PrintSelf ( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf ( os, indent );
  os << indent << "vtkMRMLWatchdogDisplayableManager3D: " << this->GetClassName() << "\n";
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if ( !node->IsA("vtkMRMLWatchdogNode") )
    {
    return;
    }

  // Escape if the scene a scene is being closed, imported or connected
  if (this->GetMRMLScene()->IsBatchProcessing())
    {
    this->SetUpdateFromMRMLRequested(1);
    return;
    }

  this->Internal->AddWatchdogNode(vtkMRMLWatchdogNode::SafeDownCast(node));
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  if ( node
    && (!node->IsA("vtkMRMLWatchdogNode"))
    && (!node->IsA("vtkMRMLWatchdogDisplayNode")) )
    {
    return;
    }

  vtkMRMLWatchdogNode* watchdogNode = NULL;
  vtkMRMLWatchdogDisplayNode* displayNode = NULL;

  bool modified = false;
  if ( (watchdogNode = vtkMRMLWatchdogNode::SafeDownCast(node)) )
    {
    this->Internal->RemoveWatchdogNode(watchdogNode);
    modified = true;
    }
  else if ( (displayNode = vtkMRMLWatchdogDisplayNode::SafeDownCast(node)) )
    {
    this->Internal->RemoveDisplayNode(displayNode);
    modified = true;
    }
  if (modified)
    {
    this->RequestRender();
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  vtkMRMLScene* scene = this->GetMRMLScene();

  if ( scene->IsBatchProcessing() )
    {
    return;
    }

  vtkMRMLWatchdogNode* displayableNode = vtkMRMLWatchdogNode::SafeDownCast(caller);

  if ( displayableNode )
    {
    vtkMRMLNode* callDataNode = reinterpret_cast<vtkMRMLDisplayNode *> (callData);
    vtkMRMLWatchdogDisplayNode* displayNode = vtkMRMLWatchdogDisplayNode::SafeDownCast(callDataNode);

    if ( displayNode && (event == vtkMRMLDisplayableNode::DisplayModifiedEvent) )
      {
      this->Internal->UpdateDisplayNode(displayNode);
      this->RequestRender();
      }
    else if (event == vtkCommand::ModifiedEvent)
      {
      this->Internal->UpdateDisplayableWatchdogs(displayableNode);
      this->RequestRender();
      }
    }
  else
    {
    this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::UpdateFromMRML()
{
  this->SetUpdateFromMRMLRequested(0);

  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene)
    {
    vtkDebugMacro( "vtkMRMLWatchdogDisplayableManager3D->UpdateFromMRML: Scene is not set.")
    return;
    }
  this->Internal->ClearDisplayableNodes();

  vtkMRMLWatchdogNode* mNode = NULL;
  std::vector<vtkMRMLNode *> mNodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLWatchdogNode", mNodes) : 0;
  for (int i=0; i<nnodes; i++)
    {
    mNode  = vtkMRMLWatchdogNode::SafeDownCast(mNodes[i]);
    if (mNode && this->Internal->UseDisplayableNode(mNode))
      {
      this->Internal->AddWatchdogNode(mNode);
      }
    }
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::UnobserveMRMLScene()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::OnMRMLSceneStartClose()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::OnMRMLSceneEndClose()
{
  this->SetUpdateFromMRMLRequested(1);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::OnMRMLSceneEndBatchProcess()
{
  this->SetUpdateFromMRMLRequested(1);
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager3D::Create()
{
  Superclass::Create();
  this->SetUpdateFromMRMLRequested(1);
}
