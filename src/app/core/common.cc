#include "common.h"

#include <cstdint>

namespace yaze {
namespace app {
namespace core {

unsigned int SnesToPc(unsigned int addr) {
  if (addr >= 0x808000) {
    addr -= 0x808000;
  }
  unsigned int temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

int AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3) {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}

//hextodec has been imported from SNESDisasm to parse hex numbers
int HexToDec(char *input, int length)
{
    int result = 0;
    int value;
    int ceiling = length - 1;
    int power16 = 16;

    int j = ceiling;

    for( ;  j >= 0; j--)
    {
        if(input[j] >= 'A' && input[j] <= 'F')
        {
            value = input[j] - 'F';
            value += 15;
        }
        else
        {
            value = input[j] - '9';
            value += 9;
        }

        if(j == ceiling)
        {
            result += value;
            continue;
        }

        result += (value * power16);
        power16 *= 16;
    }
    
    return result;
}

}  // namespace core
}  // namespace app
}  // namespace premia
