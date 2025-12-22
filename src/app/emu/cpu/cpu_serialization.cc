
void Cpu::SaveState(std::ostream& stream) {
  // Registers
  stream.write(reinterpret_cast<const char*>(&A), sizeof(A));
  stream.write(reinterpret_cast<const char*>(&X), sizeof(X));
  stream.write(reinterpret_cast<const char*>(&Y), sizeof(Y));
  stream.write(reinterpret_cast<const char*>(&D), sizeof(D));
  stream.write(reinterpret_cast<const char*>(&DB), sizeof(DB));
  stream.write(reinterpret_cast<const char*>(&PB), sizeof(PB));
  stream.write(reinterpret_cast<const char*>(&PC), sizeof(PC));
  stream.write(reinterpret_cast<const char*>(&status), sizeof(status));

  // Flags and State
  stream.write(reinterpret_cast<const char*>(&E), sizeof(E));
  stream.write(reinterpret_cast<const char*>(&waiting_), sizeof(waiting_));
  stream.write(reinterpret_cast<const char*>(&stopped_), sizeof(stopped_));
  stream.write(reinterpret_cast<const char*>(&irq_wanted_), sizeof(irq_wanted_));
  stream.write(reinterpret_cast<const char*>(&nmi_wanted_), sizeof(nmi_wanted_));
  stream.write(reinterpret_cast<const char*>(&reset_wanted_), sizeof(reset_wanted_));
  stream.write(reinterpret_cast<const char*>(&int_wanted_), sizeof(int_wanted_));
  stream.write(reinterpret_cast<const char*>(&int_delay_), sizeof(int_delay_));

  // Breakpoints
  uint32_t bp_count = static_cast<uint32_t>(breakpoints_.size());
  stream.write(reinterpret_cast<const char*>(&bp_count), sizeof(bp_count));
  if (bp_count > 0) {
    stream.write(reinterpret_cast<const char*>(breakpoints_.data()), bp_count * sizeof(uint32_t));
  }
}

void Cpu::LoadState(std::istream& stream) {
  // Registers
  stream.read(reinterpret_cast<char*>(&A), sizeof(A));
  stream.read(reinterpret_cast<char*>(&X), sizeof(X));
  stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
  stream.read(reinterpret_cast<char*>(&D), sizeof(D));
  stream.read(reinterpret_cast<char*>(&DB), sizeof(DB));
  stream.read(reinterpret_cast<char*>(&PB), sizeof(PB));
  stream.read(reinterpret_cast<char*>(&PC), sizeof(PC));
  stream.read(reinterpret_cast<char*>(&status), sizeof(status));

  // Flags and State
  stream.read(reinterpret_cast<char*>(&E), sizeof(E));
  stream.read(reinterpret_cast<char*>(&waiting_), sizeof(waiting_));
  stream.read(reinterpret_cast<char*>(&stopped_), sizeof(stopped_));
  stream.read(reinterpret_cast<char*>(&irq_wanted_), sizeof(irq_wanted_));
  stream.read(reinterpret_cast<char*>(&nmi_wanted_), sizeof(nmi_wanted_));
  stream.read(reinterpret_cast<char*>(&reset_wanted_), sizeof(reset_wanted_));
  stream.read(reinterpret_cast<char*>(&int_wanted_), sizeof(int_wanted_));
  stream.read(reinterpret_cast<char*>(&int_delay_), sizeof(int_delay_));

  // Breakpoints
  uint32_t bp_count;
  stream.read(reinterpret_cast<char*>(&bp_count), sizeof(bp_count));
  breakpoints_.resize(bp_count);
  if (bp_count > 0) {
    stream.read(reinterpret_cast<char*>(breakpoints_.data()), bp_count * sizeof(uint32_t));
  }
}
