if(WIN32)
	add_library(libredxx libredxx_windows.c)
	target_link_libraries(libredxx PRIVATE setupapi)
elseif(APPLE)
	add_library(libredxx libredxx_darwin.c)
	find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
	find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
	target_link_libraries(libredxx PUBLIC ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
else()
	add_library(libredxx libredxx_linux.c)
endif()

# warnings
if(MSVC)
	target_compile_options(libredxx PRIVATE /W4 $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:/WX>)
else()
	target_compile_options(libredxx PRIVATE -Wall -Wextra $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:-Werror>)
endif()

target_include_directories(libredxx PUBLIC ${PROJECT_SOURCE_DIR})

add_library(libredxx::libredxx ALIAS libredxx)
