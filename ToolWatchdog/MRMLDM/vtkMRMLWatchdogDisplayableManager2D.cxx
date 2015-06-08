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
#include "vtkMRMLWatchdogDisplayableManager2D.h"

#include "vtkSlicerWatchdogLogic.h"

// MRML includes
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLWatchdogDisplayNode.h>
#include <vtkMRMLWatchdogNode.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkColorTransferFunction.h>
#include <vtkEventBroker.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>
#include <vtkPointLocator.h>

// STD includes
#include <algorithm>
#include <cassert>
#include <set>
#include <map>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLWatchdogDisplayableManager2D );

//---------------------------------------------------------------------------
class vtkMRMLWatchdogDisplayableManager2D::vtkInternal
{
public:

  vtkInternal( vtkMRMLWatchdogDisplayableManager2D* external );
  ~vtkInternal();
  struct Pipeline
    {
    vtkSmartPointer<vtkProp> Actor;
    };

  typedef std::map < vtkMRMLWatchdogDisplayNode*, const Pipeline* > PipelinesCacheType;
  PipelinesCacheType DisplayPipelines;

  typedef std::map < vtkMRMLWatchdogNode*, std::set< vtkMRMLWatchdogDisplayNode* > > WatchdogToDisplayCacheType;
  WatchdogToDisplayCacheType WatchdogToDisplayNodes;

  // Watchdogs
  void AddWatchdogNode(vtkMRMLWatchdogNode* displayableNode);
  void RemoveWatchdogNode(vtkMRMLWatchdogNode* displayableNode);
  void UpdateDisplayableWatchdogs(vtkMRMLWatchdogNode *node);

  // Slice Node
  void SetSliceNode(vtkMRMLSliceNode* sliceNode);
  void UpdateSliceNode();

  // Display Nodes
  void AddDisplayNode(vtkMRMLWatchdogNode*, vtkMRMLWatchdogDisplayNode*);
  void UpdateDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode);
  void UpdateDisplayNodePipeline(vtkMRMLWatchdogDisplayNode*, const Pipeline*);
  void RemoveDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode);

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
  vtkMRMLWatchdogDisplayableManager2D* External;
  bool AddingWatchdogNode;
  vtkSmartPointer<vtkMRMLSliceNode> SliceNode;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager2D::vtkInternal::vtkInternal(vtkMRMLWatchdogDisplayableManager2D* external)
