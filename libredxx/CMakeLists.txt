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

set_target_properties(libredxx PROPERTIES PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/libredxx.h PREFIX "" POSITION_INDEPENDENT_CODE ON)

# warnings
if(MSVC)
	target_compile_options(libredxx PRIVATE /W4 $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:/WX>)
else()
	target_compile_options(libredxx PRIVATE -Wall -Wextra $<$<BOOL:${LIBREDXX_COMPILE_WARNING_AS_ERROR}>:-Werror>)
endif()

target_include_directories(libredxx PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}> $<INSTALL_INTERFACE:include>)

add_library(libredxx::libredxx ALIAS libredxx)

include(GNUInstallDirs)
install(TARGETS libredxx EXPORT libredxxConfig ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libredxx)
install(EXPORT libredxxConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libredxx NAMESPACE libredxx::)
