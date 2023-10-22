#ifndef YAZE_APP_GFX_PALETTE_H
#define YAZE_APP_GFX_PALETTE_H

#include <SDL.h>
#include <imgui/imgui.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "absl/base/casts.h"
#include "absl/status/status.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

/**
 * @brief Struct representing an SNES color.
 */
struct snes_color {
  uint16_t red;   /**< Red component of the color. */
  uint16_t blue;  /**< Blue component of the color. */
  uint16_t green; /**< Green component of the color. */
};
using snes_color = struct snes_color;

/**
 * @brief Struct representing an SNES palette.
 */
struct snes_palette {
  uint id;            /**< ID of the palette. */
  uint size;          /**< Size of the palette. */
  snes_color* colors; /**< Pointer to the colors in the palette. */
};
using snes_palette = struct snes_palette;

/**
 * @brief Converts an RGB color to an SNES color.
 *
 * @param color The RGB color to convert.
 * @return The converted SNES color.
 */
uint16_t ConvertRGBtoSNES(const snes_color& color);

/**
 * @brief Converts an SNES color to an RGB color.
 *
 * @param snes_color The SNES color to convert.
 * @return The converted RGB color.
 */
snes_color ConvertSNEStoRGB(uint16_t snes_color);

/**
 * @brief Extracts a vector of SNES colors from a data buffer.
 *
 * @param data The data buffer to extract from.
 * @param offset The offset in the buffer to start extracting from.
 * @param palette_size The size of the palette to extract.
 * @return A vector of SNES colors extracted from the buffer.
 */
std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size);

/**
 * @brief Converts a vector of SNES colors to a vector of characters.
 *
 * @param palette The vector of SNES colors to convert.
 * @return A vector of characters representing the converted SNES colors.
 */
std::vector<char> Convert(const std::vector<snes_color>& palette);

/**
 * @brief Struct representing an SNES color with additional functionality.
 */
struct SNESColor {
  /**
   * @brief Default constructor.
   */
  SNESColor() : rgb(0.f, 0.f, 0.f, 0.f), snes(0) {}

  /**
   * @brief Constructor that takes an ImVec4 value and converts it to an SNES
   * color.
   *
   * @param val The ImVec4 value to convert.
   */
  explicit SNESColor(const ImVec4 val) : rgb(val) {
    snes_color color;
    color.red = val.x / 255;
    color.green = val.y / 255;
    color.blue = val.z / 255;
    snes = ConvertRGBtoSNES(color);
  }

  /**
   * @brief Constructor that takes an SNES color and initializes the object with
   * it.
   *
   * @param val The SNES color to initialize with.
   */
  explicit SNESColor(const snes_color val)
      : rgb(val.red, val.green, val.blue, 255.f),
        snes(ConvertRGBtoSNES(val)),
        rom_color(val) {}

  /**
   * @brief Gets the RGB value of the color.
   *
   * @return The RGB value of the color.
   */
  ImVec4 GetRGB() const { return rgb; }

  /**
   * @brief Sets the RGB value of the color.
   *
   * @param val The RGB value to set.
   */
  void SetRGB(const ImVec4 val) {
    rgb.x = val.x / 255;
    rgb.y = val.y / 255;
    rgb.z = val.z / 255;
    snes_color color;
    color.red = val.x;
    color.green = val.y;
    color.blue = val.z;
    rom_color = color;
    snes = ConvertRGBtoSNES(color);
    modified = true;
  }

  /**
   * @brief Gets the RGB value of the color as it appears in the ROM.
   *
   * @return The RGB value of the color as it appears in the ROM.
   */
  snes_color GetRomRGB() const { return rom_color; }

  /**
   * @brief Gets the SNES value of the color.
   *
   * @return The SNES value of the color.
   */
  uint16_t GetSNES() const { return snes; }

  /**
   * @brief Sets the SNES value of the color.
   *
   * @param val The SNES value to set.
   */
  void SetSNES(uint16_t val) {
    snes = val;
    snes_color col = ConvertSNEStoRGB(val);
    rgb = ImVec4(col.red, col.green, col.blue, 0.f);
    modified = true;
  }

  /**
   * @brief Checks if the color has been modified.
   *
   * @return True if the color has been modified, false otherwise.
   */
  bool isModified() const { return modified; }

  /**
   * @brief Checks if the color is transparent.
   *
   * @return True if the color is transparent, false otherwise.
   */
  bool isTransparent() const { return transparent; }

  /**
   * @brief Sets whether the color is transparent or not.
   *
   * @param t True to set the color as transparent, false otherwise.
   */
  void setTransparent(bool t) { transparent = t; }

