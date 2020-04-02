#include <gflags/gflags.h>
#include <glog/logging.h>
#include <future>
#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <sys/time.h>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "cnstream_core.hpp"
#include "fps_stats.hpp"

#include "/usr/local/neuware/include/cnrt.h"

bool LOG_ON = false;

double what_time_is_it_now()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

class SsdNet {
public:
    SsdNet(std::string model_path);
    void run_network(cv::Mat &img1, std::vector<std::vector<float>> &detections);

    cnrtModel_t model_;
    cnrtFunction_t function_;
    cnrtRuntimeContext_t runtime_ctx_;

    cnrtQueue_t queue_;
    cnrtNotifier_t start_notifier_;
    cnrtNotifier_t end_notifier_;
    int64_t* inputSizeS_, *outputSizeS_;
    int inputNum_, outputNum_;
    void** inputMluPtrS;
    void** outputMluPtrS;
    int in_n_, in_c_, in_h_, in_w_;
    int *out_n_, *out_c_, *out_h_, *out_w_;
    void* cpu_input_data_cast;
    float* cpu_input_data;
    void** net_param;
    void** output_data_cast;
    float** outputCpu;
    int out_count_;

    ~SsdNet() {
        cnrtFreeArray(inputMluPtrS, inputNum_);
        cnrtFreeArray(outputMluPtrS, outputNum_);

        cnrtDestroyFunction(function_);
        cnrtDestroyQueue(queue_);
        cnrtDestroyNotifier(&start_notifier_);
        cnrtDestroyNotifier(&end_notifier_);
        cnrtDestroyRuntimeContext(runtime_ctx_);
        cnrtUnloadModel(model_);
        free(out_n_);
        free(out_c_);
        free(out_h_);
        free(out_w_);
        free(output_data_cast);
        free(outputCpu);
        free(cpu_input_data_cast);
        free(cpu_input_data);
        free(net_param);
        //std::cout << "~SsdNet" << std::endl;
    }
};

