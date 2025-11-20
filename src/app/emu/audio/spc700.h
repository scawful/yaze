#ifndef YAZE_APP_EMU_SPC700_H
#define YAZE_APP_EMU_SPC700_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace yaze {
namespace emu {

/**
 * @brief AudioRam is an interface for the Audio RAM used by the SPC700.
 */
class AudioRam {
 public:
  virtual ~AudioRam() = default;
  virtual void reset() = 0;
  virtual uint8_t read(uint16_t address) const = 0;
  virtual uint8_t& mutable_read(uint16_t address) = 0;
  virtual void write(uint16_t address, uint8_t value) = 0;
  uint8_t operator[](uint16_t address) { return mutable_read(address); }
};

/**
 * @brief AudioRamImpl is an implementation of the AudioRam interface.
 */
class AudioRamImpl : public AudioRam {
  static const int ARAM_SIZE = 0x10000;
  std::vector<uint8_t> ram = std::vector<uint8_t>(ARAM_SIZE, 0);

 public:
  AudioRamImpl() = default;
  void reset() override { ram = std::vector<uint8_t>(ARAM_SIZE, 0); }

  uint8_t read(uint16_t address) const override {
    return ram[address % ARAM_SIZE];
  }

  uint8_t& mutable_read(uint16_t address) override {
    return ram.at(address % ARAM_SIZE);
  }

  void write(uint16_t address, uint8_t value) override {
    ram[address % ARAM_SIZE] = value;
  }

  // add [] operators
  uint8_t operator[](uint16_t address) const { return read(address); }
};

typedef struct ApuCallbacks {
  std::function<void(uint16_t, uint8_t)> write;
  std::function<uint8_t(uint16_t)> read;
  std::function<void(bool)> idle;
} ApuCallbacks;

/**
 * @class Spc700
 * @brief The Spc700 class represents the SPC700 processor.
 *
 * The Spc700 class provides the functionality to execute instructions, read and
 * write memory, and handle various addressing modes. It also contains registers
 * and flags specific to the SPC700.
 *
 * @note This class assumes the existence of an `AudioRam` object for memory
 * access.
 */
class Spc700 {
 private:
  ApuCallbacks callbacks_;
  std::vector<std::string> log_;

  bool stopped_;
  bool reset_wanted_;
  // single-cycle
  uint8_t opcode;
  uint32_t step = 0;
  uint32_t bstep;
  uint16_t adr;
  uint16_t adr1;
  uint8_t dat;
  uint16_t dat16;
  uint8_t param;
  int extra_cycles_ = 0;

  // Cycle tracking for accurate APU synchronization
  int last_opcode_cycles_ = 0;

  const uint8_t ipl_rom_[64]{
      0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA,
      0xF4, 0x8F, 0xBB, 0xF5, 0x78, 0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19,
      0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5, 0xCB,
      0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E,
      0xF4, 0x10, 0xEB, 0xBA, 0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4,
      0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF};

 public:
  explicit Spc700(ApuCallbacks& callbacks) : callbacks_(callbacks) {}

  // Registers
  uint8_t A = 0x00;      // 8-bit accumulator
  uint8_t X = 0x00;      // 8-bit index
  uint8_t Y = 0x00;      // 8-bit index
  uint16_t YA = 0x00;    // 16-bit pair of A (lsb) and Y (msb)
  uint16_t PC = 0xFFC0;  // program counter
  uint8_t SP = 0x00;     // stack pointer

  struct Flags {
    bool N : 1;  // Negative flag
    bool V : 1;  // Overflow flag
    bool P : 1;  // Direct page flag
    bool B : 1;  // Break flag
    bool H : 1;  // Half-carry flag
    bool I : 1;  // Interrupt enable
    bool Z : 1;  // Zero flag
    bool C : 1;  // Carry flag
  };
  Flags PSW;  // Processor status word

  uint8_t FlagsToByte(Flags flags) {
    return (flags.N << 7) | (flags.V << 6) | (flags.P << 5) | (flags.B << 4) |
           (flags.H << 3) | (flags.I << 2) | (flags.Z << 1) | (flags.C);
  }

  Flags ByteToFlags(uint8_t byte) {
    Flags flags;
    flags.N = (byte & 0x80) >> 7;
    flags.V = (byte & 0x40) >> 6;
    flags.P = (byte & 0x20) >> 5;
    flags.B = (byte & 0x10) >> 4;
    flags.H = (byte & 0x08) >> 3;
    flags.I = (byte & 0x04) >> 2;
    flags.Z = (byte & 0x02) >> 1;
    flags.C = (byte & 0x01);
    return flags;
  }

  void Reset(bool hard = false);

  void RunOpcode();

  // New atomic step function - executes one complete instruction and returns cycles consumed
  // This is the preferred method for cycle-accurate emulation
  int Step();

  // Get the number of cycles consumed by the last opcode execution
  int GetLastOpcodeCycles() const { return last_opcode_cycles_; }

  void ExecuteInstructions(uint8_t opcode);
  void LogInstruction(uint16_t initial_pc, uint8_t opcode);

  // Read a byte from the memory-mapped registers
  uint8_t read(uint16_t address) { return callbacks_.read(address); }

  uint16_t read_word(uint16_t address) {
    uint16_t adrl = address;
    uint16_t adrh = address + 1;
    uint8_t value = callbacks_.read(adrl);
    return value | (callbacks_.read(adrh) << 8);
  }

  uint8_t ReadOpcode() {
    uint8_t opcode = callbacks_.read(PC++);
    return opcode;
  }

