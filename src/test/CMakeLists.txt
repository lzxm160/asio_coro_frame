add_executable(test1 main.cpp)
set(MAIN_LIB_LIST boost_system  boost_program_options boost_filesystem boost_coroutine boost_thread pthread boost_context boost_date_time boost_regex boost_timer boost_signals boost_locale libgo)

set_target_properties(test1 PROPERTIES COMPILE_FLAGS "-std=c++14")

target_link_libraries(test1 ${MAIN_LIB_LIST})
include_directories(../libgo)
include_directories(../libgo/libgo)
include_directories(../libgo/libgo/linux)