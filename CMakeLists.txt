cmake_minimum_required(VERSION 3.10)

project(MyLangCompiler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# مسیر نصب LLVM باید روی سیستم تنظیم شده باشد (مثلاً با llvm-config یا بسته‌های llvm-dev)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# کامپوننت‌هایی که لازم داریم
set(LLVM_COMPONENTS
    core
    support
    irreader
    executionengine
    mcjit
    native
    nativecodegen
)

llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_COMPONENTS})

add_executable(compiler
    main.cpp
    Lexer.cpp
    parser.cpp
    Sema.cpp
    CodeGen.cpp
    Runtime.cpp
)


target_link_libraries(compiler PRIVATE ${LLVM_LIBS})
