
add_subdirectory(common)

add_subdirectory(test)
add_subdirectory(libgo)
add_subdirectory(tools/all_report)
add_subdirectory(tools/excel_to_mysql)
include_directories(common)
include_directories(common/server)
add_subdirectory(fastcgi)
add_subdirectory(tools/work_attendance)
add_executable(asio_coro_frame 
	common/config.cpp 
	common/log.cpp 
    main.cpp)
set(MAIN_LIB_LIST boost_program_options boost_filesystem boost_coroutine boost_system boost_thread pthread boost_context boost_date_time boost_log_setup boost_log redispp hiredis ssl crypto boost_regex boost_timer boost_signals boost_locale)

 

set_target_properties(asio_coro_frame PROPERTIES COMPILE_FLAGS "-std=c++14")

target_link_libraries(asio_coro_frame ${MAIN_LIB_LIST})
