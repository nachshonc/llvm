//=== HexagonSplitConst32AndConst64.cpp - split CONST32/Const64 into HI/LO ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// When the compiler is invoked with no small data, for instance, with the -G0
// command line option, then all CONST* opcodes should be broken down into
// appropriate LO and HI instructions. This splitting is done by this pass.
// The only reason this is not done in the DAG lowering itself is that there
// is no simple way of getting the register allocator to allot the same hard
// register to the result of LO and HI instructions. This pass is always
// scheduled after register allocation.
//
//===----------------------------------------------------------------------===//

#include "HexagonSubtarget.h"
#include "HexagonTargetMachine.h"
#include "HexagonTargetObjectFile.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "xfer"

namespace llvm {
  FunctionPass *createHexagonSplitConst32AndConst64();
  void initializeHexagonSplitConst32AndConst64Pass(PassRegistry&);
}

namespace {
  class HexagonSplitConst32AndConst64 : public MachineFunctionPass {
  public:
    static char ID;
    HexagonSplitConst32AndConst64() : MachineFunctionPass(ID) {
      PassRegistry &R = *PassRegistry::getPassRegistry();
      initializeHexagonSplitConst32AndConst64Pass(R);
    }
    StringRef getPassName() const override {
      return "Hexagon Split Const32s and Const64s";
    }
    bool runOnMachineFunction(MachineFunction &Fn) override;
    MachineFunctionProperties getRequiredProperties() const override {
      return MachineFunctionProperties().set(
          MachineFunctionProperties::Property::NoVRegs);
    }
  };
}

char HexagonSplitConst32AndConst64::ID = 0;

INITIALIZE_PASS(HexagonSplitConst32AndConst64, "split-const-for-sdata",
      "Hexagon Split Const32s and Const64s", false, false)

bool HexagonSplitConst32AndConst64::runOnMachineFunction(MachineFunction &Fn) {
  const HexagonTargetObjectFile &TLOF =
      *static_cast<const HexagonTargetObjectFile *>(
          Fn.getTarget().getObjFileLowering());
  if (TLOF.isSmallDataEnabled())
    return true;

  const TargetInstrInfo *TII = Fn.getSubtarget().getInstrInfo();
  const TargetRegisterInfo *TRI = Fn.getSubtarget().getRegisterInfo();

  // Loop over all of the basic blocks
  for (MachineBasicBlock &B : Fn) {
    for (auto I = B.begin(), E = B.end(); I != E; ) {
      MachineInstr &MI = *I;
      ++I;
      unsigned Opc = MI.getOpcode();

      if (Opc == Hexagon::CONST32) {
        unsigned DestReg = MI.getOperand(0).getReg();
        uint64_t ImmValue = MI.getOperand(1).getImm();
        const DebugLoc &DL = MI.getDebugLoc();
        BuildMI(B, MI, DL, TII->get(Hexagon::A2_tfrsi), DestReg)
            .addImm(ImmValue);
        B.erase(&MI);
      } else if (Opc == Hexagon::CONST64) {
        unsigned DestReg = MI.getOperand(0).getReg();
        int64_t ImmValue = MI.getOperand(1).getImm();
        const DebugLoc &DL = MI.getDebugLoc();
        unsigned DestLo = TRI->getSubReg(DestReg, Hexagon::isub_lo);
        unsigned DestHi = TRI->getSubReg(DestReg, Hexagon::isub_hi);

        int32_t LowWord = (ImmValue & 0xFFFFFFFF);
        int32_t HighWord = (ImmValue >> 32) & 0xFFFFFFFF;

        BuildMI(B, MI, DL, TII->get(Hexagon::A2_tfrsi), DestLo)
            .addImm(LowWord);
        BuildMI(B, MI, DL, TII->get(Hexagon::A2_tfrsi), DestHi)
            .addImm(HighWord);
        B.erase(&MI);
      }
    }
  }

  return true;
}


//===----------------------------------------------------------------------===//
//                         Public Constructor Functions
//===----------------------------------------------------------------------===//

FunctionPass *llvm::createHexagonSplitConst32AndConst64() {
  return new HexagonSplitConst32AndConst64();
}
