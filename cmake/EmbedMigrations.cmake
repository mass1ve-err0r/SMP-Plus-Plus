# EmbedMigrations.cmake
#
# For each .sql file in db/migrations/, generates a C++ header in
# ${GENERATED_DIR}/migrations/ containing the SQL as a null-terminated
# const char array via xxd -i.
#
# Naming convention:
#   001_initial_schema.sql -> migration_001.h
#   Variable name inside:  -> migration_001_sql

function(embed_migrations TARGET MIGRATIONS_DIR GENERATED_DIR)
    file(GLOB MIGRATION_FILES "${MIGRATIONS_DIR}/*.sql")
    list(SORT MIGRATION_FILES)

    set(GENERATED_HEADERS "")

    foreach(SQL_FILE ${MIGRATION_FILES})
        get_filename_component(SQL_NAME "${SQL_FILE}" NAME_WE)
        string(REGEX MATCH "^([0-9]+)" VERSION_NUM "${SQL_NAME}")

        set(HEADER_NAME "migration_${VERSION_NUM}.h")
        set(HEADER_PATH "${GENERATED_DIR}/migrations/${HEADER_NAME}")
        set(VAR_NAME    "migration_${VERSION_NUM}_sql")

        add_custom_command(
                OUTPUT  "${HEADER_PATH}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_DIR}/migrations"
                COMMAND ${CMAKE_COMMAND}
                -DINPUT_FILE=${SQL_FILE}
                -DOUTPUT_FILE=${HEADER_PATH}
                -DVAR_NAME=${VAR_NAME}
                -P "${CMAKE_SOURCE_DIR}/cmake/XxdEmbed.cmake"
                DEPENDS "${SQL_FILE}"
                COMMENT "Embedding ${SQL_NAME}.sql -> ${HEADER_NAME}"
                VERBATIM
        )

        list(APPEND GENERATED_HEADERS "${HEADER_PATH}")
    endforeach()

    add_custom_target(embed_migrations_target
            DEPENDS ${GENERATED_HEADERS}
    )
    add_dependencies(${TARGET} embed_migrations_target)

    target_include_directories(${TARGET} PRIVATE "${GENERATED_DIR}")
endfunction()