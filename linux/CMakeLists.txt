project(linux-driver)

set(KERNEL_DIR "" CACHE PATH "Path to the kernel sources to use for the build. If not set, attempt to build for the currently running kernel.")

get_filename_component(DRIVER_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

option(KERNEL_VERSION "The kernel version to build for (e.g., 5.19.0-40-generic). Leave empty to use current kernel.")

list(APPEND CMAKE_MODULE_PATH 
	${REPOPATH_DRIVER}/cmake/modules
)

include(DriverLibDebugOptions)

get_debugging_flags_list(DebuggingFlags)
if (DebuggingFlags)
	list(TRANSFORM DebuggingFlags PREPEND "-D")
endif()

if (KERNEL_VERSION)
	set(KERNEL_VERSION_ARG "KERNELVER=${KERNEL_VERSION}")
endif()

if (KERNEL_DIR)
	if(EXISTS "${KERNEL_DIR}")
		set(KERNEL_DIR_ARG CUSTOM_KERNEL_DIR="${KERNEL_DIR}")
	else ()
		message(FATAL_ERROR "The specified KERNEL_DIR does not exist. (${KERNEL_DIR})")
	endif()
endif ()

add_custom_command(
	OUTPUT menable.ko
	COMMAND make V=1 -C "${CMAKE_CURRENT_LIST_DIR}" ${KERNEL_DIR_ARG} ${KERNEL_VERSION_ARG} CFLAGS=\"${DebuggingFlags}\" clean all
)

add_custom_target(
	linux-driver ALL
	DEPENDS menable.ko
)

install(
	FILES ${CMAKE_CURRENT_LIST_DIR}/menable.ko
	DESTINATION .
)