  /**
   * @brief Sets whether the color has been modified or not.
   *
   * @param m True to set the color as modified, false otherwise.
   */
  void setModified(bool m) { modified = m; }

 private:
  ImVec4 rgb;    /**< The RGB value of the color. */
  uint16_t snes; /**< The SNES value of the color. */
  snes_color
      rom_color; /**< The RGB value of the color as it appears in the ROM. */
  bool modified = false;    /**< Whether the color has been modified or not. */
  bool transparent = false; /**< Whether the color is transparent or not. */
};

/**
 * @brief Reads an SNES color from a ROM.
 *
 * @param offset The offset in the ROM to read from.
 * @param rom The ROM to read from.
 */
gfx::SNESColor ReadColorFromROM(int offset, const uchar* rom);

/**
 * @brief Gets an SNES color from a CGX color.
 *
 * @param color The CGX color to get the SNES color from.
 * @return The SNES color.
 */
SNESColor GetCgxColor(uint16_t color);

/**
 * @brief Gets a vector of SNES colors from a data buffer.
 *
 * @param data The data buffer to extract from.
 * @return A vector of SNES colors extracted from the buffer.
 */
std::vector<SNESColor> GetColFileData(uchar* data);

/**
 * @brief Class representing an SNES palette with additional functionality.
 */
class SNESPalette {
 public:
  /**
   * @brief Constructor that takes a vector of data and initializes the palette
   * with it.
   *
   * @tparam T The type of data in the vector.
   * @param data The vector of data to initialize the palette with.
   */
  template <typename T>
  explicit SNESPalette(const std::vector<T>& data) {
    for (const auto& item : data) {
      colors.push_back(SNESColor(item));
    }
  }

  /**
   * @brief Default constructor.
   */
  SNESPalette() = default;

  /**
   * @brief Constructor that takes a size and initializes the palette with it.
   *
   * @param mSize The size to initialize the palette with.
   */
  explicit SNESPalette(uint8_t mSize);

  /**
   * @brief Constructor that takes a character array and initializes the palette
   * with it.
   *
   * @param snesPal The character array to initialize the palette with.
   */
  explicit SNESPalette(char* snesPal);

  /**
   * @brief Constructor that takes an unsigned character array and initializes
   * the palette with it.
   *
   * @param snes_pal The unsigned character array to initialize the palette
   * with.
   */
  explicit SNESPalette(const unsigned char* snes_pal);

  /**
   * @brief Constructor that takes a vector of ImVec4 values and initializes the
   * palette with it.
   *
   * @param The vector of ImVec4 values to initialize the palette with.
   */
  explicit SNESPalette(const std::vector<ImVec4>&);

  /**
   * @brief Constructor that takes a vector of SNES colors and initializes the
   * palette with it.
   *
   * @param The vector of SNES colors to initialize the palette with.
   */
  explicit SNESPalette(const std::vector<snes_color>&);

  /**
   * @brief Constructor that takes a vector of SNES colors with additional
   * functionality and initializes the palette with it.
   *
   * @param The vector of SNES colors with additional functionality to
   * initialize the palette with.
   */
  explicit SNESPalette(const std::vector<SNESColor>&);

  /**
   * @brief Gets the SDL palette associated with the SNES palette.
   *
   * @return The SDL palette.
   */
  SDL_Palette* GetSDL_Palette();

  /**
   * @brief Creates the palette with a vector of SNES colors.
   *
   * @param cols The vector of SNES colors to create the palette with.
   */
  void Create(const std::vector<SNESColor>& cols) {
    for (const auto& each : cols) {
      colors.push_back(each);
    }
    size_ = cols.size();
  }

  /**
   * @brief Adds an SNES color to the palette.
   *
   * @param color The SNES color to add.
   */
  void AddColor(SNESColor color) {
    colors.push_back(color);
    size_++;
  }

  /**
   * @brief Adds an SNES color to the palette.
   *
   * @param color The SNES color to add.
   */
  void AddColor(snes_color color) {
    colors.emplace_back(color);
    size_++;
  }

