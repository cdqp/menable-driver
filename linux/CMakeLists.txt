project(linux-driver)

set(KERNEL_DIR "" CACHE PATH "Path to the kernel sources to use for the build. If not set, attempt to build for the currently running kernel.")

get_filename_component(DRIVER_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if (KERNEL_DIR AND EXISTS "${KERNEL_DIR}")
	set(KERNEL_DIR_ARG CUSTOM_KERNEL_DIR="${KERNEL_DIR}")
else ()
	message(WARNING "The specified KERNEL_DIR does not exist. Try to build with the default one.")
endif ()

add_custom_command(
	OUTPUT menable.ko
	COMMAND make V=1 -C "${CMAKE_CURRENT_LIST_DIR}" ${KERNEL_DIR_ARG}
)

add_custom_target(
	linux-driver ALL
	DEPENDS menable.ko
)

install(
	FILES ${CMAKE_CURRENT_LIST_DIR}/menable.ko
	DESTINATION .
)