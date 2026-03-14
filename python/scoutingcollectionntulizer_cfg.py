import FWCore.ParameterSet.Config as cms

from FWCore.ParameterSet.VarParsing import VarParsing
params = VarParsing('analysis')

params.setDefault('inputFiles', 'file:/eos/user/j/jprendi/scoutingEvByEv/src/outputLocalTestDataScouting.root')
params.setDefault('outputFile', 'output.root')
params.setDefault('maxEvents', -1)

params.parseArguments()

process = cms.Process('Ntuplizer')
process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 100 
process.options = cms.untracked.PSet(
    wantSummary = cms.untracked.bool(True)
)
process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(params.maxEvents))
process.source = cms.Source("PoolSource",
    fileNames = cms.untracked.vstring(params.inputFiles)
)

process.TFileService = cms.Service("TFileService",
    fileName = cms.string(params.outputFile)
)

process.scoutingCollectionNtuplizer = cms.EDAnalyzer('ScoutingCollectionNtuplizer',
    pfcands = cms.InputTag("hltScoutingPFPacker")
)

process.p = cms.Path(process.scoutingCollectionNtuplizer)

