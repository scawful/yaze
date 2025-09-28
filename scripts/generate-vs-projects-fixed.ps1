# PowerShell script to generate proper Visual Studio project files for YAZE
# This script creates a comprehensive .vcxproj file with all necessary source files

param(
    [string]$ProjectRoot = ".",
    [string]$OutputDir = "."
)

$ErrorActionPreference = "Stop"

Write-Host "Generating Visual Studio project files for YAZE..." -ForegroundColor Green

# Source file lists (from CMake files)
$AppCoreSrc = @(
    "app/core/controller.cc",
    "app/emu/emulator.cc", 
    "app/core/project.cc",
    "app/core/window.cc",
    "app/core/asar_wrapper.cc",
    "app/core/platform/font_loader.cc",
    "app/core/platform/clipboard.cc",
    "app/core/platform/file_dialog.cc"
)

$AppEmuSrc = @(
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
)

$AppEditorSrc = @(
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
)

$AppGfxSrc = @(
    "app/gfx/arena.cc",
    "app/gfx/background_buffer.cc",
    "app/gfx/bitmap.cc",
    "app/gfx/compression.cc",
    "app/gfx/scad_format.cc",
    "app/gfx/snes_palette.cc",
    "app/gfx/snes_tile.cc",
    "app/gfx/snes_color.cc",
    "app/gfx/tilemap.cc"
)

$AppZelda3Src = @(
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
)

$GuiSrc = @(
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
)

$UtilSrc = @(
    "util/bps.cc",
    "util/flag.cc",
    "util/hex.cc"
)

# Combine all source files
$AllSourceFiles = @(
    "yaze.cc",
    "app/main.cc",
    "app/rom.cc"
) + $AppCoreSrc + $AppEmuSrc + $AppEditorSrc + $AppGfxSrc + $AppZelda3Src + $GuiSrc + $UtilSrc

# Header files
$HeaderFiles = @(
    "incl/yaze.h",
    "incl/zelda.h",
    "src/yaze_config.h.in"
)

