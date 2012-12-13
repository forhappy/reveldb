# - Try to find the Leveldb config processing library
# Once done this will define
#
# LEVELDB_FOUND - System has Leveldb
# LEVELDB_INCLUDE_DIR - the Leveldb include directory
# LEVELDB_LIBRARIES 0 The libraries needed to use Leveldb

FIND_PATH(LEVELDB_INCLUDE_DIR NAMES c.h)
FIND_LIBRARY(LEVELDB_LIBRARY NAMES leveldb)

MARK_AS_ADVANCED(
    LEVELDB_INCLUDE_DIR
    LEVELDB_LIBRARY
    )
