#include "Overworld.h"

namespace yaze {
namespace Application {
namespace Data {

Overworld::Overworld() {

  for(int i = 0; i < 0x2B ;i++)
  {
      // tileLeftEntrance[i] =  (ushort)ROM.ReadShort(Core::Constants::overworldEntranceAllowedTilesLeft + (i * 2));
      // tileRightEntrance[i] = (ushort)ROM.ReadShort(Core::Constants::overworldEntranceAllowedTilesRight + (i * 2));

      //Console.WriteLine(tileLeftEntrance[i].ToString("D4") + " , " + tileRightEntrance[i].ToString("D4"));
  }
}

void Overworld::AssembleMap32Tiles()
{
  for (int i = 0; i < 0x33F0; i += 6)
  {
      // ushort[,] b = new ushort[4, 4];
      // ushort tl, tr, bl, br;
      // for (int k = 0; k < 4; k++)
      // {
      //     tl = generate(i, k, (int)Dimension.map32TilesTL);
      //     tr = generate(i, k, (int)Dimension.map32TilesTR);
      //     bl = generate(i, k, (int)Dimension.map32TilesBL);
      //     br = generate(i, k, (int)Dimension.map32TilesBR);
      //     tiles32.Add(new Tile32(tl, tr, bl, br));
      // }
  }
}

void Overworld::AssembleMap16Tiles()
{
  int tpos = Core::Constants::map16Tiles;
  for (int i = 0; i < 4096; i += 1)//3760
  {
      // TileInfo t0 = GFX.gettilesinfo((ushort)BitConverter.ToInt16(ROM.DATA, (tpos)));
      // tpos += 2;
      // TileInfo t1 = GFX.gettilesinfo((ushort)BitConverter.ToInt16(ROM.DATA, (tpos)));
      // tpos += 2;
      // TileInfo t2 = GFX.gettilesinfo((ushort)BitConverter.ToInt16(ROM.DATA, (tpos)));
      // tpos += 2;
      // TileInfo t3 = GFX.gettilesinfo((ushort)BitConverter.ToInt16(ROM.DATA, (tpos)));
      // tpos += 2;
      // tiles16.push_back(new Tile16(t0, t1, t2, t3));
  }
}

}  // namespace Data
}  // namespace Applicationß
}  // namespace yaze