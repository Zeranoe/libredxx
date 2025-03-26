find_package(Threads REQUIRED)

add_executable(read_thread read_thread.c)

target_link_libraries(read_thread libredxx::libredxx Threads::Threads)

if(MSVC)
	target_compile_options(read_thread PRIVATE /W4 $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:/WX>)
else()
	target_compile_options(read_thread PRIVATE -Wall -Wextra $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:-Werror>)
endif()
