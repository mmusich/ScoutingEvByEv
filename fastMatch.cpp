// Compilation:
// - bash:
// g++ -O3 fastMatch.cpp $(root-config --cflags --libs) -o fastMatch
// - tcsh
// g++ -O3 -march=native fastMatch.cpp `root-config --cflags --libs` -o fastMatch
//
//
// Exectution:
// ./fastMatch

#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TCanvas.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <cmath>
#include <string>
#include <map>

const int ETA_BINS = 100;
const int PHI_BINS = 100;
const int NEIGHBOURSTOSCAN = 20;
const double ETA_MIN = -4.0;
const double ETA_MAX = 4.0;
const double PHI_MIN = -M_PI;
const double PHI_MAX = M_PI;
const double DR_THRESHOLD = 0.1;

int get_hash(double eta, double phi) {
  int eta_bin = floor((eta - ETA_MIN)/(ETA_MAX-ETA_MIN)*ETA_BINS);
  int phi_bin = floor((phi - PHI_MIN)/(PHI_MAX-PHI_MIN)*PHI_BINS);
  eta_bin = std::max(0, std::min(ETA_BINS-1, eta_bin));
  phi_bin = std::max(0, std::min(PHI_BINS-1, phi_bin));
  return eta_bin * PHI_BINS + phi_bin;
}

double delta_r(double eta1, double phi1, double eta2, double phi2) {
  double deta = eta1 - eta2;
  double dphi = fmod(phi1 - phi2 + M_PI, 2*M_PI) - M_PI;
  return sqrt(deta*deta + dphi*dphi);
}

std::string branch_name(const std::string& var, int pdg_id) {
  std::string tag = (pdg_id < 0) ? "n" + std::to_string(-pdg_id)
                                 : std::to_string(pdg_id);
  return "PF_" + var + "_" + tag;
  // -> "PF_eta_n211"
}

