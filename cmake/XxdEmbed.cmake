# XxdEmbed.cmake
#
# Called by add_custom_command in EmbedMigrations.cmake.
# Runs xxd -i on INPUT_FILE and wraps the output in a proper C++ header
# with a null terminator appended, so the array can be used as a C string.
#
# Variables expected:
#   INPUT_FILE   - path to the .sql file
#   OUTPUT_FILE  - path to the generated .h file
#   VAR_NAME     - C variable name for the array

# Read the SQL file content
file(READ "${INPUT_FILE}" SQL_CONTENT)

# Escape for C string embedding
string(LENGTH "${SQL_CONTENT}" SQL_LEN)

# Use xxd to generate hex array
execute_process(
        COMMAND xxd -i "${INPUT_FILE}"
        OUTPUT_VARIABLE XXD_OUTPUT
        RESULT_VARIABLE XXD_RESULT
)

if(NOT XXD_RESULT EQUAL 0)
    message(FATAL_ERROR "xxd failed on ${INPUT_FILE}")
endif()

# xxd -i produces:
#   unsigned char path_to_file[] = { 0x... };
#   unsigned int path_to_file_len = N;
#
# We extract just the hex bytes between { and } and rebuild with our var name.
string(REGEX MATCH "\\{([^}]*)\\}" HEX_BLOCK "${XXD_OUTPUT}")
set(HEX_BYTES "${CMAKE_MATCH_1}")

get_filename_component(INPUT_FILENAME "${INPUT_FILE}" NAME)

# Write the header
file(WRITE "${OUTPUT_FILE}" "// Auto-generated at build time from ${INPUT_FILENAME}
// DO NOT EDIT - modify the source .sql file instead

#pragma once

namespace SMPPlusPlus::Migrations {

// Null-terminated SQL string - safe to pass directly to libpq PQexec()
inline constexpr char ${VAR_NAME}[] = {
${HEX_BYTES},
  0x00  // null terminator
};

} // namespace SMPPlusPlus::Migrations
")

message(STATUS "Embedded ${INPUT_FILENAME} -> ${OUTPUT_FILE}")
