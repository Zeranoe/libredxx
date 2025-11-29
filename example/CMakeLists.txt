find_package(Threads REQUIRED)

add_executable(read_thread read_thread.c)
add_executable(ft260_i2c_read ft260_i2c_read.c)

target_link_libraries(read_thread libredxx::libredxx Threads::Threads)
target_link_libraries(ft260_i2c_read libredxx::libredxx)

if(MSVC)
	target_compile_options(read_thread PRIVATE /W4 $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:/WX>)
	target_compile_options(ft260_i2c_read PRIVATE /W4 $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:/WX>)
else()
	target_compile_options(read_thread PRIVATE -Wall -Wextra $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:-Werror>)
	target_compile_options(ft260_i2c_read PRIVATE -Wall -Wextra $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:-Werror>)
endif()
