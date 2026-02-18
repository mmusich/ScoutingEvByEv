import uproot
import numpy as np
import awkward as ak
import matplotlib.pyplot as plt

def main():
    """Main function to perform event matching."""
    
    HLT_FILE = "output_HLT.root"
    PROMPT_FILE = "output_Prompt.root"
    TREE_NAME = "scoutingCollectionNtuplizer/tree" 
    ETA_BINS = 60 
    PHI_BINS = 60
    ETA_MIN, ETA_MAX = -3.0, 3.0
    PHI_MIN, PHI_MAX = -np.pi, np.pi
    DR_THRESHOLD = 0.01

    def get_hash(eta, phi):
        """Calculates a spatial hash for a given eta and phi."""
        eta = np.asarray(eta)
        phi = np.asarray(phi)
        eta_bin = np.floor((eta - ETA_MIN) / (ETA_MAX - ETA_MIN) * ETA_BINS).astype(int)
        phi_bin = np.floor((phi - PHI_MIN) / (PHI_MAX - PHI_MIN) * PHI_BINS).astype(int)
        # Handle boundary cases
        eta_bin = np.clip(eta_bin, 0, ETA_BINS - 1)
        phi_bin = np.clip(phi_bin, 0, PHI_BINS - 1)
        return eta_bin * PHI_BINS + phi_bin

    def delta_r(eta1, phi1, eta2, phi2):
        """Calculates the delta R between two objects."""
        d_eta = eta1 - eta2
        d_phi = np.mod(phi1 - phi2 + np.pi, 2 * np.pi) - np.pi
        return np.sqrt(d_eta**2 + d_phi**2)

    def create_plots(pdg_id, **kwargs):
        """Creates and saves plots for the matched pairs."""
        
        id_str = str(pdg_id)
        if pdg_id < 0:
            id_str = "n" + str(abs(pdg_id))

        for key, values in kwargs.items():
            diff = np.array(values[1]) - np.array(values[0])
            mean_diff = np.mean(diff)
            std_diff = np.std(diff)
            plot_range = (mean_diff - 3 * std_diff, mean_diff + 3 * std_diff)
            
            plt.figure()
            plt.hist(diff, bins=100)#, range=plot_range)
            plt.xlabel(f"{key} (Prompt - HLT)")
            plt.ylabel("Number of Matched Pairs")
            plt.title(f"{key} Difference for Matched Pairs (PDG ID {pdg_id})")
            plt.yscale('log')
            plt.savefig(f"{key}_difference_{id_str}.png")
            plt.close()

        print(f"\nPlots for PDG ID {pdg_id} have been saved to PNG files.")


    # --- Load Data ---
    tree_hlt = uproot.open(HLT_FILE)[TREE_NAME].arrays()
    tree_prompt = uproot.open(PROMPT_FILE)[TREE_NAME].arrays()
    
    # --- Create Event Maps ---
    event_list_hlt = list(tree_hlt['event'])
    event_list_prompt = list(tree_prompt['event'])
    event_index_map_hlt = {event_number: index for index, event_number in enumerate(event_list_hlt)}
    event_index_map_prompt = {event_number: index for index, event_number in enumerate(event_list_prompt)}
    
    common_events = set(event_list_hlt) & set(event_list_prompt)
    
    print(f"Found {len(common_events)} common events.")

    particle_ids_to_process = [211, -211, 130, 22, 13, -13, 1, 2]
    variables_to_plot = ["pT", "eta", "phi", "normchi2", "dxy", "dz", "vertex", "dzsig", "dxysig", "trk_pt", "trk_eta", "trk_phi"]


    for pdg_id in particle_ids_to_process:
        
        id_str = str(pdg_id)
        if pdg_id < 0:
            id_str = "n" + str(abs(pdg_id))

        CANDIDATE_ID = id_str
        
        branches = {}
        for var in variables_to_plot:
            branches[var] = f"PF_{var}_{CANDIDATE_ID}"

        total_matched_pairs = 0
        num_events_processed = 0
        
        matched_data = {var: ([], []) for var in variables_to_plot}

        # --- Event Loop ---
        for event in sorted(list(common_events)):
            hlt_idx = event_index_map_hlt[event]
            prompt_idx = event_index_map_prompt[event]

            # --- Get HLT & Prompt Candidates ---
            try:
                # Use np.asarray to ensure we have standard numpy arrays and avoid awkward-related issues
                hlt_cands = {var: np.asarray(tree_hlt[hlt_idx][branch]) for var, branch in branches.items()}
                prompt_cands = {var: np.asarray(tree_prompt[prompt_idx][branch]) for var, branch in branches.items()}
            except Exception:
                continue
            
            # --- Hashing and Matching ---
            hlt_hashes = get_hash(hlt_cands["eta"], hlt_cands["phi"])
            prompt_hashes = get_hash(prompt_cands["eta"], prompt_cands["phi"])

            used_prompt_indices = set()
            prompt_hash_map = {}
            for i, h in enumerate(prompt_hashes):
                if h not in prompt_hash_map:
                    prompt_hash_map[h] = []
                prompt_hash_map[h].append(i)

            matched_pairs = 0
            
            # For each HLT candidate, search for its best match in its cell and its neighbors
            for hlt_cand_idx in range(len(hlt_hashes)):
                h = hlt_hashes[hlt_cand_idx]
                eta_bin = h // PHI_BINS
                phi_bin = h % PHI_BINS
                
                best_match_dr = DR_THRESHOLD
                best_match_idx = -1
                
                # Check 3x3 neighborhood to handle candidates near cell boundaries
                for d_eta in [-1, 0, 1]:
                    neighbor_eta_bin = eta_bin + d_eta
                    if not (0 <= neighbor_eta_bin < ETA_BINS):
                        continue
                        
                    for d_phi in [-1, 0, 1]:
                        neighbor_phi_bin = (phi_bin + d_phi) % PHI_BINS # Phi is periodic
                        
                        neighbor_hash = neighbor_eta_bin * PHI_BINS + neighbor_phi_bin
                        if neighbor_hash in prompt_hash_map:
                            for prompt_cand_idx in prompt_hash_map[neighbor_hash]:
                                if prompt_cand_idx in used_prompt_indices:
                                    continue
                                    
                                dr = delta_r(hlt_cands["eta"][hlt_cand_idx], hlt_cands["phi"][hlt_cand_idx], 
                                             prompt_cands["eta"][prompt_cand_idx], prompt_cands["phi"][prompt_cand_idx])
                                if dr < best_match_dr:
                                    best_match_dr = dr
                                    best_match_idx = prompt_cand_idx
                
                if best_match_idx != -1:
                    used_prompt_indices.add(best_match_idx)
                    matched_pairs += 1
                    for var in variables_to_plot:
                        matched_data[var][0].append(hlt_cands[var][hlt_cand_idx])
                        matched_data[var][1].append(prompt_cands[var][best_match_idx])

            total_matched_pairs += matched_pairs
            num_events_processed += 1

        print(f"\n--- Analysis Summary for PDG ID {pdg_id} ---")
        print(f"Processed {num_events_processed} common events.")
        print(f"Total matched pairs: {total_matched_pairs}")
        if num_events_processed > 0:
            average_pairs = total_matched_pairs / num_events_processed
            print(f"Average matched pairs per event: {average_pairs:.2f}")

        # --- Plotting ---
        if total_matched_pairs > 0:
            create_plots(pdg_id, **matched_data)

if __name__ == "__main__":
    main()
