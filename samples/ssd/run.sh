export LD_LIBRARY_PATH="../../lib:/usr/local/neuware/lib64"
g++ -std=c++11 -DHAVE_OPENCV  \
    -I../../modules/source/include \
    -I../../modules/display/include \
    -I../../modules/inference/include \
    -I../../easydk/include \
    -I../../modules/fps_stats/include \
    demo.cpp \
    `pkg-config --cflags --libs opencv` -L../../lib -lcnstream -lglog -lgflags -lpthread \
    -L/usr/local/neuware/lib64 -lcnrt
