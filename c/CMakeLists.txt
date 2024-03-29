CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12 FATAL_ERROR)


#SET(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
#ADD_DEFINITIONS(-DPACKAGE_VERSION="${PACKAGE_VERSION}")
#ADD_DEFINITIONS(-DPACKAGE_STRING="${PACKAGE_STRING}")

#SET (MAIN_DIR ${CMAKE_SOURCE_DIR})
#get_filename_component(MAIN_DIR ${CMAKE_SOURCE_DIR} PATH)
#SET(TEST_DIR ${MAIN_DIR}/t)

#MESSAGE(---> ${MAIN_DIR})

#INCLUDE_DIRECTORIES(${MAIN_DIR})
#INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}) # for generated files

#Uncomment below to build / link modules statically into a single deliverabe
#set (STATIC_BUILD true)

#Uncomemnt below to build fw itself as a shared library (no main)
set (SHARED_ONLY true)

#Used to govern include file directive (local vs system)
ADD_DEFINITIONS(-DBUILDING_LIB)

#Uncomment to use pre-allocated buffers
ADD_DEFINITIONS(-DUSE_MSG_BUFFERS)

#This will count how many messages (events) each module got.
ADD_DEFINITIONS(-DMSG_ACCOUNTING)

#Count how many times each (suypporting) module main message loop is active
ADD_DEFINITIONS(-DWAKEUP_ACCOUNTING)

find_package( PkgConfig REQUIRED)
find_package( Threads REQUIRED )
       
set (HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR/../include}
    )#PARENT_SCOPE)


if (APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result -Wno-missing-braces -Wno-microsoft-anon-tag")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -g -O0 -Wno-unused-result -Wno-missing-braces -Wno-microsoft-anon-tag")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result -Wno-missing-braces -Wno-unknown-warning-option -Wno-address-of-packed-member")
    # not neede for static bin... #-Wl,--no-as-needed")
endif()

#file(GLOB FW_HEADERS ${PROJECT_SOURCE_DIR}/include/*.h)
set(FW_HEADERS ${PROJECT_SOURCE_DIR}/include/rdbfw.h 
	${PROJECT_SOURCE_DIR}/include/log.h
	${PROJECT_SOURCE_DIR}/include/ansi.h
	${PROJECT_SOURCE_DIR}/include/fwalloc.h
	${PROJECT_SOURCE_DIR}/include/messaging.h
	${PROJECT_SOURCE_DIR}/include/utils.h )

set (FW_SRC
    "messaging.c"
    "fwalloc.c"
    "rdbfw_main.c"
    "utils.c"
    )

set (LINUX_SRC 
    "timers.c"
    )
#"hw_timers.c" //Optional

set (APPLE_SRC 
    "timers.c"
    "iOS_rdbmsg.c"
    )
    
if (STATIC_BUILD)
    ADD_DEFINITIONS(-DSTATIC_BUILD)
    MESSAGE("Building a statically linked deliverable")
endif()

if (SHARED_ONLY)
    ADD_DEFINITIONS(-DSHARED_ONLY)
endif()

if (NOT APPLE)
    ADD_DEFINITIONS(-DUSE_PRCTL)
endif()

#add_subdirectory(cpp_helpers)
#message("HDR==${CMAKE_CURRENT_SOURCE_DIR}/../include")
#message("HDR@==${PROJECT_SOURCE_DIR}/include")
#message("HDR3==${FW_HEADERS}")
if (SHARED_ONLY)
    MESSAGE("Building a Shared-Only build (library)")
    if(APPLE)
        ADD_LIBRARY(rdbfwlib SHARED ${FW_SRC} ${APPLE_SRC} fwalloc.h rdbfw.h)
    else()
	    ADD_LIBRARY(rdbfwlib SHARED ${FW_SRC} ${LINUX_SRC})# ${FW_HEADERS})
    endif()
    target_link_libraries (rdbfwlib pthread dl rdb)# cpp_helpers)
    target_include_directories (rdbfwlib PUBLIC ${PROJECT_SOURCE_DIR}/include)
else()
    ADD_EXECUTABLE(rdbfw rdbfw_main.c messaging.c utils.c rdbfw.h hw_timers.c timers.c fwalloc.c skeleton.c event_driven_skeleton.c)
    target_link_libraries (rdbfw pthread dl rdb rt)
    
    ADD_EXECUTABLE(rdbfw-test rdbfw_main.c messaging.c utils.c rdbfw.h hw_timers.c timers.c fwalloc.c ../src/rDB.h)
    target_link_libraries (rdbfw-test pthread dl rdb rt)
    set_target_properties(rdbfw-test PROPERTIES COMPILE_DEFINITIONS "TESTER")
endif()

if (NOT STATIC_BUILD)
    ADD_LIBRARY(timers SHARED timers.c utils.c messaging.c)
    #target_include_directories (timers PRIVATE $<TARGET_PROPERTY:cpp_helpers,INCLUDE_DIRECTORIES>)
    target_link_libraries (timers pthread dl rdb rt)

    ADD_LIBRARY(hw_timers SHARED hw_timers.c utils.c messaging.c)
    #target_include_directories (hw_timers PRIVATE $<TARGET_PROPERTY:cpp_helpers,INCLUDE_DIRECTORIES>)
    target_link_libraries (hw_timers pthread dl rdb rt)
endif()

#set_target_properties(rdbfwlib PROPERTIES PUBLIC_HEADER "rdbfw.h;log.h;ansi.h;fwalloc.h;messaging.h;utils.h")
set_target_properties(rdbfwlib PROPERTIES PUBLIC_HEADER "${FW_HEADERS}")

set_target_properties(rdbfwlib PROPERTIES VERSION ${RDBFWLIB_DEFAULT_VERSION_STRING} SOVERSION ${RDBFWLIB_DEFAULT_VERSION_MAJOR} )
set_target_properties(hw_timers PROPERTIES VERSION ${RDBFWLIB_DEFAULT_VERSION_STRING} SOVERSION ${RDBFWLIB_DEFAULT_VERSION_MAJOR} )
set_target_properties(timers PROPERTIES VERSION ${RDBFWLIB_DEFAULT_VERSION_STRING} SOVERSION ${RDBFWLIB_DEFAULT_VERSION_MAJOR} )

INSTALL(TARGETS rdbfwlib hw_timers timers
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/rdbfw
)

#add_subdirectory(test)

enable_testing()
