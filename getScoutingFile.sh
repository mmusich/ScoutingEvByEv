#!/bin/bash
set -euo pipefail

############################################
# Configuration
############################################

MENU="/users/musich/tests/dev/CMSSW_15_0_0/NGT_DEMONSTRATOR/TestData/online/HLT/V1"

INPUT_FILE="/store/data/Run2025G/EphemeralHLTPhysics0/RAW/v1/000/398/183/00000/002bbd0c-b9ed-4758-b7a6-e2e13149ca34.root"

HLT_GT="150X_dataRun3_HLT_v1"
PROMPT_GT="150X_dataRun3_Prompt_v1"

ERA="Run3_2025"
L1_MENU="L1Menu_Collisions2025_v1_0_0_xml"

HLT_CFG="hltData.py"
PROMPT_CFG="promptData.py"

############################################
# Utilities
############################################

rename_outputs () {
    TAG=$1
    for f in *.root; do
	 # Skip files already tagged
        if [[ "$f" == hlt_* || "$f" == prompt_* ]]; then
            continue
        fi

        if [[ -f "$f" ]]; then
            mv "$f" "${TAG}_${f}"
        fi
    done
}

run_cms () {
    CFG=$1
    LOG=$2
    TAG=$3

    echo "Running cmsRun ${CFG}"
    cmsRun "${CFG}" >& "${LOG}"

    echo "Renaming ROOT outputs with tag: ${TAG}"
    rename_outputs "${TAG}"
}

############################################
# Step 1: Build HLT configuration
############################################

echo "Generating HLT configuration"

hltGetConfiguration "${MENU}" \
    --globaltag "${HLT_GT}" \
    --data \
    --unprescale \
    --output all \
    --max-events -1 \
    --eras "${ERA}" \
    --l1-emulator uGT \
    --l1 "${L1_MENU}" \
    --input "${INPUT_FILE}" \
    > "${HLT_CFG}"

############################################
# Step 2: Run HLT
############################################

run_cms "${HLT_CFG}" hltData.log hlt

############################################
# Step 3: Build Prompt configuration
############################################

echo "Preparing prompt configuration"

cp "${HLT_CFG}" "${PROMPT_CFG}"

cat <<EOF >> "${PROMPT_CFG}"
process.GlobalTag.globaltag = "${PROMPT_GT}"
EOF

############################################
# Step 4: Run Prompt
############################################

run_cms "${PROMPT_CFG}" promptData.log prompt

echo "Workflow finished successfully"

