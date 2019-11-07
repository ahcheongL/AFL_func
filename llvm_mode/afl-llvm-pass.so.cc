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

using namespace llvm;

namespace {

  class AFLCoverage : public ModulePass {

    public:

      static char ID;
      u32 CidCounter;
      u32 getInstructionId(Instruction * Inst);
      AFLCoverage() : ModulePass(ID) { }
      void visitCallInst(Module &M, Instruction *Inst, std::vector<u32> &cmp_list);
      void visitInvokeInst(Module &M, Instruction *Inst, std::vector<u32> &cmp_list);
      void visitCompareFunc(Module &M, Instruction *Inst, std::vector<u32> &cmp_list);
      void visitBranchInst(Module &M, Instruction *Inst, std::vector<u32> &cmp_list);
      void visitCmpInst(Module &M, Instruction *Inst, std::vector<u32> & cmp_list);
      void processCmp(Module &M, Instruction *Cond, Constant *Cid, Instruction *InsertPoint);
      void processBoolCmp(Module &M, Value *Cond, Constant *Cid, Instruction *InsertPoint);
      void visitSwitchInst(Module &M, Instruction *Inst, std::vector<u32> & cmp_list);
      void visitExploitation(Instruction *Inst, std::vector<u32> & cmp_list);
      void processCall(Module &M, Instruction *Inst, std::vector<u32> &cmp_list);
      GlobalVariable * AFLCondPtr;

      Constant * CmpLogger;
      Constant * SwLogger;

      IntegerType *Int8Ty ;
      IntegerType *Int32Ty; 
      IntegerType *Int64Ty;
      Type * Int64PtrTy;

      bool runOnModule(Module &M) override;

      // StringRef getPassName() const override {
      //  return "American Fuzzy Lop Instrumentation";
      // }

  };

}


char AFLCoverage::ID = 0;


