rm a.out
clear
clear
g++ -std=c++11 -I../../../mlu/MLU270/include \
    jpeg_decode_context.cpp jpeg_decode_sample.cpp  \
    `pkg-config --cflags --libs opencv` \
    -L../../../mlu/MLU270/libs/x86_64 -lcncodec -lcnrt -lpthread

./a.out
