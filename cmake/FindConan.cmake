find_program(Conan_EXECUTABLE conan HINTS ${Conan_DIR})

if (Conan_EXECUTABLE)
    set(CONAN_CMD ${Conan_EXECUTABLE})
    function(download_conan_cmake out)
        set(options ";")
        set(single_values DIR)
        set(multi_values ";")
        cmake_parse_arguments(arg "${options}" "${single_values}" "${multi_values}" ${ARGN})
        if (NOT DEFINED arg_DIR)
            set(arg_DIR "${CMAKE_CURRENT_BINARY_DIR}")
            message(NOTICE "DIR is not set, defaulting to ${arg_DIR}")
        endif ()

        if (NOT EXISTS ${arg_DIR}/conan.cmake)
            file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake"
                    "${arg_DIR}/conan.cmake"
                    STATUS download_status)
            list(POP_BACK download_status download_error)
            if (NOT download_error EQUAL 0)
                message(FATAL_ERROR "Failed to download conan.cmake")
            endif ()
        endif ()
        set(${out} ${arg_DIR}/conan.cmake PARENT_SCOPE)
    endfunction()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Conan DEFAULT_MSG Conan_EXECUTABLE)