SsdNet::SsdNet(std::string model_path){
    int dev_id_ = 0;
    cnrtLoadModel(&model_, model_path.c_str());
    std::string name = "subnet0";

    cnrtDev_t dev;
    CNRT_CHECK(cnrtGetDeviceHandle(&dev, dev_id_));
    CNRT_CHECK(cnrtSetCurrentDevice(dev));
    if (LOG_ON) {
        std::cout << "Init SsdNet for device " << dev_id_ << std::endl;
    }

    CNRT_CHECK(cnrtCreateFunction(&(function_)));
    CNRT_CHECK(cnrtExtractFunction(&(function_), model_, name.c_str()));

    CNRT_CHECK(cnrtCreateRuntimeContext(&runtime_ctx_, function_, nullptr));
    cnrtSetRuntimeContextDeviceId(runtime_ctx_, dev_id_);

    if (cnrtInitRuntimeContext(runtime_ctx_, nullptr) != CNRT_RET_SUCCESS) {
        std::cout << "Failed to init runtime context" << std::endl;
        return;
    }

    CNRT_CHECK(cnrtGetInputDataSize(&inputSizeS_, &inputNum_, function_));
    if (LOG_ON) {
        std::cout << "model input num: " << inputNum_ << " input size " << *inputSizeS_ << std::endl;
    }
    CNRT_CHECK(cnrtGetOutputDataSize(&outputSizeS_, &outputNum_, function_));
    if (LOG_ON) {
        std::cout << "model output num: " << outputNum_ << " output size " << *outputSizeS_ << std::endl;
    }

    inputMluPtrS = (void**)malloc(sizeof(void*) * inputNum_);
    for (int k = 0; k < inputNum_; k++){
        cnrtMalloc(&(inputMluPtrS[k]), inputSizeS_[k]);
    }
    outputMluPtrS = (void**)malloc(sizeof(void*) * outputNum_);
    for (int j = 0; j < outputNum_; j++){
        cnrtMalloc(&(outputMluPtrS[j]), outputSizeS_[j]);
    }

    CNRT_CHECK(cnrtCreateQueue(&queue_));
    CNRT_CHECK(cnrtCreateNotifier(&start_notifier_));
    CNRT_CHECK(cnrtCreateNotifier(&end_notifier_));

    int *dimValues = nullptr;
    int dimNum = 0;
    CNRT_CHECK(cnrtGetInputDataShape(&dimValues, &dimNum, 0, function_));
    in_n_ = dimValues[0];
    in_h_ = dimValues[1];
    in_w_ = dimValues[2];
    in_c_ = dimValues[3];
    free(dimValues);
    if (LOG_ON) {
        std::cout << "model input dimNum: " << dimNum << " N: " << in_n_ << " H: " << in_h_
                  << " W: " << in_w_ << " C: " << in_c_ << std::endl;
    }

    int input_size = in_n_ * in_h_ * in_w_ * in_c_;
    cpu_input_data_cast = malloc(cnrtDataTypeSize(CNRT_FLOAT16) * input_size);
    cpu_input_data = (float *)malloc(input_size * sizeof(float));
    net_param = (void**)malloc((inputNum_ + outputNum_) * sizeof(void *));


    output_data_cast = (void **)malloc(sizeof(void **) * outputNum_);
    outputCpu = (float**)malloc(sizeof(float*) * outputNum_);
    out_n_ = (int *)malloc(sizeof(int) * outputNum_);
    out_h_ = (int *)malloc(sizeof(int) * outputNum_);
    out_w_ = (int *)malloc(sizeof(int) * outputNum_);
    out_c_ = (int *)malloc(sizeof(int) * outputNum_);
    for (int j = 0; j < outputNum_; j++){
        CNRT_CHECK(cnrtGetOutputDataShape(&dimValues, &dimNum, j, function_));
        out_n_[j] = dimValues[0];
        out_h_[j] = dimValues[1];
        out_w_[j] = dimValues[2];
        out_c_[j] = dimValues[3];
        out_count_ = out_n_[j] * out_h_[j] * out_w_[j] * out_c_[j];
        free(dimValues);
        if (LOG_ON) {
            std::cout << "model output " << j << " dimNum: " << dimNum << " N: " << out_n_[j] << " H: " << out_h_[j]
                      << " W: " << out_w_[j] << " C: " << out_c_[j] << " outputSize "
                      << outputSizeS_[j] << std::endl;
        }
    }
}

