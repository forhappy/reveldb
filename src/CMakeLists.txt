AUX_SOURCE_DIRECTORY(nedmalloc NEDMALLOC)

SET(REVELDB_SRC
    evhttpx/evhttpx.c
    evhttpx/evthr/evthr.c
    evhttpx/httpparser/http-parser.c
    main.c
    reveldb.c
    cJSON.c
    config.c
    rbtree.c
    splaytree.c
    tstring.c
    )

ADD_EXECUTABLE(reveldb ${REVELDB_SRC})

# LINK_DIRECTORIES(${REVELDB_EXTERNAL_LIBS})

TARGET_LINK_LIBRARIES(reveldb ${REVELDB_EXTERNAL_LIBS} pthread m)
