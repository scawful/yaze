#!/usr/bin/env python3
"""
Simple Visual Studio project generator for YAZE
This script creates Visual Studio project files without complex CMake dependencies
"""

import os
import sys
from pathlib import Path

def generate_vcxproj():
    """Generate the YAZE.vcxproj file with all source files"""
    
    # Source file lists (from CMake files)
    app_core_src = [
        "app/core/controller.cc",
        "app/emu/emulator.cc", 
        "app/core/project.cc",
        "app/core/window.cc",
        "app/core/asar_wrapper.cc",
        "app/core/platform/font_loader.cc",
        "app/core/platform/clipboard.cc",
        "app/core/platform/file_dialog.cc"
    ]

    app_emu_src = [
        "app/emu/audio/apu.cc",
        "app/emu/audio/spc700.cc",
        "app/emu/audio/dsp.cc",
        "app/emu/audio/internal/addressing.cc",
        "app/emu/audio/internal/instructions.cc",
        "app/emu/cpu/internal/addressing.cc",
        "app/emu/cpu/internal/instructions.cc",
        "app/emu/cpu/cpu.cc",
        "app/emu/video/ppu.cc",
        "app/emu/memory/dma.cc",
        "app/emu/memory/memory.cc",
        "app/emu/snes.cc"
    ]

    app_editor_src = [
        "app/editor/editor_manager.cc",
        "app/editor/dungeon/dungeon_editor.cc",
        "app/editor/dungeon/dungeon_room_selector.cc",
        "app/editor/dungeon/dungeon_canvas_viewer.cc",
        "app/editor/dungeon/dungeon_object_selector.cc",
        "app/editor/dungeon/dungeon_toolset.cc",
        "app/editor/dungeon/dungeon_object_interaction.cc",
        "app/editor/dungeon/dungeon_renderer.cc",
        "app/editor/dungeon/dungeon_room_loader.cc",
        "app/editor/dungeon/dungeon_usage_tracker.cc",
        "app/editor/overworld/overworld_editor.cc",
        "app/editor/overworld/overworld_editor_manager.cc",
        "app/editor/sprite/sprite_editor.cc",
        "app/editor/music/music_editor.cc",
        "app/editor/message/message_editor.cc",
        "app/editor/message/message_data.cc",
        "app/editor/message/message_preview.cc",
        "app/editor/code/assembly_editor.cc",
        "app/editor/graphics/screen_editor.cc",
        "app/editor/graphics/graphics_editor.cc",
        "app/editor/graphics/palette_editor.cc",
        "app/editor/overworld/tile16_editor.cc",
        "app/editor/overworld/map_properties.cc",
        "app/editor/graphics/gfx_group_editor.cc",
        "app/editor/overworld/entity.cc",
        "app/editor/system/settings_editor.cc",
        "app/editor/system/command_manager.cc",
        "app/editor/system/extension_manager.cc",
        "app/editor/system/shortcut_manager.cc",
        "app/editor/system/popup_manager.cc",
        "app/test/test_manager.cc"
    ]

    app_gfx_src = [
        "app/gfx/arena.cc",
        "app/gfx/background_buffer.cc",
        "app/gfx/bitmap.cc",
        "app/gfx/compression.cc",
        "app/gfx/scad_format.cc",
        "app/gfx/snes_palette.cc",
        "app/gfx/snes_tile.cc",
        "app/gfx/snes_color.cc",
        "app/gfx/tilemap.cc"
    ]

    app_zelda3_src = [
        "app/zelda3/hyrule_magic.cc",
        "app/zelda3/overworld/overworld_map.cc",
        "app/zelda3/overworld/overworld.cc",
        "app/zelda3/screen/inventory.cc",
        "app/zelda3/screen/title_screen.cc",
        "app/zelda3/screen/dungeon_map.cc",
        "app/zelda3/sprite/sprite.cc",
        "app/zelda3/sprite/sprite_builder.cc",
        "app/zelda3/music/tracker.cc",
        "app/zelda3/dungeon/room.cc",
        "app/zelda3/dungeon/room_object.cc",
        "app/zelda3/dungeon/object_parser.cc",
        "app/zelda3/dungeon/object_renderer.cc",
        "app/zelda3/dungeon/room_layout.cc",
        "app/zelda3/dungeon/dungeon_editor_system.cc",
        "app/zelda3/dungeon/dungeon_object_editor.cc"
    ]

    gui_src = [
        "app/gui/modules/asset_browser.cc",
        "app/gui/modules/text_editor.cc",
        "app/gui/canvas.cc",
        "app/gui/canvas_utils.cc",
        "app/gui/enhanced_palette_editor.cc",
        "app/gui/input.cc",
        "app/gui/style.cc",
        "app/gui/color.cc",
        "app/gui/zeml.cc",
        "app/gui/theme_manager.cc",
        "app/gui/background_renderer.cc"
    ]

    util_src = [
        "util/bps.cc",
        "util/flag.cc",
        "util/hex.cc"
    ]

    # Combine all source files
    all_source_files = (
        ["yaze.cc", "app/main.cc", "app/rom.cc"] +
        app_core_src + app_emu_src + app_editor_src + 
        app_gfx_src + app_zelda3_src + gui_src + util_src
    )

    # Header files
    header_files = [
        "incl/yaze.h",
        "incl/zelda.h",
        "src/yaze_config.h.in"
    ]

    # Generate the .vcxproj file content
    vcxproj_content = '''<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="17.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>17.0</VCProjectVersion>
    <ProjectGuid>{B2C3D4E5-F6G7-8901-BCDE-F23456789012}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>YAZE</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
    <ProjectName>YAZE</ProjectName>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="Shared">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <Import Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <Import Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <LinkIncremental>true</LinkIncremental>
    <OutDir>$(SolutionDir)build\\bin\\$(Configuration)\\</OutDir>
    <IntDir>$(SolutionDir)build\\obj\\$(Configuration)\\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>$(SolutionDir)build\\bin\\$(Configuration)\\</OutDir>
    <IntDir>$(SolutionDir)build\\obj\\$(Configuration)\\</IntDir>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <AdditionalIncludeDirectories>$(ProjectDir)src;$(ProjectDir)incl;$(ProjectDir)src\\lib;$(ProjectDir)vcpkg\\installed\\x64-windows\\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <LanguageStandard>stdcpp23</LanguageStandard>
      <BigObj>true</BigObj>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalLibraryDirectories>$(ProjectDir)vcpkg\\installed\\x64-windows\\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>SDL2.lib;SDL2main.lib;absl_base.lib;absl_strings.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <AdditionalIncludeDirectories>$(ProjectDir)src;$(ProjectDir)incl;$(ProjectDir)src\\lib;$(ProjectDir)vcpkg\\installed\\x64-windows\\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <LanguageStandard>stdcpp23</LanguageStandard>
      <BigObj>true</BigObj>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <EnableCOMDATFolding>true</EnableCOMDATFolding>
      <OptimizeReferences>true</OptimizeReferences>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalLibraryDirectories>$(ProjectDir)vcpkg\\installed\\x64-windows\\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>SDL2.lib;SDL2main.lib;absl_base.lib;absl_strings.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
'''

    for header in header_files:
        vcxproj_content += f'    <ClInclude Include="{header}" />\n'

    vcxproj_content += '''  </ItemGroup>
  <ItemGroup>
'''

    for source in all_source_files:
        vcxproj_content += f'    <ClCompile Include="src\\{source}" />\n'

    vcxproj_content += '''  </ItemGroup>
  <ItemGroup>
    <None Include="CMakeLists.txt" />
    <None Include="vcpkg.json" />
    <None Include="README.md" />
    <None Include="LICENSE" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>'''

    return vcxproj_content

