## include
include_directories(${HCOM4DB_SRC_PATH})

include_directories(.)

## other dependency include 
include_directories(${HCOM4DB_SECUREC_INC_PATH})
include_directories(${HCOM4DB_OPENSSL_PATH})
include_directories(${ZLIB_INC_PATH})
include_directories(${LZ4_INC_PATH})

## source
aux_source_directory(./ COMMON_SRC)
set(COMMON_ALL_SRC
        ${COMMON_SRC}
        )

add_library(hcom4db SHARED ${COMMON_ALL_SRC})
target_link_libraries(hcom4db pthread dl rt -Wl,--whole-archive ${vpp_libsecurec} -Wl,--no-whole-archive)

add_library(hcom4db_static STATIC ${COMMON_ALL_SRC})
set_property(TARGET hcom4db_static PROPERTY OUTPUT_NAME "hcom4db")
target_link_libraries(hcom4db_static pthread dl rt -Wl,--whole-archive ${vpp_libsecurec} -Wl,--no-whole-archive)

add_library(hcom4db_static_master STATIC ${COMMON_ALL_SRC})
set_property(TARGET hcom4db_static_master PROPERTY OUTPUT_NAME "hcom4db_static")
target_link_libraries(hcom4db_static_master pthread dl rt -Wl,--whole-archive ${vpp_libsecurec} -Wl,--no-whole-archive)