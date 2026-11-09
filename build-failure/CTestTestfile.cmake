# CMake generated Testfile for 
# Source directory: /workspace
# Build directory: /workspace/build-failure
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(student_tests "/workspace/build-failure/student_tests")
set_tests_properties(student_tests PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT "60" _BACKTRACE_TRIPLES "/workspace/CMakeLists.txt;58;add_test;/workspace/CMakeLists.txt;0;")