  uint16_t ReadOpcodeWord() {
    uint8_t low = ReadOpcode();
    uint8_t high = ReadOpcode();
    return low | (high << 8);
  }

  void DoBranch(uint8_t value, bool check) {
    callbacks_.idle(false);  // Add missing base cycle for all branches
    if (check) {
      // taken branch: 2 extra cycles
      callbacks_.idle(false);
      callbacks_.idle(false);
      PC += (int8_t)value;
      extra_cycles_ = 2;
    }
  }

  // Write a byte to the memory-mapped registers
  void write(uint16_t address, uint8_t value) {
    callbacks_.write(address, value);
  }

  void push_byte(uint8_t value) {
    callbacks_.write(0x100 | SP, value);
    SP--;
  }

  void push_word(uint16_t value) {
    push_byte(value >> 8);
    push_byte(value & 0xFF);
  }

  uint8_t pull_byte() {
    SP++;
    return read(0x100 | SP);
  }

  uint16_t pull_word() {
    uint16_t value = pull_byte();
    value |= pull_byte() << 8;
    return value;
  }

  // ======================================================
  // Addressing modes

  // Immediate
  uint16_t imm();

  // Direct page
  uint8_t dp();
  uint16_t dpx();

  uint8_t get_dp_addr();

  uint8_t abs_bit(uint16_t* adr);
  uint16_t dp_dp(uint8_t* src);
  uint16_t ind();
  uint16_t ind_ind(uint8_t* srcVal);
  uint16_t dp_word(uint16_t* low);
  uint16_t ind_p();
  uint16_t abs_x();
  uint16_t abs_y();
  uint16_t idx();
  uint16_t idy();
  uint16_t dp_y();
  uint16_t dp_imm(uint8_t* srcVal);
  uint16_t abs();

  int8_t rel();

  // Direct page indexed by X
  uint8_t dp_plus_x();

  // Direct page indexed by Y
  uint8_t dp_plus_y();

  // Indexed indirect (add index before 16-bit lookup).
  uint16_t dp_plus_x_indirect();

  // Indirect indexed (add index after 16-bit lookup).
  uint16_t dp_indirect_plus_y();
  uint8_t i();

  uint8_t i_postinc();

  uint16_t addr_plus_i();

  uint16_t addr_plus_i_indexed();

  // ==========================================================================
  // Instructions

  void MOV(uint16_t adr);
  void MOV_ADDR(uint16_t address, uint8_t operand);
  void MOVY(uint16_t adr);
  void MOVX(uint16_t adr);
  void MOVS(uint16_t adr);
  void MOVSX(uint16_t adr);
  void MOVSY(uint16_t adr);

  void ADC(uint16_t adr);
  void ADCM(uint16_t& dest, uint8_t operand);

  void SBC(uint16_t adr);
  void SBCM(uint16_t& dest, uint8_t operand);

  void CMP(uint16_t adr);
  void CMPX(uint16_t adr);
  void CMPM(uint16_t dst, uint8_t value);
  void CMPY(uint16_t adr);

  void AND(uint16_t adr);
  void ANDM(uint16_t dest, uint8_t operand);

  void OR(uint16_t adr);
  void ORM(uint16_t dest, uint8_t operand);

  void EOR(uint16_t adr);
  void EORM(uint16_t dest, uint8_t operand);

  void ASL(uint16_t operand);
  void LSR(uint16_t adr);

  void ROL(uint16_t operand);
  void ROR(uint16_t adr);

  void XCN(uint8_t operand, bool isImmediate = false);
  void INC(uint16_t adr);
  void DEC(uint16_t operand);
  void MOVW(uint16_t& dest, uint16_t operand);
  void INCW(uint16_t& operand);
  void DECW(uint16_t& operand);
  void ADDW(uint16_t& dest, uint16_t operand);
  void SUBW(uint16_t& dest, uint16_t operand);
  void CMPW(uint16_t operand);
  void MUL(uint8_t operand);
  void DIV(uint8_t operand);
  void BRA(int8_t offset);
  void BEQ(int8_t offset);
  void BNE(int8_t offset);
  void BCS(int8_t offset);
  void BCC(int8_t offset);
  void BVS(int8_t offset);
  void BVC(int8_t offset);
  void BMI(int8_t offset);
  void BPL(int8_t offset);
  void BBS(uint8_t bit, uint8_t operand);
  void BBC(uint8_t bit, uint8_t operand);
  void JMP(uint16_t address);
  void CALL(uint16_t address);
  void PCALL(uint8_t offset);
  void TCALL(uint8_t offset);
  void BRK();
  void RET();
  void RETI();
  void PUSH(uint8_t operand);
  void POP(uint8_t& operand);
  void SET1(uint8_t bit, uint8_t& operand);
  void CLR1(uint8_t bit, uint8_t& operand);
  void TSET1(uint8_t bit, uint8_t& operand);
  void TCLR1(uint8_t bit, uint8_t& operand);
  void AND1(uint8_t bit, uint8_t& operand);
  void OR1(uint8_t bit, uint8_t& operand);
  void EOR1(uint8_t bit, uint8_t& operand);
  void NOT1(uint8_t bit, uint8_t& operand);
  void MOV1(uint8_t bit, uint8_t& operand);
  void CLRC();
  void SETC();
  void NOTC();
  void CLRV();
  void CLRP();
  void SETP();
  void EI();
  void DI();
  void NOP();
  void SLEEP();
  void STOP();

  // CBNE DBNZ
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_SPC700_H
