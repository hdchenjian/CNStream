#include "video_decode_context.h"
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <assert.h>
#include <algorithm>

using namespace std;

inline static u32_t get_memory_channel(cnvideoDecInstance decode_instance_id) noexcept
{
    switch (decode_instance_id)
    {
        case CNVIDEODEC_INSTANCE_0: return CNRT_CHANNEL_TYPE_0;
        case CNVIDEODEC_INSTANCE_1:
        case CNVIDEODEC_INSTANCE_2: return CNRT_CHANNEL_TYPE_1;
        case CNVIDEODEC_INSTANCE_3:
        case CNVIDEODEC_INSTANCE_4: return CNRT_CHANNEL_TYPE_2;
        case CNVIDEODEC_INSTANCE_5: return CNRT_CHANNEL_TYPE_3;
        default: break;
    }

    return CNRT_CHANNEL_TYPE_0;
}

void VideoDecodeContext::init(void) noexcept {
    seqinfo_.codec           = codec_local;
    seqinfo_.width           = width;
    seqinfo_.height          = height;
    seqinfo_.minInputBufNum  = kMinInputBufNum;
    seqinfo_.minOutputBufNum = kMinOutputBufNum;

    std::string output_filename = "output.bin";
    output_strm_.open(output_filename, ios::binary|ios::ate);
    if (!output_strm_.good()) {
        LOG_ERROR("create dump file %s failed\n", output_filename.c_str());
    }
}

VideoDecodeContext::VideoDecodeContext(int instance_id):
    input_strm_reader_("cars.h264", strm_read_mode, kDecodeInputBufferSize, kDecodeInputBufferSize, codec_local)
{
    decode_instance_id = (cnvideoDecInstance)instance_id;
    init();
}

VideoDecodeContext::~VideoDecodeContext() {
    if (output_strm_.is_open()) {
        output_strm_.close();
    }
    LOG_INFO("input video frame number %lu width %d height %d\n", input_datas.size(), width, height);
    input_datas.clear();
    //LOG_INFO("resolution:      %u*%u\n",  seqinfo_.width, seqinfo_.height);
}

uint32_t VideoDecodeContext::DecideOutputBufNum(uint32_t decode_requested_min_buf_num) noexcept
{
    uint32_t buf_num = kMinOutputBufNum;
    if (decode_requested_min_buf_num >= kMaxOutputBufNum) {
        LOG_ERROR("APP cannot allocate adequate output buffer Num(%d) requested by library\n",
                  decode_requested_min_buf_num);
        assert(0);
    } else {
        buf_num = std::max(buf_num, decode_requested_min_buf_num + 1);
        LOG_TRACE("lib decode request min output frame buf num %d, app min %d\n",
                  decode_requested_min_buf_num, kMinOutputBufNum);
    }

    //LOG_INFO("DecideOutputBufNum %d\n", buf_num);
    return buf_num;
}

void VideoDecodeContext::ForwardOutputFrame(const cnvideoDecOutput &output) noexcept {
    if (output_strm_.is_open()) {
        data_trans_mgr_.SaveFrame2File(output_strm_, output.frame);
    }
}

bool VideoDecodeContext::FeedStream2Decoder(const cnvideoDecInput& dec_input) noexcept {
    if(false){
        LOG_INFO("decode chan[%d], send data: size(%u), eos%d, pts 0x%lx, flags 0x%08x\n",
                 decode_channel_id, (uint32_t)dec_input.streamLength,
                 (bool)(dec_input.flags & CNVIDEODEC_FLAG_EOS),
                 dec_input.pts, dec_input.flags);
    }

    auto ret = cnvideoDecFeedData(decoder_, const_cast<cnvideoDecInput*>(&dec_input), -1);
    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("cnvideoDecFeedData fail, ret(%d)\n", ret);
        return false;
    }
    return true;
}

void VideoDecodeContext::BuildDecodeStartInfo(cnvideoDecCreateInfo &info) noexcept {
    memset(&info, 0, sizeof(info));
    info.codec          = seqinfo_.codec;
    info.height         = height;
    info.width          = width;
    info.pixelFmt       = pix_fmt;
    info.progressive    = (interlaced == 0 ? 1: 0);
    info.userContext    = reinterpret_cast<void*>(this);
    info.instance       = decode_instance_id;
    info.deviceId       = device_id;
    info.inputBufNum  = kDflOutputBufNum;
    info.outputBufNum = DecideOutputBufNum(seqinfo_.minOutputBufNum);
    info.allocType    = CNCODEC_BUF_ALLOC_LIB;
}

bool VideoDecodeContext::Start(pfnCncodecEventCallback event_cb) noexcept {
    cnvideoDecCreateInfo info;
    BuildDecodeStartInfo(info);
    auto ret = cnvideoDecCreate(&decoder_, event_cb, &info);
    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("%s, cnvideoDecCreate fail, ret(%d)\n", __FUNCTION__, ret);
        return false;
    }
    ret =  cnvideoDecSetAttributes(decoder_, CNVIDEO_DEC_ATTR_OUT_BUF_ALIGNMENT,
                                   const_cast<u32_t*>(&out_buf_alignment));

    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("%s, cnvideoDecSetAttributes fail, ret(%d)\n", __func__, ret);
        return false;
    }
    return true;
}

