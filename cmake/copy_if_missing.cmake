# cmake -DSRC=... -DDST=... -P copy_if_missing.cmake: copy SRC to DST unless
# DST exists (a player's settings file is theirs once it is there).
if(NOT EXISTS "${DST}")
    configure_file("${SRC}" "${DST}" COPYONLY)
endif()
