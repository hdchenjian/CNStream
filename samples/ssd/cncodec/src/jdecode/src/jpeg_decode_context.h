#ifndef SRC_JPEG_DECODE_SAMPLE_JPEG_DECODE_CONTEXT_H_
#define SRC_JPEG_DECODE_SAMPLE_JPEG_DECODE_CONTEXT_H_

#include <fstream>
#include <vector>

#include "cn_jpeg_dec.h"

#include "cncodec_test_common.h"
#include "cncodec_test_file_reader.h"
#include "cncodec_test_buffer.h"

class JpegDecodeContext
{
public:
    explicit JpegDecodeContext(int instance_id);
    ~JpegDecodeContext(void);

    bool Create(void);
    void Destroy(void);
    void RunOneDecodeRound(void);
    bool GetFrameData(const cncodecFrame& frame, char *image_data);
    
    JpegDecodeContext(const JpegDecodeContext&)=delete;
    JpegDecodeContext& operator=(const JpegDecodeContext&)=delete;
    JpegDecodeContext(JpegDecodeContext &&)=delete;
    JpegDecodeContext& operator=(JpegDecodeContext&&)=delete;
    cnjpegDecInstance decode_instance_id = CNJPEGDEC_INSTANCE_0;

private:
    static constexpr uint32_t kDecodeOutputFrameBufferNum{1};
    static constexpr uint32_t kDecodeInputBufferBufferNum{1};
    static constexpr uint32_t kJpuDecodeFrameBufAlignment{128};

    cnjpegDecoder decoder_;  /*< decoder handle corresponding to decoder_id >*/
    std::vector<std::unique_ptr<VideoStreamReader>> input_strm_reader_; /*< source file reader to buffer >*/
    int decode_channel_id = 0;
    int device_id = 0;
    cncodecPixelFormat pix_fmt = CNCODEC_PIX_FMT_NV12;
    cncodecColorSpace color_space = CNCODEC_COLOR_SPACE_BT_709;
    int width = 1920;
    int height = 1080;
    std::vector<std::string> input_filename;
    std::vector<StreamData> streamDatas;
    CnCodecTestFrameBufMgr frame_buf_mgr_;

    void init(void);
    void BuildDecodeCreateInfo(cnjpegDecCreateInfo &info, void* usr_ctx);
    bool Decode(const StreamData &data, uint32_t input_strm_idx);
};

#endif /*SRC_JPEG_DECODE_SAMPLE_JPEG_DECODE_CONTEXT_H_ */