void SsdNet::run_network(cv::Mat &image, std::vector<std::vector<float>> &detections) {
    if (LOG_ON) {
        std::cout << "image w x h x c: " << image.cols << " x " << image.rows
                  << " x " << image.channels() << std::endl;
    }
    cv::Mat img;
    cv::resize(image, img, cv::Size(in_w_, in_h_));
    cv::Mat img_float;
    img.convertTo(img_float, CV_32FC3);

    int channels = img.channels();
    float mean_value[3] = {104.0F, 117.0F, 123.0F};
    for(int k = 0; k < channels; ++k){
        float *cpu_data_ptr = cpu_input_data + k * img.rows * img.cols;
        for(int i = 0; i < img.rows; ++i){
            for(int j = 0; j < img.cols; ++j){
                cpu_data_ptr[i * img.cols + j] = img_float.at<cv::Vec3f>(i, j)[k] - mean_value[k];
            }
        }
    }
    int dim_values[4] = {in_n_, in_c_, in_h_, in_w_};
    int dim_order[4] = {0, 2, 3, 1};  // NCHW --> NHWC

    CNRT_CHECK(cnrtTransOrderAndCast(cpu_input_data, CNRT_FLOAT32,
                                     cpu_input_data_cast, CNRT_FLOAT16,
                                     nullptr, 4, dim_values, dim_order));
    //CNRT_CHECK(cnrtCastDataType(cpu_input_data, CNRT_FLOAT32,
    //                            cpu_input_data_cast, CNRT_FLOAT16, input_size, nullptr));
    CNRT_CHECK(cnrtMemcpy(inputMluPtrS[0], cpu_input_data_cast,
                          inputSizeS_[0], CNRT_MEM_TRANS_DIR_HOST2DEV));

    for (int j = 0; j < inputNum_; j++) {
        net_param[j] = inputMluPtrS[j];
    }
    for (int j = 0; j < outputNum_; j++) {
        net_param[inputNum_ + j] = outputMluPtrS[j];
    }

    CNRT_CHECK(cnrtInvokeRuntimeContext(runtime_ctx_, net_param, queue_, nullptr));

    if (cnrtSyncQueue(queue_) == CNRT_RET_SUCCESS) {
        //std::cout << "SyncStream success" << std::endl;
    } else {
        std::cout << "SyncStream error" << std::endl;
        return;
    }

    for (int j = 0; j < outputNum_; j++){
        out_count_ = out_n_[j] * out_h_[j] * out_w_[j] * out_c_[j];
        output_data_cast[j] = malloc(sizeof(float) * out_count_);
        CNRT_CHECK(cnrtMemcpy(output_data_cast[j], outputMluPtrS[j],
                              outputSizeS_[j], CNRT_MEM_TRANS_DIR_DEV2HOST));
        outputCpu[j] = (float*)malloc(cnrtDataTypeSize(CNRT_FLOAT32) * out_count_);
        CNRT_CHECK(cnrtCastDataType(output_data_cast[j], CNRT_FLOAT16,
                                    outputCpu[j], CNRT_FLOAT32, out_count_, nullptr));

        int count = out_c_[j];
        int output_num = outputCpu[j][j * out_c_[j]];
        if(LOG_ON){
            std::cout << "outputNum_ " << outputNum_ << " out_c_ " << out_c_[j]
                      << " output_num " << output_num << std::endl;
        }
        for (int k = 0; k < output_num; k++) {
            int index = j * out_c_[j] + 64 + k * 7;
            if ((int)outputCpu[j][index] != j) continue;

            std::vector<float> single_detection(outputCpu[j] + index, outputCpu[j] + index + 7);
            const int class_ = single_detection[1];
            const float score = single_detection[2];
            float threshold = 0.5F;
            if (score < threshold) continue;

            float xmin = single_detection[3] * image.cols;
            float ymin = single_detection[4] * image.rows;
            float xmax = single_detection[5] * image.cols;
            float ymax = single_detection[6] * image.rows;
            detections.push_back(single_detection);
            //std::cout << k << " class " << class_ << " score " << score <<  " xmin " << xmin
            //          <<  " ymin " << ymin <<  " xmax " << xmax <<  " ymax " << ymax << std::endl;
            //cv::rectangle(image, cvPoint(xmin, ymin), cvPoint(xmax, ymax), cvScalar(255,0,0), 4);
            //cv::imwrite("detector_out.jpg", image);
        }
        free(output_data_cast[j]);
        free(outputCpu[j]);
    }
    return;
}

class ModuleDataSource : public cnstream::Module, public cnstream::ModuleCreator<ModuleDataSource> {
public:
    explicit ModuleDataSource(const std::string &name) : cnstream::Module(name) {}
    bool Open(cnstream::ModuleParamSet paramSet) override {
        std::cout << this->GetName() << " Open called" << std::endl;
        for (auto &v : paramSet) {
            std::cout << "\t" << v.first << " : " << v.second << std::endl;
        }
        return true;
    }
    void Close() override { std::cout << this->GetName() << " Close called" << std::endl; }
    int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
        std::cout << "For a source module, Process() will not be invoked\n";
        return 0;
    }

private:
    ModuleDataSource(const ModuleDataSource &) = delete;
    ModuleDataSource &operator=(ModuleDataSource const &) = delete;
};