: External(external)
, AddingWatchdogNode(false)
{
}

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager2D::vtkInternal::~vtkInternal()
{
  this->ClearDisplayableNodes();
  this->SliceNode = NULL;
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager2D::vtkInternal::UseDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode)
{
   // allow annotations to appear only in designated viewers
  if (displayNode && !displayNode->IsDisplayableInView(this->SliceNode->GetID()))
    {
    return false;
    }

  // Check whether DisplayNode should be shown in this view
  bool use = displayNode && displayNode->IsA("vtkMRMLWatchdogDisplayNode");

  return use;
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager2D::vtkInternal::IsVisible(vtkMRMLWatchdogDisplayNode* displayNode)
{
  return displayNode && (displayNode->GetSliceIntersectionVisibility() != 0);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::SetSliceNode(vtkMRMLSliceNode* sliceNode)
{
  if (!sliceNode || this->SliceNode == sliceNode)
    {
    return;
    }
  this->SliceNode=sliceNode;
  this->UpdateSliceNode();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::UpdateSliceNode()
{
  // Update the Slice node transform

  PipelinesCacheType::iterator it;
  for (it = this->DisplayPipelines.begin(); it != this->DisplayPipelines.end(); ++it)
    {
    this->UpdateDisplayNodePipeline(it->first, it->second);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::AddWatchdogNode(vtkMRMLWatchdogNode* node)
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
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::RemoveWatchdogNode(vtkMRMLWatchdogNode* node)
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

  std::set< vtkMRMLWatchdogDisplayNode* > dnodes = displayableIt->second;
  std::set< vtkMRMLWatchdogDisplayNode* >::iterator diter;
  for ( diter = dnodes.begin(); diter != dnodes.end(); ++diter)
    {
    this->RemoveDisplayNode(*diter);
    }
  this->RemoveObservations(node);
  this->WatchdogToDisplayNodes.erase(displayableIt);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::UpdateDisplayableWatchdogs(vtkMRMLWatchdogNode* mNode)
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
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::RemoveDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode)
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
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::AddDisplayNode(vtkMRMLWatchdogNode* mNode, vtkMRMLWatchdogDisplayNode* displayNode)
{
  if (!mNode || !displayNode)
    {
    return;
    }

  // Do not add the display node if it is already associated with a pipeline object.
  // This happens when a watchdog node already associated with a display node
  // is copied into an other (using vtkMRMLNode::Copy()) and is added to the scene afterward.
  // Related issue are #3428 and #2608
  PipelinesCacheType::iterator it;
  it = this->DisplayPipelines.find(displayNode);
  if (it != this->DisplayPipelines.end())
    {
    return;
    }

  vtkNew<vtkActor2D> actor;
  if (displayNode->IsA("vtkMRMLWatchdogDisplayNode"))
    {
    actor->SetMapper( vtkNew<vtkPolyDataMapper2D>().GetPointer() );
    }

  // Create pipeline
  Pipeline* pipeline = new Pipeline();
  pipeline->Actor = actor.GetPointer();

  // Set up pipeline
  pipeline->Actor->SetVisibility(0);

  // Add actor to Renderer and local cache
  this->External->GetRenderer()->AddActor( pipeline->Actor );
  this->DisplayPipelines.insert( std::make_pair(displayNode, pipeline) );

  // Update cached matrices. Calls UpdateDisplayNodePipeline
  this->UpdateDisplayableWatchdogs(mNode);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::UpdateDisplayNode(vtkMRMLWatchdogDisplayNode* displayNode)
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
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::UpdateDisplayNodePipeline(vtkMRMLWatchdogDisplayNode* displayNode, const Pipeline* pipeline)
{
  // Sets visibility, set pipeline polydata input, update color
  //   calculate and set pipeline.

  if (!displayNode || !pipeline)
    {
    return;
    }

  // Update visibility
  bool visible = this->IsVisible(displayNode);
  pipeline->Actor->SetVisibility(visible);
  if (!visible)
    {
    return;
    }

  vtkMRMLWatchdogDisplayNode* watchdogDisplayNode = vtkMRMLWatchdogDisplayNode::SafeDownCast(displayNode);

  //vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
//  vtkSlicerWatchdogLogic::GetVisualization2d(polyData, watchdogDisplayNode, this->SliceNode);

  // Update pipeline actor
  vtkActor2D* actor = vtkActor2D::SafeDownCast(pipeline->Actor);
  vtkPolyDataMapper2D* mapper = vtkPolyDataMapper2D::SafeDownCast(actor->GetMapper());
  //mapper->SetInputConnection( pipeline->Transformer->GetOutputPort() );

  actor->SetPosition(0,0);
  vtkProperty2D* actorProperties = actor->GetProperty();
  actorProperties->SetColor(displayNode->GetColor() );
  actorProperties->SetLineWidth(displayNode->GetSliceIntersectionThickness() );
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::AddObservations(vtkMRMLWatchdogNode* node)
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
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::RemoveObservations(vtkMRMLWatchdogNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkEventBroker::ObservationVector observations;
  observations = broker->GetObservations(node, vtkCommand::ModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() );
  broker->RemoveObservations(observations);
  observations = broker->GetObservations(node, vtkMRMLDisplayableNode::DisplayModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand() );
  broker->RemoveObservations(observations);
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager2D::vtkInternal::IsNodeObserved(vtkMRMLWatchdogNode* node)
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
void vtkMRMLWatchdogDisplayableManager2D::vtkInternal::ClearDisplayableNodes()
{
  while(this->WatchdogToDisplayNodes.size() > 0)
    {
    this->RemoveWatchdogNode(this->WatchdogToDisplayNodes.begin()->first);
    }
}

//---------------------------------------------------------------------------
bool vtkMRMLWatchdogDisplayableManager2D::vtkInternal::UseDisplayableNode(vtkMRMLWatchdogNode* node)
{
  bool use = node && node->IsA("vtkMRMLWatchdogNode");
  return use;
}

//---------------------------------------------------------------------------
// vtkMRMLWatchdogDisplayableManager2D methods

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager2D::vtkMRMLWatchdogDisplayableManager2D()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLWatchdogDisplayableManager2D::~vtkMRMLWatchdogDisplayableManager2D()
{
  delete this->Internal;
  this->Internal=NULL;
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "vtkMRMLWatchdogDisplayableManager2D: " << this->GetClassName() << "\n";
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
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
void vtkMRMLWatchdogDisplayableManager2D::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
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
void vtkMRMLWatchdogDisplayableManager2D::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
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
    else if (vtkCommand::ModifiedEvent)
      {
      this->Internal->UpdateDisplayableWatchdogs(displayableNode);
      this->RequestRender();
      }
    }
  else if ( vtkMRMLSliceNode::SafeDownCast(caller) )
      {
      this->Internal->UpdateSliceNode();
      this->RequestRender();
      }
  else
    {
    this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::UpdateFromMRML()
{
  this->SetUpdateFromMRMLRequested(0);

  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene)
    {
    vtkDebugMacro( "vtkMRMLWatchdogDisplayableManager2D->UpdateFromMRML: Scene is not set.")
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
void vtkMRMLWatchdogDisplayableManager2D::UnobserveMRMLScene()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::OnMRMLSceneStartClose()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::OnMRMLSceneEndClose()
{
  this->SetUpdateFromMRMLRequested(1);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::OnMRMLSceneEndBatchProcess()
{
  this->SetUpdateFromMRMLRequested(1);
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogDisplayableManager2D::Create()
{
  this->Internal->SetSliceNode(this->GetMRMLSliceNode());
  this->SetUpdateFromMRMLRequested(1);
}
