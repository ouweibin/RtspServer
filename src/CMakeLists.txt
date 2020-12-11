add_executable(rtsp_h264file
               rtsp_h264file.cc
               h264_file.cc
               media/h264_source.cc
               media/media_session.cc
               media/rtp_connection.cc
               media/rtsp_connection.cc
               media/rtsp_message.cc
               media/rtsp_server.cc
)

target_link_libraries(rtsp_h264file muduo_net muduo_base pthread)

