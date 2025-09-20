#include "src/app/zelda3/dungeon/object_parser.h"
#include <iostream>

int main() {
    // Test that all structs can be instantiated
    yaze::zelda3::ObjectRoutineInfo routine_info;
    yaze::zelda3::ObjectSubtypeInfo subtype_info;
    yaze::zelda3::ObjectSizeInfo size_info;
    
    std::cout << "ObjectRoutineInfo created successfully" << std::endl;
    std::cout << "ObjectSubtypeInfo created successfully" << std::endl;
    std::cout << "ObjectSizeInfo created successfully" << std::endl;
    
    // Test basic functionality
    routine_info.routine_ptr = 0x12345;
    subtype_info.subtype = 1;
    size_info.width_tiles = 4;
    
    std::cout << "All structs can be modified successfully" << std::endl;
    
    return 0;
}