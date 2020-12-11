set(BASE_SOURCES
    AsyncLogging.cc
    Condition.cc
    CountDownLatch.cc
    CurrentThread.cc
    Date.cc
    Exception.cc
    FileUtil.cc
    LogFile.cc
    Logging.cc
    LogStream.cc
    ProcessInfo.cc
    Thread.cc
    ThreadPool.cc
    Timestamp.cc
    TimeZone.cc
   )

add_library(muduo_base ${BASE_SOURCES})

install(TARGETS muduo_base DESTINATION ${PROJECT_SOURCE_DIR}/lib)

