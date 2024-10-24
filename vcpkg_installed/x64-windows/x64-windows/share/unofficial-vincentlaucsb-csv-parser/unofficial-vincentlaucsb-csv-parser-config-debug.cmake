#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unofficial::vincentlaucsb-csv-parser::csv" for configuration "Debug"
set_property(TARGET unofficial::vincentlaucsb-csv-parser::csv APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(unofficial::vincentlaucsb-csv-parser::csv PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/vincentlaucsb-csv-parser-csv.lib"
  )

list(APPEND _cmake_import_check_targets unofficial::vincentlaucsb-csv-parser::csv )
list(APPEND _cmake_import_check_files_for_unofficial::vincentlaucsb-csv-parser::csv "${_IMPORT_PREFIX}/debug/lib/vincentlaucsb-csv-parser-csv.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