class InferenceModule : public cnstream::Module, public cnstream::ModuleCreator<InferenceModule> {
public:
    explicit InferenceModule(const std::string &name) : cnstream::Module(name) {}
    bool Open(cnstream::ModuleParamSet paramSet) override {
        std::cout << this->GetName() << " Open called" << std::endl;
        for (auto &v : paramSet) {
            std::cout << "\t" << v.first << " : " << v.second << std::endl;
            if(0 == v.first.compare("model_path")) model_path = v.second;
            else if(0 == v.first.compare("threshold")) threshold = std::stof(v.second);
            else if(0 == v.first.compare("device_id")) device_id = std::stoi(v.second);
        }
        return true;
    }
    void Close() override {
        std::cout << this->GetName() << " Close called" << std::endl;
        for(std::map<std::thread::id, SsdNet*>::iterator it = ssd_nets.begin(); it != ssd_nets.end(); it++){
            delete it->second;
        }
        ssd_nets.clear();
    }
    int Process(std::shared_ptr<cnstream::CNFrameInfo> data) override {
        std::thread::id thread_id = std::this_thread::get_id();
        //std::cout << this->GetName() << " process: " << data->frame.stream_id << "--"
        //          << data->frame.frame_id << " : " << thread_id << std::endl;

        SsdNet* ssd;
        std::map<std::thread::id, SsdNet*>::iterator it = ssd_nets.find(thread_id);
        bool show_detection_result = false;
        if (it == ssd_nets.end()){
            std::cout << "[InferenceModule Process] init SsdNet" << std::endl;
            ssd = new SsdNet(model_path);
            ssd_nets[thread_id] = ssd;
            show_detection_result = true;
        } else {
            ssd = ssd_nets[thread_id];
        }
        std::vector<std::vector<float>> detections;
        cv::Mat image(data->frame.height, data->frame.width, CV_8UC3, data->frame.ptr_cpu[0]);
        ssd->run_network(image,detections);
        if(show_detection_result){
            const char *label_to_name[] = {"background","aeroplane","bicycle","bird","boat","bottle",
                                           "bus","car","cat","chair","cow","diningtable","dog","horse",
                                           "motorbike","person","pottedplant","sheep","sofa","train","tvmonitor"};
            for(int i = 0; i < detections.size(); i++){
                std::cout << "detection_result label: " << label_to_name[(int)detections[i][1]]
                          << " score: " << detections[i][2] << " bbox: "
                          << detections[i][3] << " " << detections[i][4] << " "
                          << detections[i][5] << " " << detections[i][6] << std::endl;
            }
        }
        return 0;
    }

private:
    InferenceModule(const InferenceModule &) = delete;
    InferenceModule &operator=(InferenceModule const &) = delete;
    static std::map<std::thread::id, SsdNet*> ssd_nets;
    std::string model_path;
    float threshold = 0.5F;
    int device_id = 0;
};
std::map<std::thread::id, SsdNet*> InferenceModule::ssd_nets;

class PipelineWatcher {
public:
    explicit PipelineWatcher(cnstream::FpsStats* gfps_stats_) : gfps_stats(gfps_stats_) {
    }

    void SetDuration(int ms) { duration_ = ms; }

    void Start() {
        if (thread_.joinable()) {
            running_ = false;
            thread_.join();
        }
        running_ = true;
        thread_ = std::thread(&PipelineWatcher::ThreadFunc, this);
    }

    void Stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    ~PipelineWatcher() {}

private:
    void ThreadFunc() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(duration_ / 2));
            if(!running_) break;
            if (gfps_stats) {
                gfps_stats->ShowStatistics();
            } else {
                std::cout << "FpsStats has not been added to pipeline, fps will not be print." << std::endl;
            }
        }
    }
    bool running_ = false;
    std::thread thread_;
    int duration_ = 5000;  // ms
    cnstream::FpsStats* gfps_stats = nullptr;
};

class MsgObserver : cnstream::StreamMsgObserver {
public:
    MsgObserver(int chn_cnt, cnstream::Pipeline* pipeline) : chn_cnt_(chn_cnt), pipeline_(pipeline) {}

