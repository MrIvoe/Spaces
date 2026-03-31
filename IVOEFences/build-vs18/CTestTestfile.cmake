# CMake generated Testfile for 
# Source directory: C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences
# Build directory: C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/build-vs18
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[IVOEFencesIntegrationTests]=] "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/IVOEFencesIntegrationTests.exe")
  set_tests_properties([=[IVOEFencesIntegrationTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;112;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[IVOEFencesIntegrationTests]=] "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/IVOEFencesIntegrationTests.exe")
  set_tests_properties([=[IVOEFencesIntegrationTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;112;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[IVOEFencesIntegrationTests]=] "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/IVOEFencesIntegrationTests.exe")
  set_tests_properties([=[IVOEFencesIntegrationTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;112;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[IVOEFencesIntegrationTests]=] "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/IVOEFencesIntegrationTests.exe")
  set_tests_properties([=[IVOEFencesIntegrationTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;112;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/IVOEFences/CMakeLists.txt;0;")
else()
  add_test([=[IVOEFencesIntegrationTests]=] NOT_AVAILABLE)
endif()
subdirs("_deps/nlohmann_json-build")