# Generate the .vcxproj file
$VcxprojContent = @"
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="17.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x86">
      <Configuration>Debug</Configuration>
      <Platform>x86</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Debug|ARM64">
      <Configuration>Debug</Configuration>
      <Platform>ARM64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x86">
      <Configuration>Release</Configuration>
      <Platform>x86</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|ARM64">
      <Configuration>Release</Configuration>
      <Platform>ARM64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="RelWithDebInfo|x64">
      <Configuration>RelWithDebInfo</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="RelWithDebInfo|x86">
      <Configuration>RelWithDebInfo</Configuration>
      <Platform>x86</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="RelWithDebInfo|ARM64">
      <Configuration>RelWithDebInfo</Configuration>
      <Platform>ARM64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="MinSizeRel|x64">
      <Configuration>MinSizeRel</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="MinSizeRel|x86">
      <Configuration>MinSizeRel</Configuration>
      <Platform>x86</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="MinSizeRel|ARM64">
      <Configuration>MinSizeRel</Configuration>
      <Platform>ARM64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>17.0</VCProjectVersion>
    <ProjectGuid>{B2C3D4E5-F6G7-8901-BCDE-F23456789012}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>YAZE</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
    <ProjectName>YAZE</ProjectName>
    <VcpkgEnabled>true</VcpkgEnabled>
    <VcpkgManifestInstall>true</VcpkgManifestInstall>
  </PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x86'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|ARM64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x86'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|ARM64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|x86'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|ARM64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|x86'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|ARM64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="Shared">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='Debug|x64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='Debug|x86'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='Debug|ARM64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='Release|x86'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='Release|ARM64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|x64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|x86'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|ARM64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|x64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|x86'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|ARM64'">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x64'">
    <LinkIncremental>true</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x86'">
    <LinkIncremental>true</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|ARM64'">
    <LinkIncremental>true</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x86'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|ARM64'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|x64'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|x86'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='RelWithDebInfo|ARM64'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|x64'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|x86'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='MinSizeRel|ARM64'">
    <LinkIncremental>false</LinkIncremental>
    <OutDir>`$(SolutionDir)build\bin\`$(Configuration)\</OutDir>
    <IntDir>`$(SolutionDir)build\obj\`$(Configuration)\</IntDir>
  </PropertyGroup>
"@

# Add compiler and linker settings for all configurations
$Configurations = @("Debug", "Release", "RelWithDebInfo", "MinSizeRel")
$Platforms = @("x64", "x86", "ARM64")

foreach ($Config in $Configurations) {
    foreach ($Platform in $Platforms) {
        $IsDebug = ($Config -eq "Debug")
        $DebugFlags = if ($IsDebug) { "_DEBUG;_CONSOLE;%(PreprocessorDefinitions)" } else { "NDEBUG;_CONSOLE;%(PreprocessorDefinitions)" }
        $LinkIncremental = if ($IsDebug) { "true" } else { "false" }
        $GenerateDebugInfo = if ($Config -eq "MinSizeRel") { "false" } else { "true" }
        
        $VcxprojContent += @"

  <ItemDefinitionGroup Condition="'`$(Configuration)|`$(Platform)'=='$Config|$Platform'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>$DebugFlags</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <AdditionalIncludeDirectories>`$(ProjectDir)src;`$(ProjectDir)incl;`$(ProjectDir)src\lib;`$(ProjectDir)src\lib\asar\src;`$(ProjectDir)src\lib\asar\src\asar;`$(ProjectDir)src\lib\asar\src\asar-dll-bindings\c;`$(ProjectDir)src\lib\imgui;`$(ProjectDir)src\lib\imgui_test_engine;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <LanguageStandard>stdcpp23</LanguageStandard>
      <BigObj>true</BigObj>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <RuntimeLibrary>MultiThreaded$($IsDebug ? "Debug" : "")DLL</RuntimeLibrary>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <GenerateDebugInformation>$GenerateDebugInfo</GenerateDebugInformation>
      <EnableCOMDATFolding>$($IsDebug ? "false" : "true")</EnableCOMDATFolding>
      <OptimizeReferences>$($IsDebug ? "false" : "true")</OptimizeReferences>
    </Link>
  </ItemDefinitionGroup>
"@
    }
}

# Add source files
$VcxprojContent += @"

  <ItemGroup>
"@

foreach ($Header in $HeaderFiles) {
    $VcxprojContent += "    <ClInclude Include=`"$Header`" />`n"
}

$VcxprojContent += @"
  </ItemGroup>
  <ItemGroup>
"@

foreach ($Source in $AllSourceFiles) {
    $VcxprojContent += "    <ClCompile Include=`"src\$Source`" />`n"
}

$VcxprojContent += @"
  </ItemGroup>
  <ItemGroup>
    <None Include="CMakeLists.txt" />
    <None Include="CMakePresets.json" />
    <None Include="vcpkg.json" />
    <None Include="README.md" />
    <None Include="LICENSE" />
  </ItemGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>
"@

# Write the .vcxproj file
$VcxprojPath = Join-Path $OutputDir "YAZE.vcxproj"
$VcxprojContent | Out-File -FilePath $VcxprojPath -Encoding UTF8

Write-Host "Generated: $VcxprojPath" -ForegroundColor Green

# Generate a simple solution file
$SolutionContent = @"
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
VisualStudioVersion = 17.0.31903.59
MinimumVisualStudioVersion = 10.0.40219.1
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "YAZE", "YAZE.vcxproj", "{B2C3D4E5-F6G7-8901-BCDE-F23456789012}"
EndProject
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|x64 = Debug|x64
		Debug|x86 = Debug|x86
		Debug|ARM64 = Debug|ARM64
		Release|x64 = Release|x64
		Release|x86 = Release|x86
		Release|ARM64 = Release|ARM64
		RelWithDebInfo|x64 = RelWithDebInfo|x64
		RelWithDebInfo|x86 = RelWithDebInfo|x86
		RelWithDebInfo|ARM64 = RelWithDebInfo|ARM64
		MinSizeRel|x64 = MinSizeRel|x64
		MinSizeRel|x86 = MinSizeRel|x86
		MinSizeRel|ARM64 = MinSizeRel|ARM64
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|x64.ActiveCfg = Debug|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|x64.Build.0 = Debug|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|x86.ActiveCfg = Debug|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|x86.Build.0 = Debug|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|ARM64.ActiveCfg = Debug|ARM64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Debug|ARM64.Build.0 = Debug|ARM64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|x64.ActiveCfg = Release|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|x64.Build.0 = Release|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|x86.ActiveCfg = Release|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|x86.Build.0 = Release|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|ARM64.ActiveCfg = Release|ARM64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.Release|ARM64.Build.0 = Release|ARM64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.RelWithDebInfo|x64.ActiveCfg = RelWithDebInfo|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.RelWithDebInfo|x64.Build.0 = RelWithDebInfo|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.RelWithDebInfo|x86.ActiveCfg = RelWithDebInfo|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.RelWithDebInfo|x86.Build.0 = RelWithDebInfo|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.RelWithDebInfo|ARM64.ActiveCfg = RelWithDebInfo|ARM64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.RelWithDebInfo|ARM64.Build.0 = RelWithDebInfo|ARM64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.MinSizeRel|x64.ActiveCfg = MinSizeRel|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.MinSizeRel|x64.Build.0 = MinSizeRel|x64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.MinSizeRel|x86.ActiveCfg = MinSizeRel|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.MinSizeRel|x86.Build.0 = MinSizeRel|x86
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.MinSizeRel|ARM64.ActiveCfg = MinSizeRel|ARM64
		{B2C3D4E5-F6G7-8901-BCDE-F23456789012}.MinSizeRel|ARM64.Build.0 = MinSizeRel|ARM64
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
	GlobalSection(ExtensibilityGlobals) = postSolution
		SolutionGuid = {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
	EndGlobalSection
EndGlobal
"@

$SolutionPath = Join-Path $OutputDir "YAZE.sln"
$SolutionContent | Out-File -FilePath $SolutionPath -Encoding UTF8

Write-Host "Generated: $SolutionPath" -ForegroundColor Green
Write-Host "Visual Studio project files generated successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "To build:" -ForegroundColor Yellow
Write-Host "1. Open YAZE.sln in Visual Studio 2022" -ForegroundColor White
Write-Host "2. Ensure vcpkg is installed and configured" -ForegroundColor White
Write-Host "3. Select your desired configuration (Debug/Release) and platform (x64/x86/ARM64)" -ForegroundColor White
Write-Host "4. Build the solution (Ctrl+Shift+B)" -ForegroundColor White
