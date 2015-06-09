// Watchdog MRML includes
#include "vtkMRMLWatchdogNode.h"
#include "vtkMRMLDisplayableNode.h"

// Other MRML includes
#include "vtkMRMLNode.h"

// VTK includes
#include <vtkCallbackCommand.h> 
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// Other includes
#include <sstream>
#include <vector>

static const char WATCHED_NODE_REFERENCE_ROLE_NAME[]="watchedNode";

vtkMRMLNodeNewMacro(vtkMRMLWatchdogNode); 

//----------------------------------------------------------------------------
class vtkMRMLWatchdogNode::vtkInternal
{
public:
  vtkInternal();

  struct WatchedNode
  {
    std::string displayLabel;
    double lastUpdateTimeSec; // time of the last update in universal time (UTC)
    double updateTimeToleranceSec; // if no update is received for more than the tolerance value then the tool is reported as invalid
    bool playSound;
    bool lastStateUpToDate; // true if the state was valid at the last update

    WatchedNode()
    {
      lastUpdateTimeSec=vtkTimerLog::GetUniversalTime();
      playSound=false;
      updateTimeToleranceSec=1.0;
      lastStateUpToDate = false;
    }
  };

  std::vector< WatchedNode > WatchedNodes;
};

vtkMRMLWatchdogNode::vtkInternal::vtkInternal()
{
} 

//----------------------------------------------------------------------------
vtkMRMLWatchdogNode::vtkMRMLWatchdogNode()
{
  this->Internal = new vtkInternal;
  this->HideFromEditorsOff();
  this->SetSaveWithScene( true );

  this->Visible = true;

  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkCommand::ModifiedEvent);
  events->InsertNextValue(vtkMRMLTransformableNode::TransformModifiedEvent);
  this->AddNodeReferenceRole(WATCHED_NODE_REFERENCE_ROLE_NAME, WATCHED_NODE_REFERENCE_ROLE_NAME, events.GetPointer());
}

