#include "app/emu/memory/dma.h"

#include <iostream>

namespace yaze {
namespace app {
namespace emu {

void DMA::StartDMATransfer(uint8_t channelMask) {
  for (int i = 0; i < 8; ++i) {
    if ((channelMask & (1 << i)) != 0) {
      Channel& ch = channels[i];

      // Validate channel parameters (e.g., DMAPn, BBADn, A1Tn, DASn)
      // ...

      // Determine the transfer direction based on the DMAPn register
      bool fromMemory = (ch.DMAPn & 0x80) != 0;

      // Determine the transfer size based on the DMAPn register
      bool transferTwoBytes = (ch.DMAPn & 0x40) != 0;

      // Perform the DMA transfer based on the channel parameters
      std::cout << "Starting DMA transfer for channel " << i << std::endl;

      for (uint16_t j = 0; j < ch.DASn; ++j) {
        // Read a byte or two bytes from memory based on the transfer size
        // ...

        // Write the data to the B-bus address (BBADn) if transferring from
        // memory
        // ...

        // Update the A1Tn register based on the transfer direction
        if (fromMemory) {
          ch.A1Tn += transferTwoBytes ? 2 : 1;
        } else {
          ch.A1Tn -= transferTwoBytes ? 2 : 1;
        }
      }

      // Update the channel registers after the transfer (e.g., A1Tn, DASn)
      // ...
    }
  }
  MDMAEN = channelMask;  // Set the MDMAEN register to the channel mask
}

void DMA::EnableHDMATransfers(uint8_t channelMask) {
  for (int i = 0; i < 8; ++i) {
    if ((channelMask & (1 << i)) != 0) {
      Channel& ch = channels[i];

      // Validate channel parameters (e.g., DMAPn, BBADn, A1Tn, A2An, NLTRn)
      // ...

      // Perform the HDMA setup based on the channel parameters
      std::cout << "Enabling HDMA transfer for channel " << i << std::endl;

      // Read the HDMA table from memory starting at A1Tn
      // ...

      // Update the A2An register based on the HDMA table
      // ...

      // Update the NLTRn register based on the HDMA table
      // ...
    }
  }
  HDMAEN = channelMask;  // Set the HDMAEN register to the channel mask
}

}  // namespace emu
}  // namespace app
}  // namespace yaze