def generate_solution():
    """Generate the YAZE.sln file"""
    return '''Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
VisualStudioVersion = 17.0.31903.59
MinimumVisualStudioVersion = 10.0.40219.1
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "YAZE", "YAZE.vcxproj", "{B2C3D4E5-F6G7-8901-BCDE-F23456789012}"
EndProject
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|x64 = Debug|x64
		Release|x64 = Release|x64
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|x64.ActiveCfg = Debug|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|x64.Build.0 = Debug|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|x64.ActiveCfg = Release|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|x64.Build.0 = Release|x64
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
	GlobalSection(ExtensibilityGlobals) = postSolution
		SolutionGuid = {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
	EndGlobalSection
EndGlobal'''

def main():
    """Main function to generate Visual Studio project files"""
    print("Generating simple Visual Studio project files for YAZE...")
    
    # Get the project root directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    # Generate .vcxproj file
    vcxproj_content = generate_vcxproj()
    vcxproj_path = project_root / "YAZE.vcxproj"
    
    with open(vcxproj_path, 'w', encoding='utf-8') as f:
        f.write(vcxproj_content)
    
    print(f"Generated: {vcxproj_path}")
    
    # Generate .sln file
    solution_content = generate_solution()
    solution_path = project_root / "YAZE.sln"
    
    with open(solution_path, 'w', encoding='utf-8') as f:
        f.write(solution_content)
    
    print(f"Generated: {solution_path}")
    
    print("Visual Studio project files generated successfully!")
    print("")
    print("IMPORTANT: Before building in Visual Studio:")
    print("1. Make sure vcpkg is set up: .\\scripts\\setup-vcpkg-windows.ps1")
    print("2. Install dependencies: .\\vcpkg\\vcpkg.exe install --triplet x64-windows")
    print("3. Open YAZE.sln in Visual Studio 2022")
    print("4. Build the solution (Ctrl+Shift+B)")

if __name__ == "__main__":
    main()