bool AFLCoverage::runOnModule(Module &M) {

  LLVMContext &C = M.getContext();
  Int8Ty  = IntegerType::getInt8Ty(C);
  Int32Ty = IntegerType::getInt32Ty(C);
  Int64Ty = IntegerType::getInt64Ty(C);
  Int64PtrTy = PointerType::getUnqual(Int64Ty);
  CidCounter = 0;

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

 AFLCondPtr =
      new GlobalVariable(M, PointerType::get(Int8Ty, 0), false,
                         GlobalValue::ExternalLinkage, 0, "__afl_branch_map");

  GlobalVariable *AFLPrevLoc = new GlobalVariable(
      M, Int32Ty, false, GlobalValue::ExternalLinkage, 0, "__afl_prev_loc",
      0, GlobalVariable::GeneralDynamicTLSModel, 0, false);

  Type * VoidTy = Type::getVoidTy(C); 

  //Type *CmpLoggerArgs[2] = {Int32Ty, Int32Ty};
  //FunctionType * CmpLoggerTy = FunctionType::get(VoidTy, CmpLoggerArgs, false);
  //CmpLogger = M.getOrInsertFunction("__cmp_logger", CmpLoggerTy);

  Type * SwLoggerArgs[4] = {Int32Ty, Int64Ty, Int32Ty, Int64PtrTy};
  FunctionType * SwLoggerTy = FunctionType::get(VoidTy, SwLoggerArgs, false);
  SwLogger = M.getOrInsertFunction("__sw_logger", SwLoggerTy);
  
  if (Function *F = dyn_cast<Function>(CmpLogger)) {
    F->addAttribute(AttributeSet::FunctionIndex, Attribute::NoUnwind);
    F->addAttribute(AttributeSet::FunctionIndex, Attribute::ReadNone);
  }


  /* Instrument all the things! */

  int inst_blocks = 0;
	unsigned int func_idx = 0;

	//append it all in one file.
  std::ofstream tmp;
  std::ofstream func;
  std::ifstream tmp2;
  tmp.open("tmp", std::ofstream::out | std::ofstream::trunc);
  func.open("FInfo-cmp.txt", std::ofstream::out | std::ofstream::trunc);

  for (auto &F : M){
		std::string funcName = F.getName();
		std::cout << "implementing " << funcName << "\n";
		std::vector<std::string> blacklist = { "asan.", "llvm.", "sancov.",
				"free","malloc","calloc","realloc", "__cmp_logger", "__sw_logger"};
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
 
    std::vector<u32> cmp_list;

		for (auto &BB : F) {
      BasicBlock::iterator IP = BB.getFirstInsertionPt();
      IRBuilder<> IRB(&(*IP));

      if (AFL_R(100) >= inst_ratio) continue;

      /* Make up cur_loc */

      unsigned int cur_loc = AFL_R(MAP_SIZE);

			blocklist.push_back(cur_loc);

      ConstantInt *CurLoc = ConstantInt::get(Int32Ty, cur_loc);

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

      /* Set prev_loc to cur_loc >> 1 */

      StoreInst *Store =
          IRB.CreateStore(ConstantInt::get(Int32Ty, cur_loc >> 1), AFLPrevLoc);
      Store->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));

      inst_blocks++;
			numBlocks++;
      
      std::vector<Instruction *> inst_list;

      for (auto inst = BB.begin(); inst != BB.end(); inst++){
        Instruction * Inst = &(*inst);
        inst_list.push_back(Inst);
      }
      
      for (auto inst = inst_list.begin(); inst != inst_list.end(); inst++){
        Instruction *Inst = *inst;

        if (isa<CallInst>(Inst)) {
          visitCallInst(M, Inst, cmp_list);
        } else if (isa<InvokeInst>(Inst)) {
          visitInvokeInst(M, Inst, cmp_list);
        } else if (isa<BranchInst>(Inst)) {
          visitBranchInst(M, Inst, cmp_list);
        } else if (isa<SwitchInst>(Inst)) {
          visitSwitchInst(M, Inst, cmp_list);
        } else if (isa<CmpInst>(Inst)) {
          visitCmpInst(M, Inst, cmp_list);
        } else {
          //visitExploitation(Inst, cmp_list);
        }
      }
    }
    if (cmp_list.size() > 0) {
      tmp << cmp_list.size()  << " " << F.getName().str() <<  "\n";
      func_idx++;
    }
	}

  tmp.close();
  tmp2.open("tmp", std::ios::in);
  func << func_idx << "\n";
  char buffer[200];
  while (!tmp2.eof()){
     tmp2.getline(buffer, 200);
     func << buffer << "\n";
  }
  func.close();
  tmp2.close();
  
  

  /* Say something nice. */

  if (!be_quiet) {

    if (!inst_blocks) WARNF("No instrumentation targets found.");
    else OKF("Instrumented %u locations (%s mode, ratio %u%%), %u functions, %u Conds",
             inst_blocks, getenv("AFL_HARDEN") ? "hardened" :
             ((getenv("AFL_USE_ASAN") || getenv("AFL_USE_MSAN")) ?
              "ASAN/MSAN" : "non-hardened"), inst_ratio, func_idx,CidCounter);

  }

  return true;

}

u32 AFLCoverage::getInstructionId(Instruction * Inst) {
  return CidCounter++;
}

void AFLCoverage::visitCallInst(Module &M, Instruction * Inst, std::vector<u32> &cmp_list) {
  CallInst *Caller = dyn_cast<CallInst>(Inst);
  Function *Callee = Caller->getCalledFunction();
  
  if (!Callee || Callee->isIntrinsic() || isa<InlineAsm>(Caller->getCalledValue())) {
    return;
  }

  //processCall(Inst, cmp_list);
}