    void Update(const cnstream::StreamMsg& smsg) override {
        if (stop_) return;
        if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
            eos_chn_.push_back(smsg.chn_idx);
            std::cout << "[Observer] received EOS from channel:" << smsg.chn_idx << std::endl;
            if (static_cast<int>(eos_chn_.size()) == chn_cnt_) {
                std::cout << "[Observer] received all EOS" << std::endl;
                stop_ = true;
                wakener_.set_value(0);
            }
        } else if (smsg.type == cnstream::StreamMsgType::ERROR_MSG) {
            LOG(ERROR) << "[Observer] received ERROR_MSG";
            stop_ = true;
            wakener_.set_value(1);
        }
    }

    void WaitForStop() {
        wakener_.get_future().get();
        pipeline_->Stop();
    }

private:
    const int chn_cnt_ = 0;
    cnstream::Pipeline* pipeline_ = nullptr;
    bool stop_ = false;
    std::vector<int> eos_chn_;
    std::promise<int> wakener_;
};

int main(int argc, char** argv) {
    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, false);

    std::cout << "\033[01;31m"
              << "CNSTREAM VERSION:" << cnstream::VersionString() << "\033[0m" << std::endl;
    unsigned int real_dev_num;
    cnrtInit(0);
    cnrtGetDeviceCount(&real_dev_num);
    if (real_dev_num == 0) {
        std::cerr << "only have " << real_dev_num << " device(s) " << std::endl;
        cnrtDestroy();
        return -1;
    }

    cnstream::Pipeline pipeline("pipeline");
    int build_ret = pipeline.BuildPipelineByJSONFile("detection_config.json");
    std::cout << "Build pipeline " << build_ret << std::endl;

    int stream_num = 6;
    MsgObserver msg_observer(stream_num, &pipeline);
    pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

    cnstream::Module* source = pipeline.GetModule("source");
    if (nullptr == source) {
        LOG(ERROR) << "DataSource module not found.";
        return EXIT_FAILURE;
    }

    if (!pipeline.Start()) {
        LOG(ERROR) << "Pipeline start failed.";
        return EXIT_FAILURE;
    }
    /* watcher, for rolling print */
    cnstream::FpsStats* gfps_stats = dynamic_cast<cnstream::FpsStats*>(pipeline.GetModule("fps_stats"));
    PipelineWatcher watcher(gfps_stats);
    watcher.Start();
    double start = what_time_is_it_now();

    std::vector<std::thread> threads;
    cv::Mat img = cv::imread("test.jpg");
    cnstream::DevContext dev_ctx_;
    dev_ctx_.dev_type = cnstream::DevContext::MLU;
    dev_ctx_.dev_id = 0;
    int times = 1000;
    for (int j = 0; j < stream_num; j++) {
        threads.push_back(std::thread([&, j]() {
                    std::string stream_id("stream_id_");
                    stream_id += std::to_string(j);
                    for (int i = 0; i < times; i++) {
                        std::shared_ptr<cnstream::CNFrameInfo> data = cnstream::CNFrameInfo::Create(stream_id);
                        cnstream::CNDataFrame &frame = data->frame;
                        data->channel_idx = j;
                        data->frame.ctx = dev_ctx_;
                        frame.fmt = cnstream::CN_PIXEL_FORMAT_BGR24;
                        frame.width = img.cols;
                        frame.height = img.rows;
                        frame.stride[0] = img.step;
                        data->frame.frame_id = i;
                        frame.timestamp = 1000;
                        //frame.ptr_mlu[0] = img.data;
                        frame.ptr_cpu[0] = img.data;
                        pipeline.ProvideData(source, data);
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }

                    std::shared_ptr<cnstream::CNFrameInfo> data_eos = cnstream::CNFrameInfo::Create(stream_id, true);
                    data_eos->channel_idx = j;
                    pipeline.ProvideData(source, data_eos);
                }));
    }
    for (auto &thread : threads) {
        thread.join();
    }
    std::cout << "ProvideData joined"  << std::endl;

    msg_observer.WaitForStop();
    if (gfps_stats) gfps_stats->ShowStatistics();
    watcher.Stop();
    double end = what_time_is_it_now();
    printf("\nmain spend %f seconds, %f fps \n", end - start, (stream_num * times) / (end - start));
    google::ShutdownGoogleLogging();
    cnrtDestroy();
    return EXIT_SUCCESS;
}
