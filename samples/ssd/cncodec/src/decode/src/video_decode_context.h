#ifndef SRC_VIDEO_DECODE_SAMPLE_VIDEO_DECODE_CONTEXT_H_
#define SRC_VIDEO_DECODE_SAMPLE_VIDEO_DECODE_CONTEXT_H_

// ffmpeg -i cars.mp4 -vcodec copy -an  -bsf:v h264_mp4toannexb cars.h264

#include <mqueue.h>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

#include "cn_video_dec.h"
#include "cncodec_semaphore.h"

#include "cncodec_test_data_transfer.h"
#include "cncodec_test_buffer.h"
#include "cncodec_test_common.h"
#include "cncodec_test_file_reader.h"
#include "cncodec_test_statistics.h"

class VideoDecodeContext
{
public:
    explicit VideoDecodeContext(int instance_id);
    ~VideoDecodeContext();

    void BuildDecodeStartInfo(cnvideoDecCreateInfo &info) noexcept;
    bool Start(pfnCncodecEventCallback event_cb) noexcept;
    void DecodeEnd();
    void Stop(void) noexcept;
    bool ReadDecInputStream(cnvideoDecInput &dec_input) noexcept;
    bool FeedStream2Decoder(const cnvideoDecInput &dec_input) noexcept;
    int HandleNewFrameEvent(const cnvideoDecOutput& dec_output) noexcept;
    void HandleEosEvent(void) noexcept;
    void HandleOutOfMemoryEvent(void) noexcept;
    void HandleAbortEvent(void) noexcept;
    void HandleDecStartFailed(void) noexcept;
    int HandleSequenceEvent(const cnvideoDecSequenceInfo& seqinfo) noexcept;
    static int EventCallback(cncodecCbEventType eventType, void *usrptr, void *cb_data) noexcept;
    
    VideoDecodeContext(const VideoDecodeContext&)=delete;
    VideoDecodeContext& operator=(const VideoDecodeContext&)=delete;
    VideoDecodeContext(VideoDecodeContext &&)=delete;
    VideoDecodeContext& operator=(VideoDecodeContext&&)=delete;
    cnvideoDecInstance decode_instance_id{CNVIDEODEC_INSTANCE_0};
    VideoStreamReader input_strm_reader_; /*< source file reader to buffer >*/
    std::vector<cnvideoDecInput> input_datas;

private:
    static constexpr uint32_t kDecodeInputBufferSize {4 * 1024 * 1024};
    static constexpr uint32_t kMaxOutputBufNum{128};
    static constexpr uint32_t kDflOutputBufNum{6};
    static constexpr uint32_t kMinInputBufNum{3};
    static constexpr uint32_t kMinOutputBufNum{3};

    cnvideoDecoder                              decoder_;           /*< decoder handle corresponding to decoder_id >*/
    cnvideoDecSequenceInfo                      seqinfo_;           /*< decode parameters inited by app and updated by decoder after inital parse >*/
    DataTransferManager                         data_trans_mgr_;    /*< decoder output dump to file or to display, parameters determined at runtime >*/
    std::ofstream output_strm_;       /*< output dump stream >*/
    CncodecSemaphore<> eos_seamphore_;     /*< semaphore to sync stop between decoder and dec app thread >*/
    std::unique_ptr<CnCodecTestFrameBufMgr>     frame_buf_mgr_{nullptr};
    std::unique_ptr<CnCodecTestBitStreamBufMgr> strm_buf_mgr_{nullptr};
    CnCodecTestStatistics statistics_;        /*< decode channel statistics collection: frame num, total time >*/
    int width{1280};
    int height{720};
    cncodecType codec_local{CNCODEC_H264};
    int decode_channel_id{0};
    cncodecPixelFormat pix_fmt{CNCODEC_PIX_FMT_NV12};
    int device_id{0};
    u32_t out_buf_alignment{128};
    enum FileReadMode strm_read_mode{FileReadMode::STRM_READ_MODE_MAX};
    bool interlaced{false}; /*<flag to indicate src stream is interlaced file >*/
    /*<test case, force send EOS in null frame after last valid stream data >*/
    int times{10};
    int current_times{0};

    void init(void) noexcept;
    uint32_t DecideOutputBufNum(uint32_t decode_requested_min_buf_num) noexcept;
    void ForwardOutputFrame(const cnvideoDecOutput& output) noexcept;
};

#endif /*SRC_VIDEO_DECODE_SAMPLE_VIDEO_DECODE_CONTEXT_H_ */
