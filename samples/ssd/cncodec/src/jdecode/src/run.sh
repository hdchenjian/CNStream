rm a.out
g++ -std=c++11 -I../../../include -I../../../../../../mlu/MLU270/include \
    jpeg_decode_context.cpp jpeg_decode_sample.cpp \
    cncodec_test_common.cpp cncodec_test_file_reader.cpp cncodec_test_buffer.cpp \
    `pkg-config --cflags --libs opencv` \
    -L../../../../../../mlu/MLU270/libs/x86_64 -lcncodec -lcnrt -lpthread

./a.out
