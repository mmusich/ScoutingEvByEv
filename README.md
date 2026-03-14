# ScoutingEvbyEv

The goal is to be able to do event by event comparison on scouting output for the use of the NGT demonstrator. As a first step a NTuplizer for all the scouting objects needs to be created.

# Progress
- [x]  Event, Lumi and Run number info
- [x]  PF Candidates 

# How to set it all up


```
scram project -n scoutingEvByEv CMSSW_15_0_14
cd scoutingEvByEv/src
cmsenv
git cms-init
git clone git@github.com:jprendi/ScoutingEvByEv.git
cp ScoutingEvByEv/printTree* ScoutingEvByEv/getScoutingFile.sh .
chmod +x getScoutingFile.sh
./getScoutingFile.sh # this gives as an output the scouting-root-file needed!
mkdir -p Scouting/Ntuplizer
cp -r ScoutingEvByEv/plugins ScoutingEvByEv/python Scouting/Ntuplizer
scram b -j
cmsRun Scouting/Ntuplizer/python/scoutingcollectionntulizer_cfg.py inputFiles=file:outputLocalTestDataScouting.root
```
for which we can use `inputFiles`, `outputFile` and `numEvents` are the input parameters one can give it. Alternatively one can edit this in the python config file directly!
Once this has run, we can check if it's non-empty by running:
```
chmod +x printTree.sh
./printTree.sh
```

where it could look something like this:
```
******************************************************************************
*Tree    :tree      : tree                                                   *
*Entries :       99 : Total =          176200 bytes  File  Size =      99339 *
*        :          : Tree compression factor =   1.74                       *
******************************************************************************
*Br    0 :PF_pT_211 : vector<float>                                          *
*Entries :       99 : Total  Size=      37087 bytes  File Size  =      20788 *
*Baskets :        2 : Basket Size=      32000 bytes  Compression=   1.76     *
*............................................................................*
*Br    1 :PF_pT_n211 : vector<float>                                         *
*Entries :       99 : Total  Size=      36721 bytes  File Size  =      20730 *
*Baskets :        2 : Basket Size=      32000 bytes  Compression=   1.75     *
*............................................................................*
*Br    2 :PF_pT_130 : vector<float>                                          *
*Entries :       99 : Total  Size=      54751 bytes  File Size  =      32628 *
*Baskets :        2 : Basket Size=      32000 bytes  Compression=   1.66     *
*............................................................................*
*Br    3 :PF_pT_22  : vector<float>                                          *
*Entries :       99 : Total  Size=      33685 bytes  File Size  =      18853 *
*Baskets :        2 : Basket Size=      32000 bytes  Compression=   1.76     *
*............................................................................*
*Br    4 :PF_pT_13  : vector<float>                                          *
*Entries :       99 : Total  Size=       2022 bytes  File Size  =        418 *
*Baskets :        1 : Basket Size=      32000 bytes  Compression=   3.64     *
*............................................................................*
*Br    5 :PF_pT_n13 : vector<float>                                          *
*Entries :       99 : Total  Size=       2039 bytes  File Size  =        456 *
*Baskets :        1 : Basket Size=      32000 bytes  Compression=   3.36     *
*............................................................................*
*Br    6 :PF_pT_1   : vector<float>                                          *
*Entries :       99 : Total  Size=       4805 bytes  File Size  =       2381 *
*Baskets :        1 : Basket Size=      32000 bytes  Compression=   1.81     *
*............................................................................*
*Br    7 :PF_pT_2   : vector<float>                                          *
*Entries :       99 : Total  Size=       4337 bytes  File Size  =       2054 *
*Baskets :        1 : Basket Size=      32000 bytes  Compression=   1.87     *
*............................................................................*
```
