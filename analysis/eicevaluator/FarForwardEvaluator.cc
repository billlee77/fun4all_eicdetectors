#include "FarForwardEvaluator.h"

#include <g4main/PHG4Particle.h>
#include <g4main/PHG4Shower.h>
#include <g4main/PHG4TruthInfoContainer.h>
#include <g4main/PHG4VtxPoint.h>

#include <g4vertex/GlobalVertex.h>
#include <g4vertex/GlobalVertexMap.h>

#include <calobase/RawCluster.h>
#include <calobase/RawClusterContainer.h>
#include <calobase/RawClusterUtility.h>
#include <calobase/RawTower.h>
#include <calobase/RawTowerContainer.h>
#include <calobase/RawTowerGeom.h>
#include <calobase/RawTowerGeomContainer.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/SubsysReco.h>

#include <phool/getClass.h>
#include <phool/phool.h>

#include <TFile.h>
#include <TNtuple.h>
#include <TTree.h>

#include <CLHEP/Vector/ThreeVector.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <utility>

#include <sstream>

#include <TSystem.h>

// G4Hits includes
#include <g4main/PHG4Hit.h>
#include <g4main/PHG4HitContainer.h>

FarForwardEvaluator::FarForwardEvaluator(const std::string& name, const std::string& ffrname, const std::string& filename, const std::string& ip_str)
  : SubsysReco(name)
  , _ffrname(ffrname)
  , _ip_str(ip_str)
  , g4hitntuple(nullptr)
  , g4b0hitntuple(nullptr)
  , g4rphitntuple(nullptr)
  , h2_ZDC_XY(nullptr)
  , h2_ZDC_XY_double(nullptr)
  , h2_B0_XY(nullptr)
  , h2_RP_XY(nullptr)
  , hm(nullptr)
  , _ievent(0)
  , _towerID_debug(0)
  , _ieta_debug(0)
  , _iphi_debug(0)
  , _eta_debug(0)
  , _phi_debug(0)
  , _e_debug(0)
  , _x_debug(0)
  , _y_debug(0)
  , _z_debug(0)
  , _truth_trace_embed_flags()
  //, _truth_e_threshold(0.0)
  //, 0 GeV before reco is traced
  //,_reco_e_threshold(0.0)
  //, 0 GeV before reco is traced
  //,_caloevalstack(nullptr)
  //, _strict(false)
  , _do_gpoint_eval(true)
  , _do_gshower_eval(true)
  , _do_tower_eval(true)
  , _do_cluster_eval(true)
  , _ntp_gpoint(nullptr)
  , _ntp_gshower(nullptr)
  , _ntp_tower(nullptr)
  , _tower_debug(nullptr)
  , _ntp_cluster(nullptr)
  , ZDC_hit(0)
  , event_itt(0)
  , _filename(filename)
  , _tfile(nullptr)
  , h1_E_dep_smeared(nullptr)
  , h1_E_dep(nullptr)
  , h1_B0_E_dep(nullptr)
  , h1_B0_E(nullptr)
  , h1_B0_E_abs(nullptr)
  , h1_RP_E_dep(nullptr)
  , h1_RP_E_abs(nullptr)
{
}