//----------------------------------------------------------------------------
vtkMRMLWatchdogNode::~vtkMRMLWatchdogNode()
{
  delete this->Internal;
  this->Internal = NULL;
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::WriteXML( ostream& of, int nIndent )
{
  Superclass::WriteXML(of, nIndent); // This will take care of referenced nodes
  vtkIndent indent(nIndent);

  std::ostringstream displayLabelsAttribute;
  std::ostringstream playSoundAttribute;
  std::ostringstream updateTimeToleranceSecAttribute;
  for (std::vector<vtkMRMLWatchdogNode::vtkInternal::WatchedNode>::iterator
    it = this->Internal->WatchedNodes.begin(); it != this->Internal->WatchedNodes.end(); ++it)
  {
    if (it != this->Internal->WatchedNodes.begin())
    {
      displayLabelsAttribute << ";";
      playSoundAttribute << ";";
      updateTimeToleranceSecAttribute << ";";
    }
    displayLabelsAttribute << it->displayLabel;
    playSoundAttribute << (it->playSound ? "true" : "false");
    updateTimeToleranceSecAttribute << it->updateTimeToleranceSec;
  }

  of << indent << " watchedNodeDisplayLabel=\"" << displayLabelsAttribute.str() << "\"";
  of << indent << " watchedNodePlaySound=\"" << playSoundAttribute.str() << "\"";
  of << indent << " watchedNodeUpdateTimeToleranceSec=\"" << updateTimeToleranceSecAttribute.str() << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::ReadXMLAttributes( const char** atts )
{
  Superclass::ReadXMLAttributes(atts); // This will take care of referenced nodes

  int numberOfWatchedNodes = this->GetNumberOfNodeReferences(WATCHED_NODE_REFERENCE_ROLE_NAME);
  this->Internal->WatchedNodes.resize(numberOfWatchedNodes);

  // Read all MRML node attributes from two arrays of names and values
  while (*atts != NULL)
  {
    const char* attName  = *(atts++);
    const char* attValue = *(atts++);
    if (!strcmp(attName, "watchedNodeDisplayLabel"))
    {
      std::stringstream attributes(attValue);
      std::string attribute;
      int watchedNodeIndex=0;
      while (std::getline(attributes, attribute, ';') && watchedNodeIndex<numberOfWatchedNodes)
      {
        this->Internal->WatchedNodes[watchedNodeIndex].displayLabel = attribute;
      }
    }
    else if (!strcmp(attName, "watchedNodePlaySound"))
    {
      std::stringstream attributes(attValue);
      std::string attribute;
      int watchedNodeIndex=0;
      while (std::getline(attributes, attribute, ';') && watchedNodeIndex<numberOfWatchedNodes)
      {
        bool playSound = (attribute.compare("true")==0);
        this->Internal->WatchedNodes[watchedNodeIndex].playSound = playSound;
      }
    }
    else if (!strcmp(attName, "watchedNodeUpdateTimeToleranceSec"))
    {
      std::stringstream attributes(attValue);
      std::string attribute;
      int watchedNodeIndex=0;
      while (std::getline(attributes, attribute, ';') && watchedNodeIndex<numberOfWatchedNodes)
      {
        std::stringstream ss;
        ss << attribute;
        double updateTimeToleranceSec=1.0;
        ss >> updateTimeToleranceSec; 
        this->Internal->WatchedNodes[watchedNodeIndex].updateTimeToleranceSec = updateTimeToleranceSec;
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::Copy( vtkMRMLNode *anode )
{  
  vtkMRMLWatchdogNode* srcNode = vtkMRMLWatchdogNode::SafeDownCast(anode);
  if (srcNode==NULL)
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::Copy failed: expected intput with vtkMRMLWatchdogNode type");
    return;
  }
  Superclass::Copy( anode ); // This will take care of referenced nodes
  this->Internal->WatchedNodes = srcNode->Internal->WatchedNodes;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::PrintSelf( ostream& os, vtkIndent indent )
{
  vtkMRMLNode::PrintSelf(os,indent); // This will take care of referenced nodes

  int watchedNodeIndex=0;
  for (std::vector<vtkMRMLWatchdogNode::vtkInternal::WatchedNode>::iterator
    it = this->Internal->WatchedNodes.begin(); it != this->Internal->WatchedNodes.end(); ++it)
  {
    os << indent << "Referenced node ["<<watchedNodeIndex<<"]" << std::endl;
    const char* nodeId = this->GetNthNodeReferenceID(WATCHED_NODE_REFERENCE_ROLE_NAME, watchedNodeIndex);
    os << indent << " Node ID: " << (nodeId?nodeId:"(undefined)") << std::endl;
    os << indent << " DisplayLabel: " << it->displayLabel << std::endl;
    os << indent << " PlaySound: " << (it->playSound?"true":"false") << std::endl;
    os << indent << " UpdateTimeToleranceSec: " << it->updateTimeToleranceSec << std::endl;
    os << indent << " LastStateUpToDate: " << it->lastStateUpToDate << std::endl;
  }
}

//----------------------------------------------------------------------------
int vtkMRMLWatchdogNode::AddWatchedNode(vtkMRMLNode *watchedNode, const char* displayLabel/*=NULL*/,
                                        double updateTimeToleranceSec/*=-1*/, bool playSound/*=false*/)
{
  if (watchedNode==NULL)
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::AddWatchedNode failed: invalid watched node");
    return -1;
  }

  vtkMRMLWatchdogNode::vtkInternal::WatchedNode newWatchedNodeInfo;

  if (displayLabel)
  {
    newWatchedNodeInfo.displayLabel = displayLabel;
  }
  else
  {
    const char* watchedNodeName = watchedNode->GetName();
    if (watchedNodeName!=NULL)
    {
      std::string label = watchedNodeName;
      newWatchedNodeInfo.displayLabel = label.substr(0,5); // use the first 5 character of the node name as default name
    }
  }

  if (updateTimeToleranceSec>0)
  {
    newWatchedNodeInfo.updateTimeToleranceSec = updateTimeToleranceSec;
  }

  newWatchedNodeInfo.playSound = playSound;

  this->Internal->WatchedNodes.push_back(newWatchedNodeInfo);
  int newNodeIndex = this->Internal->WatchedNodes.size()-1;

  this->SetAndObserveNthNodeReferenceID(WATCHED_NODE_REFERENCE_ROLE_NAME, newNodeIndex, watchedNode->GetID());

  return newNodeIndex;
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::RemoveWatchedNode(int watchedNodeIndex)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::RemoveWatchedNode failed: invalid index "<<watchedNodeIndex);
    return;
  }
  this->Internal->WatchedNodes.erase(this->Internal->WatchedNodes.begin()+watchedNodeIndex);
  this->RemoveNthNodeReferenceID(WATCHED_NODE_REFERENCE_ROLE_NAME, watchedNodeIndex);
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::RemoveAllWatchedNodes()
{
  this->Internal->WatchedNodes.clear();
  this->RemoveNodeReferenceIDs(WATCHED_NODE_REFERENCE_ROLE_NAME);
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::SwapWatchedNodes( int watchedNodeIndexA, int watchedNodeIndexB )
{
  if(watchedNodeIndexA<0 || watchedNodeIndexA>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::RemoveWatchedNode failed: invalid watchedNodeIndexA "<<watchedNodeIndexA);
    return;
  }
  if(watchedNodeIndexB<0 || watchedNodeIndexB>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::RemoveWatchedNode failed: invalid watchedNodeIndexB "<<watchedNodeIndexB);
    return;
  }

  std::vector<vtkMRMLWatchdogNode::vtkInternal::WatchedNode>::iterator itA = this->Internal->WatchedNodes.begin()+watchedNodeIndexA;
  std::vector<vtkMRMLWatchdogNode::vtkInternal::WatchedNode>::iterator itB = this->Internal->WatchedNodes.begin()+watchedNodeIndexB;

  vtkMRMLWatchdogNode::vtkInternal::WatchedNode watchedNodeInfoTemp = *itA;
  *itA = *itB;
  *itB = watchedNodeInfoTemp;
}

//----------------------------------------------------------------------------
int vtkMRMLWatchdogNode::GetWatchedNodeIndex(vtkMRMLNode* watchedNode)
{
  if (watchedNode==NULL)
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::FindWatchedNodeByDisplayLabel: invalid input node");
    return -1;
  }
  int numberOfWatchedNodes = this->GetNumberOfNodeReferences(WATCHED_NODE_REFERENCE_ROLE_NAME);
  for (unsigned int watchedNodeIndex=0; watchedNodeIndex<numberOfWatchedNodes; watchedNodeIndex++)
  {
    vtkMRMLNode* watchedNodeFound = this->GetNthNodeReference(WATCHED_NODE_REFERENCE_ROLE_NAME, watchedNodeIndex);
    if (watchedNode == watchedNodeFound)
    {
      return watchedNodeIndex;
    }
  }
  // not found
  return -1;
} 


//----------------------------------------------------------------------------
vtkMRMLNode* vtkMRMLWatchdogNode::GetWatchedNode(int watchedNodeIndex)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::GetWatchedNode failed: invalid index "<<watchedNodeIndex);
    return NULL;
  }
  return this->GetNthNodeReference(WATCHED_NODE_REFERENCE_ROLE_NAME, watchedNodeIndex);
}

//----------------------------------------------------------------------------
int vtkMRMLWatchdogNode::GetNumberOfWatchedNodes()
{
  return this->Internal->WatchedNodes.size();
}

//----------------------------------------------------------------------------
const char* vtkMRMLWatchdogNode::GetWatchedNodeDisplayLabel(int watchedNodeIndex)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::GetWatchedNodeDisplayLabel failed: invalid index "<<watchedNodeIndex);
    return NULL;
  }
  return this->Internal->WatchedNodes[watchedNodeIndex].displayLabel.c_str();
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::SetWatchedNodeDisplayLabel(int watchedNodeIndex, const char* displayLabel)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::SetWatchedNodeDisplayLabel failed: invalid index "<<watchedNodeIndex);
    return;
  }
  this->Internal->WatchedNodes[watchedNodeIndex].displayLabel = (displayLabel ? displayLabel : "");
}