  /**
   * @brief Gets the color at the specified index.
   *
   * @param i The index of the color to get.
   * @return The color at the specified index.
   */
  auto GetColor(int i) const {
    if (i > size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    return colors[i];
  }

  /**
   * @brief Clears the palette.
   */
  void Clear() {
    colors.clear();
    size_ = 0;
  }

  /**
   * @brief Gets the size of the palette.
   *
   * @return The size of the palette.
   */
  auto size() const { return colors.size(); }

  /**
   * @brief Gets the color at the specified index.
   *
   * @param i The index of the color to get.
   * @return The color at the specified index.
   */
  SNESColor operator[](int i) {
    if (i > size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    return colors[i];
  }

  /**
   * @brief Sets the color at the specified index.
   *
   * @param i The index of the color to set.
   * @param color The color to set.
   */
  void operator()(int i, const SNESColor& color) {
    if (i >= size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    colors[i] = color;
  }

  /**
   * @brief Sets the color at the specified index.
   *
   * @param i The index of the color to set.
   * @param color The color to set.
   */
  void operator()(int i, const ImVec4& color) {
    if (i >= size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    colors[i].SetRGB(color);
    colors[i].setModified(true);
  }

 private:
  int size_ = 0;                 /**< The size of the palette. */
  std::vector<SNESColor> colors; /**< The colors in the palette. */
};

SNESPalette ReadPaletteFromROM(int offset, int num_colors, const uchar* rom);

/**
 * @brief Gets the address of a palette in a group.
 *
 * @param group_name The name of the group.
 * @param palette_index The index of the palette.
 * @param color_index The index of the color.
 * @return The address of the palette.
 */
uint32_t GetPaletteAddress(const std::string& group_name, size_t palette_index,
                           size_t color_index);

/**
 * @brief Converts an SNES color to a float array.
 *
 * @param color The SNES color to convert.
 * @return The float array representing the SNES color.
 */
std::array<float, 4> ToFloatArray(const SNESColor& color);

/**
 * @brief Struct representing a group of SNES palettes.
 */
struct PaletteGroup {
  /**
   * @brief Default constructor.
   */
  PaletteGroup() = default;

  /**
   * @brief Constructor that takes a size and initializes the group with it.
   *
   * @param mSize The size to initialize the group with.
   */
  explicit PaletteGroup(uint8_t mSize);

  /**
   * @brief Adds a palette to the group.
   *
   * @param pal The palette to add.
   * @return An absl::Status indicating whether the operation was successful or
   * not.
   */
  absl::Status AddPalette(SNESPalette pal) {
    palettes.emplace_back(pal);
    size_ = palettes.size();
    return absl::OkStatus();
  }

  /**
   * @brief Adds a color to the group.
   *
   * @param color The color to add.
   * @return An absl::Status indicating whether the operation was successful or
   * not.
   */
  absl::Status AddColor(SNESColor color) {
    if (size_ == 0) {
      palettes.emplace_back();
    }
    palettes[0].AddColor(color);
    return absl::OkStatus();
  }

  /**
   * @brief Clears the group.
   */
  void Clear() {
    palettes.clear();
    size_ = 0;
  }

  /**
   * @brief Gets the size of the group.
   *
   * @return The size of the group.
   */
  auto size() const { return palettes.size(); }

  /**
   * @brief Gets the palette at the specified index.
   *
   * @param i The index of the palette to get.
   * @return The palette at the specified index.
   */
  SNESPalette operator[](int i) {
    if (i > size_) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }

  /**
   * @brief Gets the palette at the specified index.
   *
   * @param i The index of the palette to get.
   * @return The palette at the specified index.
   */
  const SNESPalette& operator[](int i) const {
    if (i > size_) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }

  /**
   * @brief Sets the color at the specified index.
   *
   * @param i The index of the color to set.
   * @param color The color to set.
   * @return An absl::Status indicating whether the operation was successful or
   * not.
   */
  absl::Status operator()(int i, const SNESColor& color) {
    if (i >= size_) {
      return absl::InvalidArgumentError("PaletteGroup: Index out of bounds");
    }
    palettes[i](0, color);
    return absl::OkStatus();
  }

  /**
   * @brief Sets the color at the specified index.
   *
   * @param i The index of the color to set.
   * @param color The color to set.
   * @return An absl::Status indicating whether the operation was successful or
   * not.
   */
  absl::Status operator()(int i, const ImVec4& color) {
    if (i >= size_) {
      return absl::InvalidArgumentError("PaletteGroup: Index out of bounds");
    }
    palettes[i](0, color);
    return absl::OkStatus();
  }

 private:
  int size_ = 0;                     /**< The size of the group. */
  std::vector<SNESPalette> palettes; /**< The palettes in the group. */
};

/**
 * @brief Creates a palette group from a vector of SNES colors.
 *
 * @param colors The vector of SNES colors to create the group from.
 * @return The created palette group.
 */
PaletteGroup CreatePaletteGroupFromColFile(std::vector<SNESColor>& colors);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H