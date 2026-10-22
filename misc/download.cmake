# Usage: cmake -DURL=<url> -DDEST=<file> -P download.cmake
# Downloads a build tool once and makes it executable.

if(EXISTS "${DEST}")
  return()
endif()

file(DOWNLOAD "${URL}" "${DEST}.part" STATUS status SHOW_PROGRESS)
list(GET status 0 code)
if(NOT code EQUAL 0)
  file(REMOVE "${DEST}.part")
  list(GET status 1 message)
  message(FATAL_ERROR "Download of ${URL} failed: ${message}")
endif()

file(RENAME "${DEST}.part" "${DEST}")
file(CHMOD "${DEST}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