//---------------------------------------------------------------------------- 
double vtkMRMLWatchdogNode::GetWatchedNodeUpdateTimeToleranceSec(int watchedNodeIndex)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::GetWatchedNodeUpdateTimeToleranceSec failed: invalid index "<<watchedNodeIndex);
    return 0.0;
  }
  return this->Internal->WatchedNodes[watchedNodeIndex].updateTimeToleranceSec;
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::SetWatchedNodeUpdateTimeToleranceSec(int watchedNodeIndex, double updateTimeToleranceSec)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::SetWatchedNodeUpdateTimeToleranceSec failed: invalid index "<<watchedNodeIndex);
    return;
  }
  this->Internal->WatchedNodes[watchedNodeIndex].updateTimeToleranceSec = updateTimeToleranceSec;
}

//----------------------------------------------------------------------------
bool vtkMRMLWatchdogNode::GetWatchedNodeUpToDate(int watchedNodeIndex)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::GetWatchedNodeUpToDate failed: invalid index "<<watchedNodeIndex);
    return true;
  }
  return this->Internal->WatchedNodes[watchedNodeIndex].lastStateUpToDate;
}

//----------------------------------------------------------------------------
double vtkMRMLWatchdogNode::GetWatchedNodeElapsedTimeSinceLastUpdateSec(int watchedNodeIndex)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::GetWatchedNodeElapsedTimeSinceLastUpdateSec failed: invalid index "<<watchedNodeIndex);
    return 0;
  }
  return this->Internal->WatchedNodes[watchedNodeIndex].lastUpdateTimeSec-vtkTimerLog::GetUniversalTime();
}

