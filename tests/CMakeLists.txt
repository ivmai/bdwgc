# (C) Jack Andrews 2008
# licensed under GPL 2.0

ADD_DEFINITIONS(-DGC_NOT_DLL)
ADD_EXECUTABLE(gctest WIN32 test.c)
TARGET_LINK_LIBRARIES(gctest gc-lib)
