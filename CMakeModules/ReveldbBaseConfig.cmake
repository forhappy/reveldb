if (CMAKE_COMPILER_IS_GNUCC)

    SET(RSN_BASE_C_FLAGS      "-Wall -fno-strict-aliasing")
    SET(CMAKE_C_FLAGS         "${CMAKE_C_FLAGS} ${RSN_BASE_C_FLAGS} -DPROJECT_VERSION=\"${PROJECT_VERSION}\"")
    SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} ${RSN_BASE_C_FLAGS} -ggdb")
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${RSN_BASE_C_FLAGS}")

    if(APPLE)
        # Newer versions of OSX will spew a bunch of warnings about deprecated ssl functions,
        # this should be addressed at some point in time, but for now, just ignore them.
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_BSD_SOURCE -Wno-deprecated-declarations")
    elseif(UNIX)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112")
    endif(APPLE)

endif(CMAKE_COMPILER_IS_GNUCC)

if (EVHTTPX_DISABLE_EVTHR)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DEVHTTPX_DISABLE_EVTHR")
endif(EVHTTPX_DISABLE_EVTHR)

if (EVHTTPX_DISABLE_SSL)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DEVHTTPX_DISABLE_SSL")
endif(EVHTTPX_DISABLE_SSL)

if (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)