//----------------------------------------------------------------------------
bool vtkMRMLWatchdogNode::GetWatchedNodePlaySound(int watchedNodeIndex)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::GetWatchedNodePlaySound failed: invalid index "<<watchedNodeIndex);
    return 0.0;
  }
  return this->Internal->WatchedNodes[watchedNodeIndex].playSound;
}

//----------------------------------------------------------------------------
void vtkMRMLWatchdogNode::SetWatchedNodePlaySound(int watchedNodeIndex, bool playSound)
{
  if(watchedNodeIndex<0 || watchedNodeIndex>=this->Internal->WatchedNodes.size())
  {
    vtkErrorMacro("vtkMRMLWatchdogNode::SetWatchedNodePlaySound failed: invalid index "<<watchedNodeIndex);
    return;
  }
  this->Internal->WatchedNodes[watchedNodeIndex].playSound = playSound;
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogNode::ProcessMRMLEvents ( vtkObject *caller, unsigned long event, void *callData )
{
  Superclass::ProcessMRMLEvents(caller, event, callData);

  vtkMRMLNode* callerNode = vtkMRMLNode::SafeDownCast(caller);
  if (callerNode!=NULL)
  {
    int numberOfWatchedNodes = this->GetNumberOfNodeReferences(WATCHED_NODE_REFERENCE_ROLE_NAME);
    for (unsigned int watchedNodeIndex=0; watchedNodeIndex<numberOfWatchedNodes; watchedNodeIndex++)
    {
      vtkMRMLNode* watchedNode = this->GetNthNodeReference(WATCHED_NODE_REFERENCE_ROLE_NAME, watchedNodeIndex);
      if (watchedNode == NULL || watchedNode != callerNode
        || (event != vtkCommand::ModifiedEvent && event != vtkMRMLTransformableNode::TransformModifiedEvent))
      {
        continue;
      }
      if(watchedNodeIndex>=this->Internal->WatchedNodes.size())
      {
        vtkErrorMacro("vtkMRMLWatchdogNode::ProcessMRMLEvents failed: no watched node found for node reference "<<watchedNodeIndex);
        break;
      }
      // we've found the watched node that has been just updated
      this->Internal->WatchedNodes[watchedNodeIndex].lastUpdateTimeSec=vtkTimerLog::GetUniversalTime();
      break;
    }
  } 
}

//---------------------------------------------------------------------------
void vtkMRMLWatchdogNode::UpdateWatchedNodesStatus()
{
  bool watchedToolStateModified = false;
  double currentTimeSec = vtkTimerLog::GetUniversalTime();
  for (std::vector<vtkMRMLWatchdogNode::vtkInternal::WatchedNode>::iterator
    it = this->Internal->WatchedNodes.begin(); it != this->Internal->WatchedNodes.end(); ++it)
  {
    double elapsedTimeSec = currentTimeSec - it->lastUpdateTimeSec;
    bool upToDate = ( elapsedTimeSec <= it->updateTimeToleranceSec);
    if (upToDate != it->lastStateUpToDate)
    {
      it->lastStateUpToDate = upToDate;
      watchedToolStateModified = true;
    }
  }
  if (watchedToolStateModified)
  {
    this->Modified();
  }
}
