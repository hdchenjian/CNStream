#include <assert.h>
#include <thread>
#include <vector>
#include <pthread.h>
#include <iostream>
#include <sys/time.h>

#include "video_decode_context.h"
#include "cncodec_test_config_parser.h"

int times = 5;
int video_frame_number = 1;
double what_time_is_it_now()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

static bool singleDecodeRound(VideoDecodeContext &ctx) {
    if (!ctx.Start(VideoDecodeContext::EventCallback)) {
         return false;
    }
    bool eos = false;
    int i = 0;
    while (i < ctx.input_datas.size()) {
        cnvideoDecInput dec_input_ = ctx.input_datas[i];
        if (!ctx.FeedStream2Decoder(dec_input_)) {
            LOG_ERROR("%s, fail to feed stream to decoder, exit\n", __FUNCTION__);
            break;
        }
        i++;
    }
    ctx.Stop();
    return true;
}

static void* decodeProc(void *argv) {
    CNCODEC_TEST_CHECK_NULL_PTR_RET(argv, nullptr);
    VideoDecodeContext &ctx = *(reinterpret_cast<VideoDecodeContext *>(argv));
    int device_id = 0;
    CnCodecTestDevMemOp::cnrt_init(device_id);

    bool eos = false;
    cnvideoDecInput dec_input_;
    while (!eos) {
        if(!ctx.ReadDecInputStream(dec_input_)) {
            LOG_ERROR("%s, fail to read decoder input stream, exit\n", __FUNCTION__);
            return nullptr;
        }
        eos = ((dec_input_.flags & CNVIDEODEC_FLAG_EOS) != 0);
    }
    //LOG_INFO("input_datas.size %lu\n", ctx.input_datas.size());
    double start = what_time_is_it_now();
    for(int i = 0; i < times; i++){
        if(i % 5 == 0) printf("thread %d round %d\n", ctx.decode_instance_id, i);
        singleDecodeRound(ctx);
        //ctx.input_strm_reader_.reset();
    }

    double end = what_time_is_it_now();
    printf("\n thread %d spend %f seconds, %f fps \n",
           ctx.decode_instance_id, end - start, times * ctx.input_datas.size() / (end - start));
    if(ctx.decode_instance_id == 0) video_frame_number = ctx.input_datas.size();
    ctx.input_datas.size();
    return nullptr;
}

int main(int argc, char * argv[])
{
    unsigned int real_dev_num;
    cnrtInit(0);
    cnrtGetDeviceCount(&real_dev_num);
    if (real_dev_num == 0) {
        std::cerr << "only have " << real_dev_num << " device(s) " << std::endl;
        cnrtDestroy();
        return -1;
    }

    std::vector <std::unique_ptr<VideoDecodeContext>> all_ctx;
    std::vector<std::thread> threads;
    int thread_num = 6*3;
    double start = what_time_is_it_now();
    for(int i = 0; i < thread_num; i++){
        all_ctx.push_back(std::unique_ptr<VideoDecodeContext> {new (std::nothrow)VideoDecodeContext(i % 6)});
        threads.emplace_back(decodeProc, all_ctx[i].get());
    }
    for (auto &thread: threads) {
        thread.join();
    }
    double end = what_time_is_it_now();
    printf("\n main thread spend %f seconds, %f fps \n", end - start,
           (thread_num * times * video_frame_number) / (end - start));
    all_ctx.clear();
    cnrtDestroy();
    return EXIT_SUCCESS;
}
