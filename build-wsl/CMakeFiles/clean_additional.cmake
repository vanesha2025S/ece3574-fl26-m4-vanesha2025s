# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/ai_workspace_m4_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ai_workspace_m4_autogen.dir/ParseCache.txt"
  "CMakeFiles/aiws_app_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/aiws_app_autogen.dir/ParseCache.txt"
  "CMakeFiles/aiws_m2_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/aiws_m2_autogen.dir/ParseCache.txt"
  "CMakeFiles/aiws_send_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/aiws_send_autogen.dir/ParseCache.txt"
  "CMakeFiles/student_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/student_tests_autogen.dir/ParseCache.txt"
  "ai_workspace_m4_autogen"
  "aiws_app_autogen"
  "aiws_m2_autogen"
  "aiws_send_autogen"
  "student_tests_autogen"
  )
endif()
