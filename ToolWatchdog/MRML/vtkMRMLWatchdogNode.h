/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLWatchdogNode.h,v $
  Date:      $Date: 2006/03/19 17:12:28 $
  Version:   $Revision: 1.6 $

=========================================================================auto=*/

#ifndef __vtkMRMLWatchdogNode_h
#define __vtkMRMLWatchdogNode_h

#include <iostream>

#include "vtkMRMLDisplayableNode.h"
#include "vtkMRMLScene.h"
#include "vtkObject.h"
#include "vtkObjectBase.h"
#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkTimerLog.h"
#include "vtkWeakPointer.h"

// Watchdog includes
#include "vtkSlicerWatchdogModuleMRMLExport.h"

class VTK_SLICER_WATCHDOG_MODULE_MRML_EXPORT vtkMRMLWatchdogNode : public vtkMRMLDisplayableNode
{
public:
  static vtkMRMLWatchdogNode* New();
  vtkTypeMacro(vtkMRMLWatchdogNode, vtkMRMLDisplayableNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  //--------------------------------------------------------------------------
  /// MRMLNode methods
  //--------------------------------------------------------------------------

  virtual vtkMRMLNode* CreateNodeInstance();

  /// Get node XML tag name
  virtual const char* GetNodeTagName() { return "Watchdog"; };

  /// Read node attributes from XML file
  virtual void ReadXMLAttributes( const char** atts);

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);
  
  //--------------------------------------------------------------------------
  /// Watchdog-specific methods
  //--------------------------------------------------------------------------

  vtkSetMacro(Visible, bool);
  vtkGetMacro(Visible, bool);

  /// Returns the number of tools that this node watches
  int GetNumberOfWatchedNodes();

  /// Get the display label of the chosen watched node
  const char* GetWatchedNodeDisplayLabel(int watchedNodeIndex);
  /// Set the display label of the chosen watched node
  void SetWatchedNodeDisplayLabel(int watchedNodeIndex, const char* displayLabel);

  /// Get the maximum allowed elapsed time since the last update of the watched node.
  /// If the node is not updated within this time then the node becomes outdated.
  double GetWatchedNodeUpdateTimeToleranceSec(int watchedNodeIndex);
  /// Set the maximum allowed elapsed time since the last update of the watched node.
  void SetWatchedNodeUpdateTimeToleranceSec(int watchedNodeIndex, double updateTimeToleranceSec);

  /// Returns the status computed in the last call UpdateWatchedNodesStatus method call
  bool GetWatchedNodeUpToDate(int watchedNodeIndex);

  /// Get time elapsed since the last update of the selected watched node
  double GetWatchedNodeElapsedTimeSinceLastUpdateSec(int watchedNodeIndex);

  /// Get true if sound should be played when the watched node becomes outdated
  bool GetWatchedNodePlaySound(int watchedNodeIndex);
  /// Enable/disable playing a warning sound when the watched node becomes outdated
  void SetWatchedNodePlaySound(int watchedNodeIndex, bool playSound);

  /// Add a node to be watched. Returns the watched node's index.
  int AddWatchedNode(vtkMRMLNode *watchedNode, const char* displayLabel=NULL, double updateTimeToleranceSec=-1, bool playSound=false);

  /// Remove the specified watched nodes from the list
  void RemoveWatchedNode(int watchedNodeIndex);

  /// Remove all the watched nodes
  void RemoveAllWatchedNodes();

  /// Swap the specified tools watched from the tools' list
  void SwapWatchedNodes( int watchedNodeIndexA, int watchedNodeIndexB );

  /*
  /// Get the index of the watched node with the specified label.
  /// Returns -1 if no watched node exist with the specified label.
  int FindWatchedNodeByDisplayLabel(const char* displayLabel);
  */

  /// Get notification about updates of watched nodes
  virtual void ProcessMRMLEvents ( vtkObject * caller, unsigned long event, void * callData );

  /// Updates the up-to-date status of all watched nodes.
  /// If any of the statuses change then a Modified event is invoked.
  /// A watched node's status is valid if the last update of the node happened not longer time than the update time tolerance.
  void UpdateWatchedNodesStatus();

protected:

  // Constructor/destructor methods
  vtkMRMLWatchdogNode();
  virtual ~vtkMRMLWatchdogNode();
  vtkMRMLWatchdogNode ( const vtkMRMLWatchdogNode& );
  void operator=( const vtkMRMLWatchdogNode& );

  bool Visible;

private:
  class vtkInternal;
  vtkInternal* Internal;
};

#endif