int FarForwardEvaluator::Init(PHCompositeNode* topNode)
{
  _ievent = 0;

  _tfile = new TFile(_filename.c_str(), "RECREATE");

  if (_do_gpoint_eval) _ntp_gpoint = new TNtuple("ntp_gpoint", "primary vertex => best (first) vertex",
                                                 "event:gvx:gvy:gvz:"
                                                 "vx:vy:vz");

  if (_do_gshower_eval) _ntp_gshower = new TNtuple("ntp_gshower", "truth shower => best cluster",
                                                   "event:gparticleID:gflavor:gnhits:"
                                                   "geta:gphi:ge:gpt:gvx:gvy:gvz:gembed:gedep:"
                                                   "clusterID:ntowers:eta:x:y:z:phi:e:efromtruth");

  //Barak: Added TTree to will allow the TowerID to be set correctly as integer
  if (_do_tower_eval)
  {
    _ntp_tower = new TNtuple("ntp_tower", "tower => max truth primary",
                             "event:towerID:ieta:iphi:eta:phi:e:x:y:z:"
                             "gparticleID:gflavor:gnhits:"
                             "geta:gphi:ge:gpt:gvx:gvy:gvz:"
                             "gembed:gedep:"
                             "efromtruth");

    //Make Tree
    _tower_debug = new TTree("tower_debug", "tower => max truth primary");

    _tower_debug->Branch("event", &_ievent, "event/I");
    _tower_debug->Branch("towerID", &_towerID_debug, "towerID/I");
    _tower_debug->Branch("ieta", &_ieta_debug, "ieta/I");
    _tower_debug->Branch("iphi", &_iphi_debug, "iphi/I");
    _tower_debug->Branch("eta", &_eta_debug, "eta/F");
    _tower_debug->Branch("phi", &_phi_debug, "phi/F");
    _tower_debug->Branch("e", &_e_debug, "e/F");
    _tower_debug->Branch("x", &_x_debug, "x/F");
    _tower_debug->Branch("y", &_y_debug, "y/F");
    _tower_debug->Branch("z", &_z_debug, "z/F");
  }

  if (_do_cluster_eval) _ntp_cluster = new TNtuple("ntp_cluster", "cluster => max truth primary",
                                                   "event:clusterID:ntowers:eta:x:y:z:phi:e:"
                                                   "gparticleID:gflavor:gnhits:"
                                                   "geta:gphi:ge:gpt:gvx:gvy:gvz:"
                                                   "gembed:gedep:"
                                                   "efromtruth");

  hm = new Fun4AllHistoManager(Name());

  g4hitntuple = new TNtuple("hitntup", "G4Hits", "x0:y0:z0:x1:y1:z1:edep");
  g4b0hitntuple = new TNtuple("b0hit", "B0 Hits", "layer:type:x0:y0:z0:x1:y1:z1:t0:t1:edep");
  g4rphitntuple = new TNtuple("rphit", "RP Hits", "layer:type:x0:y0:z0:x1:y1:z1:t0:t1:edep");

  std::cout << "diff_tagg_ana::Init(PHCompositeNode *topNode) Initializing" << std::endl;

  event_itt = 0;

  //----------------------------
  // ZDC Occupancy

  gDirectory->mkdir("ZDC");
  gDirectory->cd("ZDC");

  h2_ZDC_XY = new TH2F("ZDC_XY", "ZDC XY", 200, -50, 50, 200, -50, 50);

  h2_ZDC_XY_double = new TH2F("ZDC_XY_double", "ZDC XY Double gamma", 200, -50, 50, 200, -50, 50);

  h1_E_dep = new TH1F("E_dep", "E Dependence", 120, 0.0, 60.0);

  h1_E_dep_smeared = new TH1F("E_dep_smeared", "E Dependence Smeared", 120, 0.0, 60.0);

  gDirectory->cd("/");

  //----------------------------
  // B0 Occupancy

  gDirectory->mkdir("B0");
  gDirectory->cd("B0");

  h2_B0_XY = new TH2F("B0_XY", "B0 XY", 400, -200, 200, 200, -50, 50);

  h1_B0_E_dep = new TH1F("B0_E_dep", "E Dependence", 120, 0.0, 60.0);
  h1_B0_E_abs = new TH1F("B0_E_abs", "E Absorbed", 120, 0.0, 60.0);
  h1_B0_E = new TH1F("B0_E", "E layer", 10, 0.0, 10.0);

  gDirectory->cd("/");

  //----------------------------
  // RP Occupancy

  gDirectory->mkdir("RP");
  gDirectory->cd("RP");

  h2_RP_XY = new TH2F("RP_XY", "RP XY;X (cm);Y (cm)", 400, -200, 200, 200, -50, 50);
  h1_RP_E_dep = new TH1F("RP_E_dep", "E Dependence", 120, 0.0, 60.0);
  h1_RP_E_abs = new TH1F("RP_E_abs", "E Absorbed", 120, 0.0, 60.0);

  std::string paramFile = std::string(getenv("CALIBRATIONROOT")) + "/RomanPots/RP_parameters_IP6.dat";
  int Nlayers = GetParameterFromFile <int> (paramFile, "Number_layers");

  for( int layer = 0; layer < Nlayers; layer++ ) {
	h2_RP_layers_XY.push_back( new TH2F(Form("RP_XY_layer%i",layer),"RP XY;X (cm);Y (cm)", 400, -200, 200, 200, -50, 50) );
	h2_RP_virtlayers_XY.push_back( new TH2F(Form("RP_XY_virtlayer%i",layer),"RP XY;X (cm);Y (cm)", 2000, -200, 0, 200, -10, 10) );
  }

  gDirectory->cd("/");

  return Fun4AllReturnCodes::EVENT_OK;
}
//
int FarForwardEvaluator::process_event(PHCompositeNode* topNode)
{
  ZDC_hit = 0;

  event_itt++;

  if (event_itt % 100 == 0)
    std::cout << "Event Processing Counter: " << event_itt << std::endl;

  process_g4hits_ZDC(topNode);

  process_g4hits_RomanPots(topNode);

  process_g4hits_B0(topNode);

  return Fun4AllReturnCodes::EVENT_OK;
}