void AFLCoverage::visitInvokeInst(Module &M, Instruction *Inst, std::vector<u32> &cmp_list) {

  InvokeInst *Caller = dyn_cast<InvokeInst>(Inst);
  Function *Callee = Caller->getCalledFunction();

  if (!Callee || Callee->isIntrinsic() ||
      isa<InlineAsm>(Caller->getCalledValue())) {
    return;
  }

  //processCall(Inst, cmp_list);
}

void AFLCoverage::processCall(Module &M, Instruction *Inst, std::vector<u32> &cmp_list) {
  //visitCompareFunc(Inst, cmp_list);
  //visitExploitation(Inst, cmp_list);
}

void AFLCoverage::visitCompareFunc(Module &M, Instruction *Inst, std::vector<u32> &cmp_list) {
  //if (!isa<CallInst>(Inst) || !ExploitList.isIn(*Inst, CompareFuncCat)) {
  //  return;
  //}
  u32 iid = getInstructionId(Inst);
  cmp_list.push_back(iid);
  //ConstantInt *Cid = ConstantInt::get(Int32Ty, iid);

  CallInst *Caller = dyn_cast<CallInst>(Inst);
  Value *OpArg[2];
  OpArg[0] = Caller->getArgOperand(0);
  OpArg[1] = Caller->getArgOperand(1);

  if (!OpArg[0]->getType()->isPointerTy() ||
      !OpArg[1]->getType()->isPointerTy()) {
    return;
  }
  IRBuilder<> IRB(Inst);
  //IRB.CreateCall(TraceFnTT, {Cid});
}

void AFLCoverage::visitBranchInst(Module &M, Instruction *Inst, std::vector<u32> &cmp_list) {
  BranchInst *Br = dyn_cast<BranchInst>(Inst);
  if (Br->isConditional()) {
    Value *Cond = Br->getCondition();
    if (Cond && Cond->getType()->isIntegerTy() && !isa<ConstantInt>(Cond)) {
      if (!isa<CmpInst>(Cond)) {
        // From  and, or, call, phi ....
        u32 iid = getInstructionId(Inst);
        Constant *Cid = ConstantInt::get(Int32Ty, iid);
        cmp_list.push_back(iid);
        processBoolCmp(M, Cond, Cid, Inst);
      }
    }
  }
}

void AFLCoverage::visitSwitchInst(Module &M, Instruction *Inst, std::vector<u32> &cmp_list) {

  SwitchInst *Sw = dyn_cast<SwitchInst>(Inst);
  Value *Cond = Sw->getCondition();

  if (!(Cond && Cond->getType()->isIntegerTy() && !isa<ConstantInt>(Cond))) {
    return;
  }

  int num_bits = Cond->getType()->getScalarSizeInBits();
  int num_bytes = num_bits / 8;
  if (num_bytes == 0 || num_bits % 8 > 0)
    return;

  u32 iid = getInstructionId(Inst);
  cmp_list.push_back(iid);
  Constant *Cid = ConstantInt::get(Int32Ty, iid);
  IRBuilder<> IRB(Sw);

  SmallVector<Constant *, 16> ArgList;
  for (auto It : Sw->cases()) {
    Constant *C = It.getCaseValue();
    if (C->getType()->getScalarSizeInBits() > Int64Ty->getScalarSizeInBits())
      continue;
    ArgList.push_back(ConstantExpr::getCast(CastInst::ZExt, C, Int64Ty));
  }

  ArrayType *ArrayOfInt64Ty = ArrayType::get(Int64Ty, ArgList.size());
  GlobalVariable *ArgGV = new GlobalVariable(
      M, ArrayOfInt64Ty, false, GlobalVariable::InternalLinkage,
      ConstantArray::get(ArrayOfInt64Ty, ArgList),
      "__AFL_switch_arg_values");
  Value *SwNum = ConstantInt::get(Int32Ty, ArgList.size());
  Value *ArrPtr = IRB.CreatePointerCast(ArgGV, Int64PtrTy);
  Value *CondExt = IRB.CreateZExt(Cond, Int64Ty);

  IRB.CreateCall(
      SwLogger, {Cid, CondExt, SwNum, ArrPtr});
}


