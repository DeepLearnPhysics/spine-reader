# Runs spine-dump on the Reader test files and checks what it prints and returns.
#   cmake -DSPINE_DUMP=<exe> -DDATA=<tests/data> -P check_spine_dump.cmake

function(run_dump output_var code_var)
  execute_process(COMMAND ${SPINE_DUMP} ${ARGN}
                  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code)
  set(${output_var} "${out}${err}" PARENT_SCOPE)
  set(${code_var} "${code}" PARENT_SCOPE)
endfunction()

function(expect_in text part)
  string(FIND "${text}" "${part}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "spine-dump output lacks: ${part}\n--- output ---\n${text}")
  endif()
endfunction()

function(expect_not_in text part)
  string(FIND "${text}" "${part}" position)
  if(NOT position EQUAL -1)
    message(FATAL_ERROR "spine-dump output has, and should not: ${part}\n--- output ---\n${text}")
  endif()
endfunction()

set(A ${DATA}/reader/reader_a_v1.h5)
set(B ${DATA}/reader/reader_b_v2.h5)
set(C ${DATA}/reader/reader_c_v1_meta.h5)

# three files, three events shown
run_dump(text code --events 3 ${A} ${B} ${C})
if(NOT code EQUAL 0)
  message(FATAL_ERROR "spine-dump failed with ${code}:\n${text}")
endif()
expect_in("${text}" "${A}\n  SPINE 1.4.0, format_version 1 (V1), read with model 1.4.0")
expect_in("${text}" "${B}\n  SPINE 1.4.0, format_version 2 (V2Products), read with model 1.4.0")
expect_in("${text}" "meta [Meta]: all model fields read")
expect_in("${text}" "index: array product")
string(CONCAT first_event
  "6 events\n"
  "event 0 (run 7, subrun 2, event 101): 3 reco interactions, 5 reco particles, "
  "2 truth interactions, 3 truth particles\n"
  "  first reco particle: id 11, pdg 0, ke nan MeV, start (nan, nan, nan) cm, "
  "2 particles in its interaction\n")
expect_in("${text}" "${first_event}")
string(CONCAT second_event
  "event 1 (run 7, subrun 2, event 102): 1 reco interactions, 2 reco particles, "
  "0 truth interactions, 0 truth particles\n")
expect_in("${text}" "${second_event}")
expect_in("${text}" "event 2 (run 8, subrun 1, event 5): 0 reco interactions, 0 reco particles")
expect_not_in("${text}" "event 3")

# --events 1 shows one, and a count past the end shows them all
run_dump(text code --events 1 ${B})
expect_in("${text}" "2 events\nevent 0 (run 8, subrun 1, event 5): 1 reco interactions")
expect_not_in("${text}" "event 1 (")
run_dump(text code --events 99 ${B})
string(CONCAT b_second_event
  "event 1 (run 9, subrun 0, event 1): 2 reco interactions, 2 reco particles, "
  "0 truth interactions, 2 truth particles")
expect_in("${text}" "${b_second_event}")

# usage: no files is exit code 2
run_dump(text code)
if(NOT code EQUAL 2)
  message(FATAL_ERROR "spine-dump without files returned ${code}, expected 2")
endif()
expect_in("${text}" "usage: spine-dump")

# an error is exit code 1 and names the file
run_dump(text code ${DATA}/reader/no_such_file.h5)
if(NOT code EQUAL 1)
  message(FATAL_ERROR "spine-dump on a missing file returned ${code}, expected 1")
endif()
expect_in("${text}" "no_such_file.h5")