//***************************************************

int FarForwardEvaluator::process_g4hits_ZDC(PHCompositeNode* topNode)
{
  std::ostringstream nodename;

  nodename.str("");
  nodename << "G4HIT_"
           << "ZDCsurrogate";

  PHG4HitContainer* hits = findNode::getClass<PHG4HitContainer>(topNode, nodename.str().c_str());

  if (hits)
  {
    PHG4HitContainer::ConstRange hit_range = hits->getHits();
    for (PHG4HitContainer::ConstIterator hit_iter = hit_range.first; hit_iter != hit_range.second; hit_iter++)
    {
      ZDC_hit++;
    }

    for (PHG4HitContainer::ConstIterator hit_iter = hit_range.first; hit_iter != hit_range.second; hit_iter++)
    {
      float smeared_E = 0;
      g4hitntuple->Fill(hit_iter->second->get_x(0),
                        hit_iter->second->get_y(0),
                        hit_iter->second->get_z(0),
                        hit_iter->second->get_x(1),
                        hit_iter->second->get_y(1),
                        hit_iter->second->get_z(1),
                        hit_iter->second->get_edep());

      float x_offset;

      if (_ip_str == "IP6")
      {
        x_offset = 90;
      }
      else
      {
        x_offset = -120;
      }

      h2_ZDC_XY->Fill(hit_iter->second->get_x(0) + x_offset, hit_iter->second->get_y(0));

      //
      //      smeared_E = EMCAL_Smear(hit_iter->second->get_edep());
      smeared_E = hit_iter->second->get_edep();
      //
      if (ZDC_hit == 2)
      {
        //      cout << hit_iter->second->get_x(0)-90 << "   " << hit_iter->second->get_y(0) << endl;
        //        h2_ZDC_XY_double->Fill(hit_iter->second->get_x(0)-90, hit_iter->second->get_y(0));
        h2_ZDC_XY_double->Fill(hit_iter->second->get_x(0) + x_offset, hit_iter->second->get_y(0));
        //      h1_E_dep->Fill(hit_iter->second->get_edep());

        h1_E_dep->Fill(hit_iter->second->get_edep());
        h1_E_dep_smeared->Fill(smeared_E);
        //
      }
    }
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

//***************************************************
// Getting the RomanPots hits

int FarForwardEvaluator::process_g4hits_RomanPots(PHCompositeNode* topNode)
{
  std::string nodename = "G4HIT_rpTruth";
  PHG4HitContainer* hits = findNode::getClass<PHG4HitContainer>(topNode, nodename);

  std::string nodenameVirt = nodename + "_VirtSheet";
  PHG4HitContainer* hits_virt = findNode::getClass<PHG4HitContainer>(topNode, nodenameVirt);

  if (hits)
  {
    // this returns an iterator to the beginning and the end of our G4Hits
    PHG4HitContainer::ConstRange hit_range = hits->getHits();

    for (PHG4HitContainer::ConstIterator hit_iter = hit_range.first; hit_iter != hit_range.second; hit_iter++)
    {
      h2_RP_XY->Fill(hit_iter->second->get_x(0), hit_iter->second->get_y(0));
      
      h2_RP_layers_XY[ hit_iter->second->get_layer() ]->Fill(hit_iter->second->get_x(0), hit_iter->second->get_y(0));

      g4rphitntuple->Fill(hit_iter->second->get_layer(),
                          hit_iter->second->get_hit_type(),
                          hit_iter->second->get_x(0),
                          hit_iter->second->get_y(0),
                          hit_iter->second->get_z(0),
                          hit_iter->second->get_x(1),
                          hit_iter->second->get_y(1),
                          hit_iter->second->get_z(1),
                          hit_iter->second->get_t(0),
                          hit_iter->second->get_t(1),
                          hit_iter->second->get_edep());
      if (hit_iter->second->get_hit_type())
        h1_RP_E_dep->Fill(hit_iter->second->get_edep());
      else
        h1_RP_E_abs->Fill(hit_iter->second->get_edep());
    }
  }

  // hits on virtual layer without beam hole
  if( hits_virt ) {

	  PHG4HitContainer::ConstRange hit_range = hits_virt->getHits();

	  for (PHG4HitContainer::ConstIterator hit_iter = hit_range.first; hit_iter != hit_range.second; hit_iter++)
	  {
		  h2_RP_virtlayers_XY[ hit_iter->second->get_layer() ]->Fill(hit_iter->second->get_x(0), hit_iter->second->get_y(0));
	  }

  }

  return Fun4AllReturnCodes::EVENT_OK;
}

//-----------------------------------

//***************************************************
// Getting the B0 hits

int FarForwardEvaluator::process_g4hits_B0(PHCompositeNode* topNode)
{
  std::ostringstream nodename;

  nodename.str("");
  nodename << "G4HIT_"
           << "b0Truth";

  PHG4HitContainer* hits = findNode::getClass<PHG4HitContainer>(topNode, nodename.str().c_str());

  if (hits)
  {
    //    // this returns an iterator to the beginning and the end of our G4Hits
    PHG4HitContainer::ConstRange hit_range = hits->getHits();

    for (PHG4HitContainer::ConstIterator hit_iter = hit_range.first; hit_iter != hit_range.second; hit_iter++)
    {
      //	cout << "B0 hits? " << endl;
      //	cout << "This is where you can fill your loop " << endl;

      h2_B0_XY->Fill(hit_iter->second->get_x(0), hit_iter->second->get_y(0));
      g4b0hitntuple->Fill(hit_iter->second->get_layer(),
                          hit_iter->second->get_hit_type(),
                          hit_iter->second->get_x(0),
                          hit_iter->second->get_y(0),
                          hit_iter->second->get_z(0),
                          hit_iter->second->get_x(1),
                          hit_iter->second->get_y(1),
                          hit_iter->second->get_z(1),
                          hit_iter->second->get_t(0),
                          hit_iter->second->get_t(1),
                          hit_iter->second->get_edep());
      if (hit_iter->second->get_hit_type())
        h1_B0_E_dep->Fill(hit_iter->second->get_edep());
      else
        h1_B0_E_abs->Fill(hit_iter->second->get_edep());
      h1_B0_E->Fill(hit_iter->second->get_layer(), hit_iter->second->get_edep());
    }
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

//***************************************************

int FarForwardEvaluator::End(PHCompositeNode* topNode)
{
  _tfile->cd();

  g4hitntuple->Write();
  g4b0hitntuple->Write();
  g4rphitntuple->Write();
  _tfile->Write();
  _tfile->Close();
  delete _tfile;

  return Fun4AllReturnCodes::EVENT_OK;
}

//--------------------------------------------------------
template <class T>
T GetParameterFromFile(std::string filename, std::string param)
{
	std::ifstream infile;
        std::string line;

	infile.open( filename );

	if( ! infile.is_open() ) 
	{
		std::cout << "ERROR in FarForwardEvaluator: Failed to open parameter file " << filename << std::endl;
		gSystem->Exit(1);
	}

	while( std::getline(infile, line) ) {

	    std::string name;
	    double value;

	    std::istringstream iss( line );

	    // skip comment lines
	    if( line.find("#") != std::string::npos ) { continue; }

	    if( !(iss >> name >> value) ) {
		std::cout << "Could not decode " << line << std::endl;
		gSystem->Exit(1);
	    }
	    
            if( name.compare(param) == 0 ) {
		    return value;
	    }
	}

        infile.close();
	return 0;
}

