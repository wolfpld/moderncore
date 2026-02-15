if(NOT COVERAGE)
    return()
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Coverage requires Clang compiler")
endif()

find_program(LLVM_PROFDATA llvm-profdata)
find_program(LLVM_COV llvm-cov)

if(NOT LLVM_PROFDATA OR NOT LLVM_COV)
    message(FATAL_ERROR "llvm-profdata and llvm-cov required for coverage")
endif()

add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
add_link_options(-fprofile-instr-generate)

set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)
set(COVERAGE_PROFRAW ${COVERAGE_DIR}/merged.profdata)
set(COVERAGE_LCOV ${CMAKE_BINARY_DIR}/coverage.lcov)
set(COVERAGE_HTML ${CMAKE_BINARY_DIR}/coverage_html)

add_custom_target(coverage-report
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${COVERAGE_DIR}/%p.profraw
            ${CMAKE_CTEST_COMMAND} --output-on-failure --test-dir ${CMAKE_BINARY_DIR}
    COMMAND ${LLVM_PROFDATA} merge -sparse ${COVERAGE_DIR}/*.profraw -o ${COVERAGE_PROFRAW}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${COVERAGE_DIR}/*.profraw
    COMMAND ${LLVM_COV} report $<TARGET_FILE:mcoreutil_tests>
            -instr-profile=${COVERAGE_PROFRAW}
            -ignore-filename-regex="tests/.*|contrib/.*|build.*/.*"
    COMMAND ${LLVM_COV} export -format=lcov $<TARGET_FILE:mcoreutil_tests>
            -instr-profile=${COVERAGE_PROFRAW}
            -ignore-filename-regex="tests/.*|contrib/.*|build.*/.*"
            > ${COVERAGE_LCOV}
    COMMAND ${LLVM_COV} show $<TARGET_FILE:mcoreutil_tests>
            -instr-profile=${COVERAGE_PROFRAW}
            -ignore-filename-regex="tests/.*|contrib/.*|build.*/.*"
            -format=html
            -output-dir=${COVERAGE_HTML}
    BYPRODUCTS ${COVERAGE_DIR} ${COVERAGE_PROFRAW} ${COVERAGE_LCOV}
    COMMENT "Running tests and generating coverage reports"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(coverage-clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${COVERAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove ${COVERAGE_LCOV}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${COVERAGE_HTML}
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/default.profraw
    COMMENT "Cleaning coverage files"
)
