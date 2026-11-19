cmake_minimum_required(VERSION 3.16)

project(campus_assistance_contracts LANGUAGES CXX)

add_library(campus_assistance_contracts STATIC shared/src/config.cpp
  shared/src/types.cpp)
target_include_directories(
  campus_assistance_contracts
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/include
)
target_compile_features(campus_assistance_contracts PUBLIC cxx_std_11)

add_executable(contract_compile tests/contract_compile.cpp)
target_link_libraries(contract_compile PRIVATE campus_assistance_contracts)

add_executable(repository_transition_test tests/repository_transition_test.cpp)
target_link_libraries(repository_transition_test PRIVATE campus_assistance_contracts)

add_executable(config_validator_test tests/config_validator_test.cpp)
target_link_libraries(config_validator_test PRIVATE campus_assistance_contracts)

add_executable(types_test tests/types_test.cpp)
target_link_libraries(types_test PRIVATE campus_assistance_contracts)

enable_testing()
add_test(NAME contract_compile COMMAND contract_compile)
add_test(NAME repository_transition_test COMMAND repository_transition_test)
add_test(NAME config_validator_test COMMAND config_validator_test)
add_test(NAME types_test COMMAND types_test)
