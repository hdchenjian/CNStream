#include <gflags/gflags.h>
#include <glog/logging.h>
#include <future>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "cnstream_core.hpp"
#include "data_source.hpp"
#include "displayer.hpp"
#include "fps_stats.hpp"

cnstream::FpsStats* gfps_stats = nullptr;
cnstream::Displayer* gdisplayer = nullptr;

class PipelineWatcher {
 public:
  explicit PipelineWatcher(cnstream::Pipeline* pipeline) : pipeline_(pipeline) {
    LOG_IF(FATAL, pipeline == nullptr) << "pipeline is null.";
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
      std::this_thread::sleep_for(std::chrono::milliseconds(duration_));
      if (gfps_stats) {
        gfps_stats->ShowStatistics();
      } else {
        std::cout << "FpsStats has not been added to pipeline, fps will not be print." << std::endl;
      }
    }
  }
  bool running_ = false;
  std::thread thread_;
  int duration_ = 2000;  // ms
  cnstream::Pipeline* pipeline_ = nullptr;
};  // class Pipeline Watcher

class MsgObserver : cnstream::StreamMsgObserver {
 public:
  MsgObserver(int chn_cnt, cnstream::Pipeline* pipeline) : chn_cnt_(chn_cnt), pipeline_(pipeline) {}

  void Update(const cnstream::StreamMsg& smsg) override {
    if (stop_) return;
    if (smsg.type == cnstream::StreamMsgType::EOS_MSG) {
      eos_chn_.push_back(smsg.chn_idx);
      LOG(INFO) << "[Observer] received EOS from channel:" << smsg.chn_idx;
      if (static_cast<int>(eos_chn_.size()) == chn_cnt_) {
        LOG(INFO) << "[Observer] received all EOS";
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

  std::list<std::string> video_urls;
  video_urls.push_back("a.mp4");

  cnstream::Pipeline pipeline("pipeline");

  if (0 != pipeline.BuildPipelineByJSONFile("detection_config.json")) {
    LOG(ERROR) << "Build pipeline failed.";
    return EXIT_FAILURE;
  }

  MsgObserver msg_observer(static_cast<int>(video_urls.size()), &pipeline);
  pipeline.SetStreamMsgObserver(reinterpret_cast<cnstream::StreamMsgObserver*>(&msg_observer));

  cnstream::DataSource* source = dynamic_cast<cnstream::DataSource*>(pipeline.GetModule("source"));
  if (nullptr == source) {
    LOG(ERROR) << "DataSource module not found.";
    return EXIT_FAILURE;
  }

  if (!pipeline.Start()) {
    LOG(ERROR) << "Pipeline start failed.";
    return EXIT_FAILURE;
  }

  int streams = static_cast<int>(video_urls.size());
  auto url_iter = video_urls.begin();
  for (int i = 0; i < streams; i++, url_iter++) {
    const std::string& filename = *url_iter;
    int src_frame_rate = 30;
    source->AddVideoSource(std::to_string(i), filename, src_frame_rate);
  }

  /* watcher, for rolling print */
  gfps_stats = dynamic_cast<cnstream::FpsStats*>(pipeline.GetModule("fps_stats"));
  gdisplayer = dynamic_cast<cnstream::Displayer*>(pipeline.GetModule("displayer"));
  PipelineWatcher watcher(&pipeline);
  watcher.Start();

  auto quit_callback = [&pipeline, streams, &source]() {
    for (int i = 0; i < streams; i++) {
      source->RemoveSource(std::to_string(i));
    }
    pipeline.Stop();
  };

  if (gdisplayer && gdisplayer->Show()) {
    gdisplayer->GUILoop(quit_callback);
  } else {
      msg_observer.WaitForStop();
  }
  watcher.Stop();

  if (gfps_stats)
    gfps_stats->ShowStatistics();

  google::ShutdownGoogleLogging();
  return EXIT_SUCCESS;
}
