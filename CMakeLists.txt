cmake_minimum_required(VERSION 3.20)
project(smp_plus_plus CXX)

# -----------------------------------------------------------------------------
# C++20 hard requirement - no fallback
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Using C++20")

# -----------------------------------------------------------------------------
# Build type default
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------------
# CMake modules
# -----------------------------------------------------------------------------
include(FetchContent)
include(cmake/EmbedMigrations.cmake)

# -----------------------------------------------------------------------------
# Compiler flags
# -----------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
            -Werror=return-type
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Release>:-march=native>
            $<$<CONFIG:Debug>:-g3>
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    add_link_options(
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

# -----------------------------------------------------------------------------
# PostgreSQL - Postgres.app
# All PostgreSQL-dependent targets (including Drogon) must use the same libpq.
# -----------------------------------------------------------------------------
set(PG_APP_ROOT "/Applications/Postgres.app/Contents/Versions/latest")

list(APPEND CMAKE_PREFIX_PATH "${PG_APP_ROOT}")

set(PostgreSQL_ROOT        "${PG_APP_ROOT}")
set(PostgreSQL_INCLUDE_DIR "${PG_APP_ROOT}/include")
set(PostgreSQL_LIBRARY     "${PG_APP_ROOT}/lib/libpq.dylib")

find_package(PostgreSQL REQUIRED)

# Strip any stale Homebrew server headers that FindPostgreSQL may append
set(PostgreSQL_INCLUDE_DIRS "${PG_APP_ROOT}/include" CACHE STRING "" FORCE)
set(PostgreSQL_INCLUDE_DIR  "${PG_APP_ROOT}/include" CACHE STRING "" FORCE)

message(STATUS "PostgreSQL include dirs: ${PostgreSQL_INCLUDE_DIRS}")
message(STATUS "PostgreSQL libraries:    ${PostgreSQL_LIBRARIES}")

# -----------------------------------------------------------------------------
# bcrypt
# -----------------------------------------------------------------------------
FetchContent_Declare(
        libbcrypt
        GIT_REPOSITORY https://github.com/trusch/libbcrypt.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(libbcrypt)

# -----------------------------------------------------------------------------
# Drogon - built from source so we control BUILD_POSTGRESQL=ON
# -----------------------------------------------------------------------------
FetchContent_Declare(
        drogon
        GIT_REPOSITORY https://github.com/drogonframework/drogon.git
        GIT_TAG        v1.9.13
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES_RECURSE TRUE
)

set(BUILD_POSTGRESQL ON  CACHE BOOL "" FORCE)
set(BUILD_MYSQL      OFF CACHE BOOL "" FORCE)
set(BUILD_SQLITE     OFF CACHE BOOL "" FORCE)
set(BUILD_REDIS      OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(BUILD_CTL        OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(drogon)

# -----------------------------------------------------------------------------
# OpenSSL
# -----------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)

# -----------------------------------------------------------------------------
# pugixml
# -----------------------------------------------------------------------------
find_package(pugixml QUIET)
if(NOT pugixml_FOUND)
    message(STATUS "pugixml not found via find_package, fetching...")
    FetchContent_Declare(
            pugixml
            GIT_REPOSITORY https://github.com/zeux/pugixml.git
            GIT_TAG        v1.14
            GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(pugixml)
endif()

# -----------------------------------------------------------------------------
# tl::expected (std::expected backport for C++20)
# -----------------------------------------------------------------------------
FetchContent_Declare(
        tl_expected
        GIT_REPOSITORY https://github.com/TartanLlama/expected.git
        GIT_TAG        v1.1.0
        GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tl_expected)

# -----------------------------------------------------------------------------
# Sources - auto-discover Drogon conventional dirs + utils + db
# -----------------------------------------------------------------------------
aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters     FILTER_SRC)
aux_source_directory(plugins     PLUGIN_SRC)
aux_source_directory(models      MODEL_SRC)
aux_source_directory(utils       UTILS_SRC)
aux_source_directory(db          DB_SRC)
aux_source_directory(xml         XML_SRC)

# -----------------------------------------------------------------------------
# Target
# -----------------------------------------------------------------------------
add_executable(${PROJECT_NAME} main.cc)

embed_migrations(${PROJECT_NAME}
        "${CMAKE_SOURCE_DIR}/db/migrations"
        "${CMAKE_BINARY_DIR}/generated"
)

drogon_create_views(${PROJECT_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/views
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        ${PostgreSQL_INCLUDE_DIRS}
        ${libbcrypt_SOURCE_DIR}/include
)

target_sources(${PROJECT_NAME} PRIVATE
        ${CTL_SRC}
        ${FILTER_SRC}
        ${PLUGIN_SRC}
        ${MODEL_SRC}
        ${UTILS_SRC}
        ${DB_SRC}
        ${XML_SRC}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        drogon
        OpenSSL::SSL
        OpenSSL::Crypto
        pugixml::pugixml
        tl::expected
        bcrypt
        ${PostgreSQL_LIBRARIES}
)

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
add_subdirectory(test)

# -----------------------------------------------------------------------------
# Cleanup target
# -----------------------------------------------------------------------------
add_custom_target(clean-all
        COMMAND ${CMAKE_COMMAND} -E echo "Removing all build directories..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_SOURCE_DIR}/cmake-build-debug"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_SOURCE_DIR}/cmake-build-release"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_SOURCE_DIR}/cmake-build-production"
        COMMAND ${CMAKE_COMMAND} -E echo "Done. Reload CMake to regenerate."
        COMMENT "Cleaning all build directories"
        VERBATIM
)
