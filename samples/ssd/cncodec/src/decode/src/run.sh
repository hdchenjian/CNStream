rm a.out
clear
clear
g++ -std=c++11 -I../../../include -I../../../../../../mlu/MLU270/include \
    video_decode_context.cpp video_decode_sample.cpp \
    cncodec_test_common.cpp cncodec_test_file_reader.cpp cncodec_test_buffer.cpp \
    cncodec_test_data_transfer.cpp cncodec_test_statistics.cpp \
    `pkg-config --cflags --libs opencv` \
    -L../../../../../../mlu/MLU270/libs/x86_64 -lcncodec -lcnrt -lpthread

./a.out #--chan-num 1 --chan -c H264 -i 1080p.h264 --test-round 1 --enable-dump
