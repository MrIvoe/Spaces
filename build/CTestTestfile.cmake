# CMake generated Testfile for 
# Source directory: C:/Users/MrIvo/Github/IVOESimpleFences
# Build directory: C:/Users/MrIvo/Github/IVOESimpleFences/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(HostCoreTests "C:/Users/MrIvo/Github/IVOESimpleFences/build/Debug/HostCoreTests.exe")
  set_tests_properties(HostCoreTests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;155;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(HostCoreTests "C:/Users/MrIvo/Github/IVOESimpleFences/build/Release/HostCoreTests.exe")
  set_tests_properties(HostCoreTests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;155;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(HostCoreTests "C:/Users/MrIvo/Github/IVOESimpleFences/build/MinSizeRel/HostCoreTests.exe")
  set_tests_properties(HostCoreTests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;155;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(HostCoreTests "C:/Users/MrIvo/Github/IVOESimpleFences/build/RelWithDebInfo/HostCoreTests.exe")
  set_tests_properties(HostCoreTests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;155;add_test;C:/Users/MrIvo/Github/IVOESimpleFences/CMakeLists.txt;0;")
else()
  add_test(HostCoreTests NOT_AVAILABLE)
endif()
subdirs("_deps/nlohmann_json-build")