void VideoDecodeContext::DecodeEnd(){
}

void VideoDecodeContext::Stop(void) noexcept {
    eos_seamphore_.wait();
    auto ret = cnvideoDecStop(decoder_);
    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("%s, cnvideoDecStop fail, ret(%d)\n", __FUNCTION__, ret);
    }
    ret = cnvideoDecDestroy(decoder_);
    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("%s, cnvideoDecDestroy fail, ret(%d)\n",__FUNCTION__, ret);
    }
    if (strm_buf_mgr_ != nullptr) {
        LOG_DEBUG("release bitstream buffer\n");
        strm_buf_mgr_.reset();
    }
}

inline void VideoDecodeContext::HandleOutOfMemoryEvent(void) noexcept {
    eos_seamphore_.post();
    LOG_ERROR("decoder chan[#%d] out of mem, force stop\n", decode_channel_id);
}

inline void VideoDecodeContext::HandleAbortEvent(void) noexcept {
    eos_seamphore_.post();
    LOG_ERROR("decoder chan[#%d] occurs abort error, force stop\n", decode_channel_id);
}

inline void VideoDecodeContext::HandleEosEvent(void) noexcept {
    eos_seamphore_.post();
    //LOG_INFO("HandleEosEvent %d\n", decode_channel_id);
    //LOG_INFO("decoder chan[#%d] finished\n", decode_channel_id);
}

inline void VideoDecodeContext::HandleDecStartFailed(void) noexcept
{
    eos_seamphore_.post();
    LOG_INFO("decoder chan[#%d] start failed, force stop\n", decode_channel_id);
}

inline int VideoDecodeContext::HandleNewFrameEvent(const cnvideoDecOutput& dec_output) noexcept
{
    //LOG_INFO("HandleNewFrameEvent\n");
    cnvideoDecAddReference(decoder_, const_cast<cncodecFrame*>(&dec_output.frame));
    //ForwardOutputFrame(dec_output);
    cnvideoDecReleaseReference(decoder_, const_cast<cncodecFrame*>(&dec_output.frame));
    return 0;
}

int VideoDecodeContext::HandleSequenceEvent(const cnvideoDecSequenceInfo& seqinfo) noexcept
{
    //LOG_INFO("HandleSequenceEvent\n");
    cnvideoDecCreateInfo start_info;
    seqinfo_ = seqinfo;
    BuildDecodeStartInfo(start_info);

    auto ret = cnvideoDecStart(decoder_, &start_info);
    if (ret != CNCODEC_SUCCESS)
    {
        LOG_ERROR("%s, cnvideoDecStart fail, ret(%d)\n", __FUNCTION__, ret);
        HandleDecStartFailed();
        return - 1;
    }
    return 0;
}

int VideoDecodeContext::EventCallback(cncodecCbEventType event, void *userptr, void *data) noexcept {
    CNCODEC_TEST_CHECK_NULL_PTR_RET(userptr, -1);
    VideoDecodeContext& ctx = *(reinterpret_cast<VideoDecodeContext*>(userptr));

    switch(event)
    {
        case CNCODEC_CB_EVENT_EOS:
            ctx.HandleEosEvent();
            break;

        case CNCODEC_CB_EVENT_SW_RESET:
        case CNCODEC_CB_EVENT_HW_RESET:
            LOG_INFO("decode error event %d\n", (int)event);
            ctx.HandleEosEvent();
            break;

        case CNCODEC_CB_EVENT_NEW_FRAME:
            CNCODEC_TEST_CHECK_NULL_PTR_RET(data, -1);
            ctx.HandleNewFrameEvent(*(cnvideoDecOutput *)data);
            break;

        case CNCODEC_CB_EVENT_SEQUENCE:
            CNCODEC_TEST_CHECK_NULL_PTR_RET(data, -1);
            ctx.HandleSequenceEvent(*(cnvideoDecSequenceInfo *)data);
            break;

        case CNCODEC_CB_EVENT_OUT_OF_MEMORY:
            ctx.HandleOutOfMemoryEvent();
            break;

        case CNCODEC_CB_EVENT_ABORT_ERROR:
            ctx.HandleAbortEvent();
            break;

        default:
            LOG_ERROR("Unknownd format(%d)\n", event);
            break;
    }
    return 0;
}

bool VideoDecodeContext::ReadDecInputStream(cnvideoDecInput &dec_input_) noexcept
{
    StreamData input_data;
    cnvideoDecInput dec_input;
    if(!input_strm_reader_.getStreamData(input_data)) {
        return false;
    }
    dec_input.streamBuf       = (u8_t*)input_data.start_addr;
    dec_input.streamLength    = input_data.end_addr_plus1 - input_data.start_addr;
    dec_input.pts             = CnCodecTestTime::pts();
    dec_input.flags           = CNVIDEODEC_FLAG_TIMESTAMP;
    dec_input.flags           |= (input_data.eos ? CNVIDEODEC_FLAG_EOS : 0);
    //LOG_INFO("dec_input.flags %d\n", dec_input.flags);
    assert(dec_input.streamLength <= kDecodeInputBufferSize);
    input_datas.push_back(dec_input);
    dec_input_ = dec_input;
    return true;
}
