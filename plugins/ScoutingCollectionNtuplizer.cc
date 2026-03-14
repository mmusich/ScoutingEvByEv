// -*- C++ -*-
//
// Package:    Scouting/Ntuplizer
// Class:      ScoutingCollectionNtuplizer
//
/**\class ScoutingCollectionNtuplizer ScoutingCollectionNtuplizer.cc
          Scouting/Ntuplizer/plugins/ScoutingCollectionNtuplizer.cc

Description: Ntuplizer for scouting PF candidates (for now).
*/
//
// Original Author:  Jessica Prendi
//         Created:  Mon, 29 Sep 2025 12:04:23 GMT
//
//

#include <memory>
#include <vector>
#include <string>
#include <map>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "DataFormats/Scouting/interface/Run3ScoutingParticle.h"

#include <TTree.h>

// defining a structure as it avoids repetition for the PFcands (where the tree output will either way be structured identically)
struct ParticleVars {
  std::vector<float> pT;
  std::vector<float> eta;
  std::vector<float> phi;
  std::vector<float> vertex;
  std::vector<float> normchi2;
  std::vector<float> dz;
  std::vector<float> dxy;
  std::vector<float> dzsig;
  std::vector<float> dxysig;
  std::vector<float> trk_pt;
  std::vector<float> trk_eta;
  std::vector<float> trk_phi;

  // define this clear function to remove content within the particleData_, as the std::map::clear function would otherwise clear the whole map !
  void clear() {
    pT.clear();
    eta.clear();
    phi.clear();
    vertex.clear();
    normchi2.clear();
    dz.clear();
    dxy.clear();
    dzsig.clear();
    dxysig.clear();
    trk_pt.clear();
    trk_eta.clear();
    trk_phi.clear();
  }
};

class ScoutingCollectionNtuplizer : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
  explicit ScoutingCollectionNtuplizer(const edm::ParameterSet&);
  ~ScoutingCollectionNtuplizer() override = default;
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void createTree();
  void analyze(const edm::Event&, const edm::EventSetup&) override;

  template <typename T>
  bool getValidHandle(const edm::Event& iEvent, const edm::EDGetTokenT<T>& token, edm::Handle<T>& handle) {
    iEvent.getByToken(token, handle);
    return handle.isValid();
  }

  const edm::EDGetTokenT<std::vector<Run3ScoutingParticle>> pfcandsToken_;
  TTree* tree_;

  // map to simplify the tree filling logic etc for the PF Cands
  // int is the pdgId key then the value is simply the above defined struct
  std::map<int, ParticleVars> particleData_;

  // A helper to keep track of which particle types we want to save
  const std::vector<int> pdgIdsToKeep_ = {211, -211, 130, 22, 13, -13, 1, 2};

  unsigned int run_, lumi_;
  ULong64_t event_;
};

ScoutingCollectionNtuplizer::ScoutingCollectionNtuplizer(const edm::ParameterSet& iConfig)
    : pfcandsToken_(consumes<std::vector<Run3ScoutingParticle>>(iConfig.getParameter<edm::InputTag>("pfcands"))) {
  usesResource("TFileService");
  createTree();
}

void ScoutingCollectionNtuplizer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;

  run_ = iEvent.eventAuxiliary().run();
  event_ = iEvent.eventAuxiliary().event();
  lumi_ = iEvent.eventAuxiliary().luminosityBlock();

  edm::Handle<std::vector<Run3ScoutingParticle>> pfcandsH;
  if (!getValidHandle(iEvent, pfcandsToken_, pfcandsH)) {
    return;
  }

  // here we are using the function that we defined above in the struct
  // what happens here is that we loop over all the "keys" in our map particleData_, accessing the struct ParticleVars that contains several vectors
  // with .second we access the values (so the struct particleData_) and then we remove it with our predefined function clear within the struct..
  for (auto& pair : particleData_) {
    pair.second.clear();
  }

  // loops over pfCands
  for (const auto& cand : *pfcandsH) {
    // looks whether the pdgId within the pfCands event matches with the pdgIds we want to store .
    // .find gives back either the iterator or returns .end() if it is not found which is what we check for later
    auto it = particleData_.find(cand.pdgId());

    // here we check whether "finding" the pdgId worked
    if (it != particleData_.end()) {
      // from the it, we only care about the second because this is where we have our ParticleVars struct saved in that contains all the vectors!
      ParticleVars& data = it->second;
      data.pT.push_back(cand.pt());
      data.eta.push_back(cand.eta());
      data.phi.push_back(cand.phi());
      data.vertex.push_back(cand.vertex());
      data.normchi2.push_back(cand.normchi2());
      data.dz.push_back(cand.dz());
      data.dxy.push_back(cand.dxy());
      data.dzsig.push_back(cand.dzsig());
      data.dxysig.push_back(cand.dxysig());
      data.trk_pt.push_back(cand.trk_pt());
      data.trk_eta.push_back(cand.trk_eta());
      data.trk_phi.push_back(cand.trk_phi());
    }
  }

  tree_->Fill();
}

void ScoutingCollectionNtuplizer::createTree() {
  edm::Service<TFileService> fs;
  tree_ = fs->make<TTree>("tree", "tree");

  tree_->Branch("run", &run_, "run/i");
  tree_->Branch("lumi", &lumi_, "lumi/i");
  tree_->Branch("event", &event_, "event/l");

  for (int pdgId : pdgIdsToKeep_) {
    // map.emplace will return an std::pair, the first entry being the iterator and the secon done being a bool that tells us whether the emplacing was successful
    // the emplace function on maps will only work in case the key is new so all keys in a map need to be unique
    ParticleVars& data = particleData_.emplace(pdgId, ParticleVars()).first->second;

    // using std::to_string needs to have a name assigned for the negative signed pdgId
    std::string id_str = std::to_string(pdgId);
    if (pdgId < 0) {
      id_str = "n" + std::to_string(std::abs(pdgId));
    }

    tree_->Branch(("PF_pT_" + id_str).c_str(), &data.pT);
    tree_->Branch(("PF_eta_" + id_str).c_str(), &data.eta);
    tree_->Branch(("PF_phi_" + id_str).c_str(), &data.phi);
    tree_->Branch(("PF_vertex_" + id_str).c_str(), &data.vertex);
    tree_->Branch(("PF_normchi2_" + id_str).c_str(), &data.normchi2);
    tree_->Branch(("PF_dz_" + id_str).c_str(), &data.dz);
    tree_->Branch(("PF_dxy_" + id_str).c_str(), &data.dxy);
    tree_->Branch(("PF_dzsig_" + id_str).c_str(), &data.dzsig);
    tree_->Branch(("PF_dxysig_" + id_str).c_str(), &data.dxysig);
    tree_->Branch(("PF_trk_pt_" + id_str).c_str(), &data.trk_pt);
    tree_->Branch(("PF_trk_eta_" + id_str).c_str(), &data.trk_eta);
    tree_->Branch(("PF_trk_phi_" + id_str).c_str(), &data.trk_phi);
  }
}

void ScoutingCollectionNtuplizer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("pfcands", edm::InputTag("hltScoutingPFPacker"));
  descriptions.addWithDefaultLabel(desc);
}

DEFINE_FWK_MODULE(ScoutingCollectionNtuplizer);
