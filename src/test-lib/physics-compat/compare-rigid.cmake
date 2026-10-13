# /*-----------------------------------------------------------------------------------------------------------------------|
# | MIT License (MIT)                                                                                                      |
# | Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
# |                                                                                                                        |
# | Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
# | documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
# | the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
# | to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
# |                                                                                                                        |
# | The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
# | the Software.                                                                                                          |
# |                                                                                                                        |
# | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
# | WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
# | COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
# | OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
# |                                                                                                                        |
# |-----------------------------------------------------------------------------------------------------------------------*/

execute_process(COMMAND "${RIGID}" --rigid-trace RESULT_VARIABLE rigid_status OUTPUT_VARIABLE rigid_trace TIMEOUT 20)
execute_process(COMMAND "${CANDIDATE}" --rigid-trace RESULT_VARIABLE candidate_status OUTPUT_VARIABLE candidate_trace TIMEOUT 20)
if(NOT rigid_status EQUAL 0 OR NOT candidate_status EQUAL 0)
    message(FATAL_ERROR "A rigid trajectory test did not complete successfully")
endif()
if(NOT rigid_trace STREQUAL candidate_trace)
    message(FATAL_ERROR "Particle OFF/ON trajectories differ: ${rigid_trace} / ${candidate_trace}")
endif()
message(STATUS "Rigid trajectories match: ${rigid_trace}")