int main() {
  const char* HLT_FILE    = "hlt_output.root";
  const char* PROMPT_FILE = "prompt_output.root";
  const char* TREE_NAME   = "scoutingCollectionNtuplizer/tree";

  // particle types
  const std::vector<int> pdg_ids = {211, -211, 130, 22, 13, -13, 1, 2};

  TFile f_hlt(HLT_FILE);
  TFile f_prompt(PROMPT_FILE);

  TTree* t_hlt    = (TTree*)f_hlt   .Get(TREE_NAME);
  TTree* t_prompt = (TTree*)f_prompt.Get(TREE_NAME);

  ULong64_t event_hlt, event_prompt;
  t_hlt   ->SetBranchAddress("event", &event_hlt);
  t_prompt->SetBranchAddress("event", &event_prompt);

  // per-particle branch pointers & histograms 
  struct ParticleData {
    // HLT
    std::vector<float>* eta_hlt = nullptr;
    std::vector<float>* phi_hlt = nullptr;
    std::vector<float>* pt_hlt  = nullptr;
    // Prompt
    std::vector<float>* eta_prompt = nullptr;
    std::vector<float>* phi_prompt = nullptr;
    std::vector<float>* pt_prompt  = nullptr;
    // Histograms
    TH1F* h_pt_diff  = nullptr;
    TH1F* h_eta_diff = nullptr;
    TH1F* h_phi_diff = nullptr;
    TH1F* h_dr       = nullptr;
  };

  std::map<int, ParticleData> particles;

  for (int pdg : pdg_ids) {
    ParticleData& p = particles[pdg];
    std::string tag = std::to_string(pdg);

    // Wire up HLT branches
    t_hlt->SetBranchAddress(branch_name("eta", pdg).c_str(), &p.eta_hlt);
    t_hlt->SetBranchAddress(branch_name("phi", pdg).c_str(), &p.phi_hlt);
    t_hlt->SetBranchAddress(branch_name("pT",  pdg).c_str(), &p.pt_hlt);

    // Wire up Prompt branches
    t_prompt->SetBranchAddress(branch_name("eta", pdg).c_str(), &p.eta_prompt);
    t_prompt->SetBranchAddress(branch_name("phi", pdg).c_str(), &p.phi_prompt);
    t_prompt->SetBranchAddress(branch_name("pT",  pdg).c_str(), &p.pt_prompt);

    // Book histograms
    p.h_pt_diff  = new TH1F(("pt_diff_"  + tag).c_str(), ("pT(Prompt-HLT) pdg="  + tag).c_str(), 100, -5,   5  );
    p.h_eta_diff = new TH1F(("eta_diff_" + tag).c_str(), ("eta(Prompt-HLT) pdg=" + tag).c_str(), 100, -0.1, 0.1);
    p.h_phi_diff = new TH1F(("phi_diff_" + tag).c_str(), ("phi(Prompt-HLT) pdg=" + tag).c_str(), 100, -0.1, 0.1);
    p.h_dr       = new TH1F(("dr_"       + tag).c_str(), ("DeltaR pdg="           + tag).c_str(), 100,  0,   0.05);
  }

  // build prompt event index
  std::unordered_map<ULong64_t, ULong64_t> prompt_event_map;
  ULong64_t n_prompt = t_prompt->GetEntries();
  for (ULong64_t i = 0; i < n_prompt; i++) {
    t_prompt->GetEntry(i);
    prompt_event_map[event_prompt] = i;
  }

  // main matching loop
  ULong64_t n_hlt = t_hlt->GetEntries();

  for (ULong64_t i = 0; i < n_hlt; i++) {
    t_hlt->GetEntry(i);

    if (!prompt_event_map.count(event_hlt)) continue;
    t_prompt->GetEntry(prompt_event_map[event_hlt]);

    // loop over particle types
    for (int pdg : pdg_ids) {
      ParticleData& p = particles[pdg];

      const auto& eta_h = *p.eta_hlt;
      const auto& phi_h = *p.phi_hlt;
      const auto& pt_h  = *p.pt_hlt;

      const auto& eta_pr = *p.eta_prompt;
      const auto& phi_pr = *p.phi_prompt;
      const auto& pt_pr  = *p.pt_prompt;

      // Build hash vectors
      std::vector<int> hlt_hash   (eta_h .size());
      std::vector<int> prompt_hash(eta_pr.size());

      for (size_t j = 0; j < eta_h .size(); j++) hlt_hash   [j] = get_hash(eta_h [j], phi_h [j]);
      for (size_t j = 0; j < eta_pr.size(); j++) prompt_hash[j] = get_hash(eta_pr[j], phi_pr[j]);

      // Prompt spatial map
      std::unordered_map<int, std::vector<int>> prompt_map;
      for (size_t j = 0; j < prompt_hash.size(); j++)
        prompt_map[prompt_hash[j]].push_back(j);

      std::set<int> used_prompt;

      // Match each HLT particle to its nearest prompt neighbour
      for (size_t h = 0; h < hlt_hash.size(); h++) {
        int hash    = hlt_hash[h];
        int eta_bin = hash / PHI_BINS;
        int phi_bin = hash % PHI_BINS;

        double best_dr  = DR_THRESHOLD;
        int    best_idx = -1;

        for (int d_eta = -NEIGHBOURSTOSCAN; d_eta <= NEIGHBOURSTOSCAN; d_eta++) {
          int eta_n = eta_bin + d_eta;
          if (eta_n < 0 || eta_n >= ETA_BINS) continue;

          for (int d_phi = -NEIGHBOURSTOSCAN; d_phi <= NEIGHBOURSTOSCAN; d_phi++) {
            int phi_n    = (phi_bin + d_phi + PHI_BINS) % PHI_BINS;
            int neighbor = eta_n * PHI_BINS + phi_n;

            if (!prompt_map.count(neighbor)) continue;

            for (int p_idx : prompt_map[neighbor]) {
              if (used_prompt.count(p_idx)) continue;

              double dr = delta_r(eta_h[h], phi_h[h], eta_pr[p_idx], phi_pr[p_idx]);
              if (dr < best_dr) { best_dr = dr; best_idx = p_idx; }
            }
          }
        }

        if (best_idx != -1) {
          used_prompt.insert(best_idx);
          p.h_pt_diff ->Fill(pt_pr [best_idx] - pt_h [h]);
          p.h_eta_diff->Fill(eta_pr[best_idx] - eta_h[h]);
          p.h_phi_diff->Fill(phi_pr[best_idx] - phi_h[h]);
          p.h_dr      ->Fill(best_dr);
        }
      }
    } // end particle loop
  }   // end event loop

  // save histograms
  TFile fout("matching_plots.root", "RECREATE");
  for (int pdg : pdg_ids) {
    ParticleData& p = particles[pdg];
    p.h_pt_diff ->Write();
    p.h_eta_diff->Write();
    p.h_phi_diff->Write();
    p.h_dr      ->Write();
  }
  fout.Close();

  std::cout << "Plots saved to matching_plots.root" << std::endl;
}
