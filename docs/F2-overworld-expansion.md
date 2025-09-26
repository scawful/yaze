
Module09_21
Module08_02_LoadAndAdvance
Credits_LoadScene_Overworld_LoadMap

  Overworld_LoadAndBuildScreen:
  #_02ED59:

    Overworld_DrawQuadrantsAndOverlays:
    #_02EEC5: 

      Overworld_DecompressAndDrawAllQuadrants:
      #_02F54A:

        OverworldLoad_Map32HPointers/LPointers:
        #_02F94D
          OverworldMap32_Screens 
          Banks $0B and $0C


public void fill(int x, int y, byte indextoreplace)
{
    if (indextoreplace == (byte)colorIndex) { return; }
    if (sheetPtr[x, y] == indextoreplace)
    {
        sheetPtr[x, y] = (byte)colorIndex;
        fill(x - 1, y, indextoreplace);
        fill(x + 1, y, indextoreplace);
        fill(x, y - 1, indextoreplace);
        fill(x, y + 1, indextoreplace);
    }
}