cmake_minimum_required(VERSION 3.16)

project(campus_assistance_contracts LANGUAGES CXX)

add_library(campus_assistance_contracts INTERFACE)
target_include_directories(
  campus_assistance_contracts
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/include
)
target_compile_features(campus_assistance_contracts INTERFACE cxx_std_11)

add_executable(contract_compile tests/contract_compile.cpp)
target_link_libraries(contract_compile PRIVATE campus_assistance_contracts)

enable_testing()
add_test(NAME contract_compile COMMAND contract_compile)

add_executable(repository_transition_test tests/repository_transition_test.cpp)
target_link_libraries(repository_transition_test PRIVATE campus_assistance_contracts)
add_test(NAME repository_transition_test COMMAND repository_transition_test)
