#include <assert.h>
#include <thread>
#include <vector>
#include <sys/time.h>
#include <iostream>
#include "cnrt.h"
#include "jpeg_decode_context.h"

int times = 10;

double what_time_is_it_now()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

static void* decodeProc(void *argv){
    JpegDecodeContext &ctx = *(reinterpret_cast<JpegDecodeContext *>(argv));
    int device_id = 0;
    double start = what_time_is_it_now();
    if (!ctx.Create()) {
        return nullptr;
    }
    for(int i = 0; i < times; i++){
        ctx.RunOneDecodeRound();
    }
    ctx.Destroy();
    double end = what_time_is_it_now();
    printf("\n thread %d spend %f seconds, %f fps \n", ctx.decode_instance_id, end - start, times / (end - start));
    return nullptr;
}

int main(int argc, char * argv[]){
    unsigned int real_dev_num;
    cnrtInit(0);
    cnrtGetDeviceCount(&real_dev_num);
    if (real_dev_num == 0) {
        std::cerr << "only have " << real_dev_num << " device(s) " << std::endl;
        cnrtDestroy();
        return -1;
    }
    cnrtDev_t dev;
    int device_id = 0;
    CNRT_CHECK(cnrtGetDeviceHandle(&dev, device_id));
    CNRT_CHECK(cnrtSetCurrentDevice(dev));

    std::vector <std::unique_ptr<JpegDecodeContext>> all_ctx;
    std::vector<std::thread> threads;
    int thread_num = 1;
    double start = what_time_is_it_now();
    for(int i = 0; i < thread_num; i++){
        all_ctx.push_back(std::unique_ptr<JpegDecodeContext> {new (std::nothrow)JpegDecodeContext(i % 6)});
        try {
            threads.emplace_back(decodeProc, all_ctx[i].get());
        } catch (const std::bad_alloc&) {
                printf("jpeg decode decode chan[%d] start fail!\n", i);
                assert(0);
            }
    }
    for (auto &thread: threads) {
        thread.join();
    }
    double end = what_time_is_it_now();
    printf("\n main thread spend %f seconds, %f fps \n", end - start, (thread_num * times) / (end - start));
    all_ctx.clear();
    cnrtDestroy();
    return 0;
}
