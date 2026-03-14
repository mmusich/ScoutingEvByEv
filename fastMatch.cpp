// Compilation:
// - bash:
// g++ -O3 fastMatch.cpp $(root-config --cflags --libs) -o fastMatch
// - tcsh
// g++ -O3 -march=native fastMatch.cpp `root-config --cflags --libs` -o fastMatch

#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TCanvas.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <cmath>

const int ETA_BINS = 100;
const int PHI_BINS = 100;
const double ETA_MIN = -3.0;
const double ETA_MAX = 3.0;
const double PHI_MIN = -M_PI;
const double PHI_MAX = M_PI;
const double DR_THRESHOLD = 0.1;

int get_hash(double eta, double phi)
{
    int eta_bin = floor((eta - ETA_MIN)/(ETA_MAX-ETA_MIN)*ETA_BINS);
    int phi_bin = floor((phi - PHI_MIN)/(PHI_MAX-PHI_MIN)*PHI_BINS);

    eta_bin = std::max(0,std::min(ETA_BINS-1,eta_bin));
    phi_bin = std::max(0,std::min(PHI_BINS-1,phi_bin));

    return eta_bin * PHI_BINS + phi_bin;
}

double delta_r(double eta1,double phi1,double eta2,double phi2)
{
    double deta = eta1 - eta2;
    double dphi = fmod(phi1 - phi2 + M_PI, 2*M_PI) - M_PI;
    return sqrt(deta*deta + dphi*dphi);
}

int main()
{
    const char* HLT_FILE = "hlt_output.root";
    const char* PROMPT_FILE = "prompt_output.root";
    const char* TREE_NAME = "scoutingCollectionNtuplizer/tree";

    TFile f_hlt(HLT_FILE);
    TFile f_prompt(PROMPT_FILE);

    TTree* t_hlt = (TTree*)f_hlt.Get(TREE_NAME);
    TTree* t_prompt = (TTree*)f_prompt.Get(TREE_NAME);

    std::vector<float>* eta_hlt=nullptr;
    std::vector<float>* phi_hlt=nullptr;
    std::vector<float>* pt_hlt=nullptr;

    std::vector<float>* eta_prompt=nullptr;
    std::vector<float>* phi_prompt=nullptr;
    std::vector<float>* pt_prompt=nullptr;

    ULong64_t  event_hlt;
    ULong64_t  event_prompt;

    t_hlt->SetBranchAddress("event",&event_hlt);
    t_hlt->SetBranchAddress("PF_eta_13",&eta_hlt);
    t_hlt->SetBranchAddress("PF_phi_13",&phi_hlt);
    t_hlt->SetBranchAddress("PF_pT_13",&pt_hlt);

    t_prompt->SetBranchAddress("event",&event_prompt);
    t_prompt->SetBranchAddress("PF_eta_13",&eta_prompt);
    t_prompt->SetBranchAddress("PF_phi_13",&phi_prompt);
    t_prompt->SetBranchAddress("PF_pT_13",&pt_prompt);

    TH1F* h_pt_diff  = new TH1F("pt_diff","pT(Prompt-HLT)",100,-5,5);
    TH1F* h_eta_diff = new TH1F("eta_diff","eta(Prompt-HLT)",100,-0.1,0.1);
    TH1F* h_phi_diff = new TH1F("phi_diff","phi(Prompt-HLT)",100,-0.1,0.1);
    TH1F* h_dr       = new TH1F("dr","DeltaR",100,0,0.1);

    std::unordered_map<ULong64_t ,ULong64_t > prompt_event_map;

    ULong64_t  n_prompt = t_prompt->GetEntries();

    for(ULong64_t  i=0;i<n_prompt;i++)
    {
        t_prompt->GetEntry(i);
        prompt_event_map[event_prompt]=i;
    }

    ULong64_t  n_hlt = t_hlt->GetEntries();

    for(ULong64_t  i=0;i<n_hlt;i++)
    {
        t_hlt->GetEntry(i);

        if(prompt_event_map.count(event_hlt)==0) continue;

        t_prompt->GetEntry(prompt_event_map[event_hlt]);

        std::vector<int> hlt_hash(eta_hlt->size());
        std::vector<int> prompt_hash(eta_prompt->size());

        for(size_t j=0;j<eta_hlt->size();j++)
            hlt_hash[j]=get_hash((*eta_hlt)[j],(*phi_hlt)[j]);

        for(size_t j=0;j<eta_prompt->size();j++)
            prompt_hash[j]=get_hash((*eta_prompt)[j],(*phi_prompt)[j]);

        std::unordered_map<int,std::vector<int>> prompt_map;

        for(size_t j=0;j<prompt_hash.size();j++)
            prompt_map[prompt_hash[j]].push_back(j);

        std::set<int> used_prompt;

        for(size_t h=0;h<hlt_hash.size();h++)
        {
            int hash = hlt_hash[h];

            int eta_bin = hash/PHI_BINS;
            int phi_bin = hash%PHI_BINS;

            double best_dr = DR_THRESHOLD;
            int best_idx=-1;

            for(int d_eta=-3;d_eta<=3;d_eta++)
            {
                int eta_n=eta_bin+d_eta;
                if(eta_n<0||eta_n>=ETA_BINS) continue;

                for(int d_phi=-3;d_phi<=3;d_phi++)
                {
                    int phi_n=(phi_bin+d_phi+PHI_BINS)%PHI_BINS;

                    int neighbor=eta_n*PHI_BINS+phi_n;

                    if(!prompt_map.count(neighbor)) continue;

                    for(int p:prompt_map[neighbor])
                    {
                        if(used_prompt.count(p)) continue;

                        double dr=delta_r(
                          (*eta_hlt)[h],(*phi_hlt)[h],
                          (*eta_prompt)[p],(*phi_prompt)[p]
                        );

                        if(dr<best_dr)
                        {
                            best_dr=dr;
                            best_idx=p;
                        }
                    }
                }
            }

            if(best_idx!=-1)
            {
                used_prompt.insert(best_idx);

                h_pt_diff->Fill((*pt_prompt)[best_idx] - (*pt_hlt)[h]);
                h_eta_diff->Fill((*eta_prompt)[best_idx] - (*eta_hlt)[h]);
                h_phi_diff->Fill((*phi_prompt)[best_idx] - (*phi_hlt)[h]);
                h_dr->Fill(best_dr);
            }
        }
    }

    TFile fout("matching_plots.root","RECREATE");

    h_pt_diff->Write();
    h_eta_diff->Write();
    h_phi_diff->Write();
    h_dr->Write();

    fout.Close();

    std::cout<<"Plots saved to matching_plots.root"<<std::endl;
}
