#include "G4EicDircSubsystem.h"

#include "G4EicDircDetector.h"
#include "G4EicDircDisplayAction.h"
#include "G4EicDircSteppingAction.h"

#include <phparameter/PHParameters.h>

#include <g4main/PHG4HitContainer.h>
#include <g4main/PHG4SteppingAction.h>  // for PHG4SteppingAction

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>    // for PHIODataNode
#include <phool/PHNode.h>          // for PHNode
#include <phool/PHNodeIterator.h>  // for PHNodeIterator
#include <phool/PHObject.h>        // for PHObject
#include <phool/getClass.h>

#include <cmath>  // for isfinite

using namespace std;

//_______________________________________________________________________
G4EicDircSubsystem::G4EicDircSubsystem(const std::string &name)
  : PHG4DetectorSubsystem(name)
  , m_Detector(nullptr)
  , m_SteppingAction(nullptr)
  , m_DisplayAction(nullptr)
{
  // call base class method which will set up parameter infrastructure
  // and call our SetDefaultParameters() method
  InitializeParameters();
}

//_______________________________________________________________________
G4EicDircSubsystem::~G4EicDircSubsystem()
{
  delete m_DisplayAction;
}

//_______________________________________________________________________
int G4EicDircSubsystem::InitRunSubsystem(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));
  // G4EicDircDisplayAction *disp_action = new G4EicDircDisplayAction(Name(), GetParams());
  // if (isfinite(m_ColorArray[0]) &&
  //     isfinite(m_ColorArray[1]) &&
  //     isfinite(m_ColorArray[2]) &&
  //     isfinite(m_ColorArray[3]))
  // {
  //   disp_action->SetColor(m_ColorArray[0], m_ColorArray[1], m_ColorArray[2], m_ColorArray[3]);
  // }
  // m_DisplayAction = disp_action;
  // create detector
  m_Detector = new G4EicDircDetector(this, topNode, GetParams(), Name());
  m_Detector->SuperDetector(SuperDetector());
  m_Detector->OverlapCheck(CheckOverlap());

  string detector_suffix = SuperDetector();
  if (detector_suffix == "NONE")
  {
    detector_suffix = SuperDetector();
  }

  std::set<std::string> nodes;
  if (GetParams()->get_int_param("active"))
  {
    PHNodeIterator dstIter(dstNode);
    PHCompositeNode *DetNode = dstNode;
    if (SuperDetector() != "NONE")
    {
      DetNode = dynamic_cast<PHCompositeNode*>(dstIter.findFirst("PHCompositeNode", SuperDetector()));
      if (!DetNode)
      {
        DetNode = new PHCompositeNode(SuperDetector());
        dstNode->addNode(DetNode);
      }
    }
    m_HitNodeName = "G4HIT_" + detector_suffix;
    nodes.insert(m_HitNodeName);
   m_AbsorberNodeName = "G4HIT_ABSORBER_" + detector_suffix;
    if (GetParams()->get_int_param("absorberactive"))
    {
      nodes.insert(m_AbsorberNodeName);
    }
    m_SupportNodeName = "G4HIT_SUPPORT_" + detector_suffix;
    if (GetParams()->get_int_param("supportactive"))
    {
      nodes.insert(m_SupportNodeName);
    }
    for (auto thisnode : nodes)
    {
      PHG4HitContainer* g4_hits = findNode::getClass<PHG4HitContainer>(topNode, thisnode);
      if (!g4_hits)
      {
        g4_hits = new PHG4HitContainer(thisnode);
        DetNode->addNode(new PHIODataNode<PHObject>(g4_hits, thisnode, "PHObject"));
      }
    }
    G4EicDircSteppingAction *tmp = new G4EicDircSteppingAction(m_Detector, GetParams());
    tmp->SetHitNodeName(m_HitNodeName);
    tmp->SetAbsorberNodeName(m_AbsorberNodeName);
    tmp->SetSupportNodeName(m_SupportNodeName);
    m_SteppingAction = tmp;
  }
  return 0;
}

//_______________________________________________________________________
int G4EicDircSubsystem::process_event(PHCompositeNode *topNode)
{
  // pass top node to stepping action so that it gets
  // relevant nodes needed internally
  if (m_SteppingAction)
  {
    m_SteppingAction->SetInterfacePointers(topNode);
  }
  return 0;
}

void G4EicDircSubsystem::Print(const string &what) const
{
  if (m_Detector)
  {
    m_Detector->Print(what);
  }
  return;
}

//_______________________________________________________________________
PHG4Detector *G4EicDircSubsystem::GetDetector(void) const
{
  return m_Detector;
}

void G4EicDircSubsystem::SetDefaultParameters()
{
  // sizes are in cm
  // angles are in deg
  // units will be converted to G4 units when used
  set_default_double_param("place_x", 0.);
  set_default_double_param("place_y", 0.);
  set_default_double_param("place_z", 0.);
  set_default_double_param("rot_x", 0.);
  set_default_double_param("rot_y", 0.);
  set_default_double_param("rot_z", 0.);
  set_default_double_param("size_x", 20.);
  set_default_double_param("size_y", 20.);
  set_default_double_param("size_z", 20.);

  set_default_string_param("material", "G4_Galactic");
}
