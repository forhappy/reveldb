AUX_SOURCE_DIRECTORY(nedmalloc NEDMALLOC)

SET(REVELDB_SRC
    evhttpx/evhttpx.c
    evhttpx/evthr/evthr.c
    evhttpx/httpparser/http-parser.c
    server.c
    reveldb.c
    cJSON.c
    xconfig.c
    log.c
    rpc.c
    rbtree.c
    splaytree.c
    tstring.c
    utility.c
    )

ADD_EXECUTABLE(reveldb ${REVELDB_SRC})

# LINK_DIRECTORIES(${REVELDB_EXTERNAL_LIBS})

TARGET_LINK_LIBRARIES(reveldb ${REVELDB_EXTERNAL_LIBS} pthread m)
