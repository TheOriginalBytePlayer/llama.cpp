set(TARGET llama-run)
add_executable(${TARGET} run.cpp linenoise.cpp/linenoise.cpp)

# TODO: avoid copying this code block from common/CMakeLists.txt
set(LLAMA_RUN_EXTRA_LIBS "")
if (LLAMA_CURL)
    find_package(CURL)
    if (NOT CURL_FOUND)
        message(WARNING "Could NOT find CURL. Falling back to cpp-httplib. Hint: to explicitly disable CURL, set -DLLAMA_CURL=OFF")
        set(LLAMA_CURL OFF)
    else()
        target_compile_definitions(${TARGET} PUBLIC LLAMA_USE_CURL)
        include_directories(${CURL_INCLUDE_DIRS})
        set(LLAMA_RUN_EXTRA_LIBS ${LLAMA_RUN_EXTRA_LIBS} ${CURL_LIBRARIES})
    endif()
endif ()

if(LLAMA_TOOLS_INSTALL)
    install(TARGETS ${TARGET} RUNTIME)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "AIX")
    # AIX's flock() function comes from libbsd.a
    target_link_libraries(${TARGET} PRIVATE -lbsd)
endif()

target_link_libraries(${TARGET} PRIVATE common llama ${CMAKE_THREAD_LIBS_INIT} ${LLAMA_RUN_EXTRA_LIBS})
target_compile_features(${TARGET} PRIVATE cxx_std_17)