void AFLCoverage::visitCmpInst(Module &M, Instruction *Inst, std::vector<u32> &cmp_list) {
  Instruction *InsertPoint = Inst->getNextNode();
  if (!InsertPoint || isa<ConstantInt>(Inst)){
    return;
  }
  u32 iid = getInstructionId(Inst);
  cmp_list.push_back(iid);
  Constant *Cid = ConstantInt::get(Int32Ty, iid);
  processCmp(M, Inst, Cid, InsertPoint);
}

void AFLCoverage::processCmp(Module &M, Instruction *Cond, Constant *Cid,
                                Instruction *InsertPoint) {
  CmpInst *Cmp = dyn_cast<CmpInst>(Cond);
  Value *OpArg[2];
  OpArg[0] = Cmp->getOperand(0);
  OpArg[1] = Cmp->getOperand(1);
  Type *OpType = OpArg[0]->getType();
  if (!((OpType->isIntegerTy() && OpType->getIntegerBitWidth() <= 64) ||
        OpType->isFloatTy() || OpType->isDoubleTy() || OpType->isPointerTy())) {
    processBoolCmp(M, Cond, Cid, InsertPoint);
    return;
  }
  int num_bytes = OpType->getScalarSizeInBits() / 8;
  if (num_bytes == 0) {
    if (OpType->isPointerTy()) {
      num_bytes = 8;
    } else {
      return;
    }
  }

  LLVMContext &C = M.getContext();

  IRBuilder<> IRB(InsertPoint);
  //Value * CondExt = IRB.CreateZExt(Cond, Int32Ty);
  LoadInst * CondMapPtr = IRB.CreateLoad(AFLCondPtr);
  CondMapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
  Value * CondMapIdx = IRB.CreateGEP(CondMapPtr, Cid);
  LoadInst * Counter = IRB.CreateLoad(CondMapIdx);
  Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C,None));
  Value * newval = IRB.CreateOr(Counter, IRB.CreateAdd(IRB.CreateZExt(Cond, Int32Ty), ConstantInt::get(Int8Ty, 1))); 
  IRB.CreateStore(newval, CondMapIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C,None));
//  IRB.CreateCall(CmpLogger, {Cid, CondExt});
}

void AFLCoverage::processBoolCmp(Module &M, Value *Cond, Constant *Cid,
                                    Instruction *InsertPoint) {
  if (!Cond->getType()->isIntegerTy() ||
      Cond->getType()->getIntegerBitWidth() > 32)
    return;

  LLVMContext &C = M.getContext();
  IRBuilder<> IRB(InsertPoint);
  //Value * CondExt = IRB.CreateZExt(Cond, Int32Ty);
//  IRB.CreateCall(CmpLogger, {Cid, CondExt});
  LoadInst * CondMapPtr = IRB.CreateLoad(AFLCondPtr);
  CondMapPtr->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C, None));
  Value * CondMapIdx = IRB.CreateGEP(CondMapPtr, Cid);
  LoadInst * Counter = IRB.CreateLoad(CondMapIdx);
  Counter->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C,None));
  Value * newval = IRB.CreateOr(Counter, IRB.CreateAdd(IRB.CreateZExt(Cond, Int32Ty), ConstantInt::get(Int8Ty, 1))); 
  IRB.CreateStore(newval, CondMapIdx)->setMetadata(M.getMDKindID("nosanitize"), MDNode::get(C,None));

}


static void registerAFLPass(const PassManagerBuilder &,
                            legacy::PassManagerBase &PM) {

  PM.add(new AFLCoverage());

}


static RegisterStandardPasses RegisterAFLPass(
    PassManagerBuilder::EP_OptimizerLast, registerAFLPass);

static RegisterStandardPasses RegisterAFLPass0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerAFLPass);
