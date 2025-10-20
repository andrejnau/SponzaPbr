set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)

set(appleclang_compile_options
    -Wno-missing-template-arg-list-after-template-kw
)
set(msvc_compile_options
    /MP
)
add_compile_options(
    "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang>:${appleclang_compile_options}>"
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${msvc_compile_options}>"
)

set(msvc_compile_definitions
    _UNICODE
    NOMINMAX
    UNICODE
)
add_compile_definitions(
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${msvc_compile_definitions}>"
)
