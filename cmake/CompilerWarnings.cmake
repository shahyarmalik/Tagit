# CompilerWarnings.cmake
# ---------------------------------------------------------------------------
# Centralized compiler warning configuration used across TagIt targets.

function(enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow
            -Wconversion
            -Wno-sign-conversion
        )
    endif()
endfunction()

