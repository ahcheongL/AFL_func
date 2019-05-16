/*
   american fuzzy lop - LLVM-mode instrumentation pass
   ---------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   LLVM integration design comes from Laszlo Szekeres. C bits copied-and-pasted
   from afl-as.c are Michal's fault.

   Copyright 2015, 2016 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This library is plugged into LLVM when invoking clang through afl-clang-fast.
   It tells the compiler to add code roughly equivalent to the bits discussed
   in ../afl-as.h.

 */

#define AFL_LLVM_PASS

#include "../config.h"
#include "../debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#define NUMNODE 2

using namespace llvm;

namespace {

  class AFLCoverage : public ModulePass {

    public:

      static char ID;
      AFLCoverage() : ModulePass(ID) { }

      bool runOnModule(Module &M) override;

      // StringRef getPassName() const override {
      //  return "American Fuzzy Lop Instrumentation";
      // }

  };

}


char AFLCoverage::ID = 0;


bool AFLCoverage::runOnModule(Module &M) {

  LLVMContext &C = M.getContext();

  IntegerType *Int8Ty  = IntegerType::getInt8Ty(C);
  IntegerType *Int32Ty = IntegerType::getInt32Ty(C);

  /* Show a banner */

  char be_quiet = 0;

  if (isatty(2) && !getenv("AFL_QUIET")) {

    SAYF(cCYA "afl-llvm-pass " cBRI VERSION cRST " by <lszekeres@google.com>\n");

  } else be_quiet = 1;

  /* Decide instrumentation ratio */

  char* inst_ratio_str = getenv("AFL_INST_RATIO");
  unsigned int inst_ratio = 100;

  if (inst_ratio_str) {

    if (sscanf(inst_ratio_str, "%u", &inst_ratio) != 1 || !inst_ratio ||
        inst_ratio > 100)
      FATAL("Bad value of AFL_INST_RATIO (must be between 1 and 100)");

  }

  /* Get globals for the SHM region and the previous location. Note that
     __afl_prev_loc is thread-local. */

  GlobalVariable *AFLMapPtr =
      new GlobalVariable(M, PointerType::get(Int8Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__afl_area_ptr");

  GlobalVariable *AFLPrevLoc = new GlobalVariable(
      M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc",
      0, GlobalVariable::GeneralDynamicTLSModel, 0, false);

	char * nodebitmap = (char *) malloc (MAP_SIZE / 8);
	memset(nodebitmap, 0, MAP_SIZE / 8);

  /* Instrument all the things! */

  int inst_blocks = 0;
	unsigned int func_idx = 0;

	for (auto &F : M){
		char bb_exist = 0;
		for (auto &BB : F){
			bb_exist ++;
			(void)BB;
		}
		if (bb_exist >= NUMNODE) func_idx++;
	}
	unsigned int num_func = func_idx;
	func_idx = 0;

	//append it all in one file.
	char tmpstr[24];
	snprintf(tmpstr,24, "FInfo.txt");
	std::ofstream funcinfos;
	funcinfos.open(tmpstr, std::ofstream::out | std::ofstream::app);


	std::cout << "generated " << tmpstr << " file.\n";
	funcinfos << num_func << "\n";

  for (auto &F : M){
		std::string funcName = F.getName();
		std::vector<std::string> blacklist = { "asan.", "llvm.", "sancov.",
				"free","malloc","calloc","realloc"};
		std::vector<unsigned int> blocklist = {};
		unsigned int numBlocks = 0;

		char cont_flag = 0;
		for (std::vector<std::string>::size_type i = 0; i < blacklist.size(); i++) {
			if(!funcName.compare(0, blacklist[i].size(), blacklist[i])){
				cont_flag = 1;
				break;
			}
		}
		if(cont_flag) continue;

		for (auto &BB : F) {

      BasicBlock::iterator IP = BB.getFirstInsertionPt();
      IRBuilder<> IRB(&(*IP));

      if (AFL_R(100) >= inst_ratio) continue;

      /* Make up cur_loc */

      unsigned int cur_loc = AFL_R(MAP_SIZE);

			while (1){
				if (nodebitmap[cur_loc / 8] & (1 << (cur_loc % 8))){
					cur_loc = AFL_R(MAP_SIZE);
				} else {
					break;
				}
			}
			nodebitmap[cur_loc /8] |= 1 << (cur_loc % 8);
			blocklist.push_back(cur_loc);

      ConstantInt *CurLoc = ConstantInt::get(Int32Ty, cur_loc);
      ConstantInt *CurBLoc = ConstantInt::get(Int32Ty, cur_loc + MAP_SIZE);

      /* Load prev_loc */

      LoadInst *PrevLoc = IRB.CreateLoad(AFLPrevLoc);
      PrevLoc->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      Value *PrevLocCasted = IRB.CreateZExt(PrevLoc, IRB.getInt32Ty());

      /* Load SHM pointer */

      LoadInst * MapPtr = IRB.CreateLoad(AFLMapPtr);
      MapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      Value * MapPtrIdx =
          IRB.CreateGEP(MapPtr, IRB.CreateXor(PrevLocCasted, CurLoc));

      /* Update bitmap */

      LoadInst *Counter = IRB.CreateLoad(MapPtrIdx);
      Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
      Value *Incr = IRB.CreateAdd(Counter, ConstantInt::get(Int8Ty, 1));
      IRB.CreateStore(Incr, MapPtrIdx)
          ->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));	

			/* update bitmap for block coverage */
			Value * MapBPtrIdx = IRB.CreateGEP(MapPtr, CurBLoc);
			LoadInst * BCounter = IRB.CreateLoad(MapBPtrIdx);
			BCounter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C,None));
			Value *IncrBlock = IRB.CreateAdd(BCounter, ConstantInt::get(Int8Ty, 1));
			IRB.CreateStore(IncrBlock, MapBPtrIdx)
					->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

      /* Set prev_loc to cur_loc >> 1 */

      StoreInst *Store =
          IRB.CreateStore(ConstantInt::get(Int32Ty, cur_loc >> 1), AFLPrevLoc);
      Store->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

      inst_blocks++;
			numBlocks++;

    }
		if (numBlocks >= NUMNODE){
			funcinfos << F.getName().str() << " " << numBlocks << "\n";
			for (std::vector<unsigned int>::size_type idx = 0;
							idx < blocklist.size() ; idx++){
				funcinfos << blocklist[idx] << "\n";
			}
			func_idx++;
		}
	}

	funcinfos.close();
  /* Say something nice. */

  if (!be_quiet) {

    if (!inst_blocks) WARNF("No instrumentation targets found.");
    else OKF("Instrumented %u locations (%s mode, ratio %u%%), %u functions.",
             inst_blocks, getenv("AFL_HARDEN") ? "hardened" :
             ((getenv("AFL_USE_ASAN") || getenv("AFL_USE_MSAN")) ?
              "ASAN/MSAN" : "non-hardened"), inst_ratio, func_idx);

  }

  return true;

}


static void registerAFLPass(const PassManagerBuilder &,
                            legacy::PassManagerBase &PM) {

  PM.add(new AFLCoverage());

}


static RegisterStandardPasses RegisterAFLPass(
    PassManagerBuilder::EP_OptimizerLast, registerAFLPass);

static RegisterStandardPasses RegisterAFLPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerAFLPass);
