# Copy file if source exists, silently skip otherwise
# Usage: cmake -D SRC=<source> -D DST=<dest> -P copy_if_exists.cmake

if(EXISTS "${SRC}")
    configure_file("${SRC}" "${DST}" COPYONLY)
endif()
