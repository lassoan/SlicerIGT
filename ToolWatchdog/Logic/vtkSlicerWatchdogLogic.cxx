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

// Watchdog Logic includes
#include "vtkSlicerWatchdogLogic.h"

// MRML includes
#include "vtkMRMLWatchdogNode.h"
#include "vtkMRMLWatchdogDisplayNode.h"

// VTK includes
#include <vtkNew.h>
#include <vtkCollection.h>
#include <vtkCollectionIterator.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerWatchdogLogic);

//----------------------------------------------------------------------------
vtkSlicerWatchdogLogic::vtkSlicerWatchdogLogic()
{
}

//----------------------------------------------------------------------------
vtkSlicerWatchdogLogic::~vtkSlicerWatchdogLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerWatchdogLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//-----------------------------------------------------------------------------
void vtkSlicerWatchdogLogic::RegisterNodes()
{
  if( ! this->GetMRMLScene() )
  {
    vtkErrorMacro( "vtkSlicerWatchdogLogic::RegisterNodes failed: MRML scene is invalid" );
    return;
  }
  this->GetMRMLScene()->RegisterNodeClass( vtkSmartPointer< vtkMRMLWatchdogNode >::New() );
  this->GetMRMLScene()->RegisterNodeClass( vtkSmartPointer< vtkMRMLWatchdogDisplayNode >::New() );
}

//-----------------------------------------------------------------------------
void vtkSlicerWatchdogLogic::UpdateAllWatchdogNodes()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (scene==NULL)
  {
    return;
  }
  vtkSmartPointer<vtkCollection> watchdogNodes = vtkSmartPointer<vtkCollection>::Take(scene->GetNodesByClass("vtkMRMLWatchdogNode"));
  vtkSmartPointer<vtkCollectionIterator> watchdogNodeIt = vtkSmartPointer<vtkCollectionIterator>::New();
  watchdogNodeIt->SetCollection( watchdogNodes );
  for ( watchdogNodeIt->InitTraversal(); ! watchdogNodeIt->IsDoneWithTraversal(); watchdogNodeIt->GoToNextItem() )
  {
    vtkMRMLWatchdogNode* watchdogNode = vtkMRMLWatchdogNode::SafeDownCast( watchdogNodeIt->GetCurrentObject() );
    if (watchdogNode==NULL)
    {
      continue;
    }
    watchdogNode->UpdateWatchedNodesStatus();
  }
}
