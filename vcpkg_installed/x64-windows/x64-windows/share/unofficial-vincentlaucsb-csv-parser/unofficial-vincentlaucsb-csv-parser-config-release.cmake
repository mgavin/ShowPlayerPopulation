#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unofficial::vincentlaucsb-csv-parser::csv" for configuration "Release"
set_property(TARGET unofficial::vincentlaucsb-csv-parser::csv APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(unofficial::vincentlaucsb-csv-parser::csv PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/vincentlaucsb-csv-parser-csv.lib"
  )

list(APPEND _cmake_import_check_targets unofficial::vincentlaucsb-csv-parser::csv )
list(APPEND _cmake_import_check_files_for_unofficial::vincentlaucsb-csv-parser::csv "${_IMPORT_PREFIX}/lib/vincentlaucsb-csv-parser-csv.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
