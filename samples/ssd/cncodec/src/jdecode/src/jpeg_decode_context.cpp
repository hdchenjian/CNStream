#include <unistd.h>
#include <assert.h>
#include <cstring>
#include <algorithm>
#include <sstream>

#include "jpeg_decode_context.h"
#include "cncodec_test_common.h"
#include "cnrt.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;

enum class PlaneType
{
    NV12_PLANE_Y = 0,         NV12_PLANE_UV,
    NV21_PLANE_Y,             NV21_PLANE_VU,
    I420_PLANE_Y,             I420_PLANE_U,                I420_PLANE_V,
    YV12_PLANE_Y,             YV12_PLANE_V,                YV12_PLANE_U,
    YUYV_PLANE,
    UYVY_PLANE,
    YVYU_PLANE,
    VYUY_PLANE,
    P010_PLANE_Y,             P010_PLANE_UV,
    YUV420_10BIT_PLANE_Y,     YUV420_10BIT_PLANE_U, YUV420_10BIT_PLANE_V,
    YUV444_10BIT_PLANE_Y,     YUV444_10BIT_U,             YUV444_10BIT_V,
    ARGB_PLANE,
    BGRA_PLANE,
    RGBA_PLANE,
    ABGR_PLANE,
    AYUV_PLANE,
    RGB565_PLANE,
    RAW_PLANE,
    INVALID = -1,
};

static inline u32_t get_plane_num(cncodecPixelFormat format)
{
    using PixelFormatPlaneNumMap = std::pair<cncodecPixelFormat, u32_t>;

    static constexpr  PixelFormatPlaneNumMap kMap[]
    {
        {CNCODEC_PIX_FMT_NV12,         2},
        {CNCODEC_PIX_FMT_NV21,         2},
        {CNCODEC_PIX_FMT_I420,         3},
        {CNCODEC_PIX_FMT_YV12,         3},
        {CNCODEC_PIX_FMT_YUYV,         1},
        {CNCODEC_PIX_FMT_UYVY,         1},
        {CNCODEC_PIX_FMT_YVYU,         1},
        {CNCODEC_PIX_FMT_VYUY,         1},
        {CNCODEC_PIX_FMT_P010,         2},
        {CNCODEC_PIX_FMT_YUV420_10BIT, 2},
        {CNCODEC_PIX_FMT_YUV444_10BIT, 3},
        {CNCODEC_PIX_FMT_ARGB,         1},
        {CNCODEC_PIX_FMT_BGRA,         1},
        {CNCODEC_PIX_FMT_ABGR,         1},
        {CNCODEC_PIX_FMT_RGBA,         1},
        {CNCODEC_PIX_FMT_AYUV,         1},
        {CNCODEC_PIX_FMT_RGB565,       1},
        {CNCODEC_PIX_FMT_RAW,          1},
    };

    for (u32_t i = 0; i < sizeof(kMap)/sizeof(PixelFormatPlaneNumMap); i++)
    {
        if (kMap[i].first == format)
        {
            return kMap[i].second;
        }
    }

    return 0;
}

static inline const enum PlaneType* get_planars(cncodecPixelFormat format)
{
    static constexpr enum PlaneType kPlanes[][CnCodecTestFrameBufWrapper::kMaxPlaneNumber]
    {
        {PlaneType::NV12_PLANE_Y,          PlaneType::NV12_PLANE_UV,         PlaneType::INVALID},
        {PlaneType::NV21_PLANE_Y,          PlaneType::NV21_PLANE_VU,         PlaneType::INVALID},
        {PlaneType::I420_PLANE_Y,          PlaneType::I420_PLANE_U,          PlaneType::I420_PLANE_V},
        {PlaneType::YV12_PLANE_Y,          PlaneType::YV12_PLANE_V,          PlaneType::YV12_PLANE_U},
        {PlaneType::YUYV_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::UYVY_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::YVYU_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::VYUY_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::P010_PLANE_Y,          PlaneType::P010_PLANE_UV,         PlaneType::INVALID},
        {PlaneType::YUV420_10BIT_PLANE_Y,  PlaneType::YUV420_10BIT_PLANE_U, PlaneType::YUV420_10BIT_PLANE_V},
        {PlaneType::YUV444_10BIT_PLANE_Y,  PlaneType::YUV444_10BIT_U,        PlaneType::YUV444_10BIT_V},
        {PlaneType::ARGB_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::BGRA_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::ABGR_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::RGBA_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::AYUV_PLANE,            PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::RGB565_PLANE,          PlaneType::INVALID,               PlaneType::INVALID},
        {PlaneType::RAW_PLANE,             PlaneType::INVALID,               PlaneType::INVALID},
    };

    using PixFormatPlanarTypeMap = std::pair<cncodecPixelFormat, const enum PlaneType*>;

    static constexpr PixFormatPlanarTypeMap kMap[]
    {
        {CNCODEC_PIX_FMT_NV12,         kPlanes[0]},
        {CNCODEC_PIX_FMT_NV21,         kPlanes[1]},
        {CNCODEC_PIX_FMT_I420,         kPlanes[2]},
        {CNCODEC_PIX_FMT_YV12,         kPlanes[3]},
        {CNCODEC_PIX_FMT_YUYV,         kPlanes[4]},
        {CNCODEC_PIX_FMT_UYVY,         kPlanes[5]},
        {CNCODEC_PIX_FMT_YVYU,         kPlanes[6]},
        {CNCODEC_PIX_FMT_VYUY,         kPlanes[7]},
        {CNCODEC_PIX_FMT_P010,         kPlanes[8]},
        {CNCODEC_PIX_FMT_YUV420_10BIT, kPlanes[9]},
        {CNCODEC_PIX_FMT_YUV444_10BIT, kPlanes[10]},
        {CNCODEC_PIX_FMT_ARGB,         kPlanes[11]},
        {CNCODEC_PIX_FMT_BGRA,         kPlanes[12]},
        {CNCODEC_PIX_FMT_ABGR,         kPlanes[13]},
        {CNCODEC_PIX_FMT_RGBA,         kPlanes[13]},
        {CNCODEC_PIX_FMT_AYUV,         kPlanes[14]},
        {CNCODEC_PIX_FMT_RGB565,       kPlanes[14]},
        {CNCODEC_PIX_FMT_RAW,          kPlanes[15]},
    };

    for (u32_t i = 0; i < sizeof(kMap)/sizeof(PixFormatPlanarTypeMap); i++)
    {
        if (kMap[i].first == format)
        {
            return kMap[i].second;
        }
    }

    return nullptr;
}

/** HW physical layout is determined by system HW architecture
 *  for low latency purpose, JPU selects most nearby DRAM channel
 */
inline static u32_t get_memory_channel(cnjpegDecInstance decode_instance_id)
{
    switch (decode_instance_id)
    {
        case CNJPEGDEC_INSTANCE_0: return CNRT_CHANNEL_TYPE_0;
        case CNJPEGDEC_INSTANCE_1:
        case CNJPEGDEC_INSTANCE_2: return CNRT_CHANNEL_TYPE_1;
        case CNJPEGDEC_INSTANCE_3:
        case CNJPEGDEC_INSTANCE_4: return CNRT_CHANNEL_TYPE_2;
        case CNJPEGDEC_INSTANCE_5: return CNRT_CHANNEL_TYPE_3;
        default: break;
    }

    return CNRT_CHANNEL_TYPE_0;
}

static uint32_t get_filesize(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (file.good())
    {
        std::streampos fsize = file.tellg();
        file.seekg(0, std::ios::end);
        fsize = file.tellg() - fsize;
        file.close();

        return fsize;
    }
    return 0;
}

static inline u32_t align (u32_t val, u32_t base)
{
    return ((val + (base-1)) & (~(base-1)));
}

static u32_t calc_plane(cncodecFrame& frame, u32_t idx, PlaneType type, const u32_t alignment)
{
    uint32_t plane_size = 0;

    switch (type)
    {
        case PlaneType::I420_PLANE_Y:
        case PlaneType::YV12_PLANE_Y:
        case PlaneType::NV12_PLANE_Y:
        case PlaneType::NV21_PLANE_Y:
            frame.stride[idx] = align(frame.width, alignment);
            plane_size = frame.stride[idx] * frame.height;
            break;

        case PlaneType::NV12_PLANE_UV:
        case PlaneType::NV21_PLANE_VU:
            frame.stride[idx] = align(frame.width, alignment);
            plane_size = (frame.stride[idx] * frame.height)>>1;
            break;

        case PlaneType::I420_PLANE_U:
        case PlaneType::YV12_PLANE_V:
        case PlaneType::I420_PLANE_V:
        case PlaneType::YV12_PLANE_U:
            frame.stride[idx] = align(frame.width>>1, alignment);
            plane_size = (frame.stride[idx] * frame.height)>>1;
            break;

        case PlaneType::P010_PLANE_Y:
        case PlaneType::YUV420_10BIT_PLANE_Y:
            frame.stride[idx] = align(frame.width<<1, alignment);
            plane_size = frame.stride[idx] * frame.height;
            break;

        case PlaneType::P010_PLANE_UV:
            frame.stride[idx] = align(frame.width<<1, alignment);
            plane_size = (frame.stride[idx] * frame.height)>>1;
            break;

        case PlaneType::YUV420_10BIT_PLANE_U:
        case PlaneType::YUV420_10BIT_PLANE_V:
            frame.stride[idx] = align(frame.width, alignment);
            plane_size = (frame.stride[idx] * frame.height)>>1;
            break;

        case PlaneType::YUYV_PLANE:
        case PlaneType::UYVY_PLANE:
        case PlaneType::YVYU_PLANE:
        case PlaneType::VYUY_PLANE:
            frame.stride[idx] = align(frame.width<<1, alignment);
            plane_size = frame.stride[idx] * frame.height;
            break;

        case PlaneType::ARGB_PLANE:
        case PlaneType::BGRA_PLANE:
        case PlaneType::RGBA_PLANE:
        case PlaneType::ABGR_PLANE:
        case PlaneType::AYUV_PLANE:
            frame.stride[idx] = align(frame.width<<2, alignment);
            plane_size = frame.stride[idx] * frame.height;
            break;

        case PlaneType::RGB565_PLANE:
            frame.stride[idx] = align(frame.width*3, alignment);
            plane_size = frame.stride[idx] * frame.height;
            break;

        case PlaneType::RAW_PLANE:
            frame.stride[idx] = align(frame.width, alignment);
            plane_size = frame.stride[idx] * frame.height;
            break;

        default:
            LOG_ERROR("%s unkonwn planar type %d\n", __FUNCTION__, (int)type);
    }

    return plane_size;
}

static bool allocFrameDevMemory(cncodecFrame& frame, u32_t alignment) {
    frame.planeNum = get_plane_num(frame.pixelFmt);
    auto planars = get_planars(frame.pixelFmt);
    if (planars  == nullptr)
    {
        return false;
    }

    for (u32_t i = 0; (i < frame.planeNum) && (planars[i] != PlaneType::INVALID); i++)
    {
        if (planars[i] == PlaneType::I420_PLANE_Y)
        {
            LOG_DEBUG("To satisfy scaler I420-Y plane special alignemnt request\n");
            frame.plane[i].size = calc_plane(frame, i, planars[i], 256);
        }
        else
        {
            frame.plane[i].size = calc_plane(frame, i, planars[i], alignment);
        }

        try
        {
             frame.plane[i].addr = CnCodecTestDevMemOp::alloc(frame.plane[i].size);
             LOG_TRACE("alloc buffer [%u], size(%u), pa 0x%lx Done\n",
                        i, frame.plane[i].size, frame.plane[i].addr);
        }
        catch (const std::bad_alloc&)
        {
            for (u32_t k = 0; k < i; k++)
            {
                CnCodecTestDevMemOp::free(frame.plane[k].addr);
                frame.plane[k].addr = 0;
            }

            LOG_ERROR("%s, alloc buffer [%u], size(%u) failed, throw bad_alloc\n",
                __FUNCTION__, i, frame.plane[i].size);
            throw;
        }
    }
    return true;
}

JpegDecodeContext::JpegDecodeContext(int instance_id):
    frame_buf_mgr_(pix_fmt, color_space, width, height,
                   kJpuDecodeFrameBufAlignment,
                   get_memory_channel(decode_instance_id),
                   device_id,
                   kDecodeOutputFrameBufferNum)

{
    decode_instance_id = (cnjpegDecInstance)instance_id;
    input_filename.push_back("1920x1080.jpg");
    std::for_each(std::begin(input_filename), std::end(input_filename), [&] (const string filename) {
            uint32_t file_size = get_filesize(filename);
            if (file_size > 0) {
                input_strm_reader_.emplace_back(
                    new VideoStreamReader(filename, FileReadMode::DEC_CHUNK, file_size, file_size));
                StreamData data;
                input_strm_reader_[input_strm_reader_.size() - 1]->getStreamData(data);
                streamDatas.push_back(data);

            }
        });
}

bool JpegDecodeContext::Decode(const StreamData &data, uint32_t input_strm_idx) {
    if (false) {
        cnjpegDecImageInfo pic_info;
        auto ret = cnjpegDecGetImageInfo(decoder_, &pic_info,
                                         (void*)data.start_addr,
                                         (u32_t)(data.end_addr_plus1 - data.start_addr));
        if (ret != CNCODEC_SUCCESS) {
            LOG_ERROR("cnjpegDecGetImageInfo fail, ret(%d)\n", ret);
            return false;
        }
        LOG_INFO("chan[%d], parse Header, strm buf va %p, size %u, res(%d*%d), marker_num %d\n",
                 decode_channel_id,
                 data.start_addr,
                 (u32_t)(data.end_addr_plus1 - data.start_addr),
                 pic_info.width, pic_info.height, pic_info.markNum);
    }

    cnjpegDecInput input;
    memset(&input, 0, sizeof(input));
    input.streamBuffer = (u8_t*)(data.start_addr);
    input.streamLength = (u32_t)(data.end_addr_plus1 - data.start_addr);
    input.flags        = CNJPEGDEC_FLAG_TIMESTAMP;
    input.privData     = (u64_t)input_strm_idx;
    input.pts          = 1000;

    cnjpegDecOutput output;
    /*
    unique_ptr<cncodecFrame> frame_(unique_ptr<cncodecFrame>{new cncodecFrame});
    memset(frame_.get(), 0, sizeof(*frame_));
    frame_->pixelFmt   = pix_fmt;
    frame_->colorSpace = color_space;
    frame_->width      = width;
    frame_->height     = height;
    frame_->channel    = get_memory_channel(decode_instance_id);
    frame_->deviceId   = device_id;
    allocFrameDevMemory(*frame_, kJpuDecodeFrameBufAlignment);
    */
    //memcpy(&output.frame, frame_.get(), sizeof(output.frame));
    memcpy(&output.frame, frame_buf_mgr_.frame(0), sizeof(output.frame));
    auto ret = cnjpegDecSyncDecode(decoder_, &output, &input, 1000);
    
    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("cnjpegDecSyncDecode fail, ret(%d)\n", ret);
        return false;
    }
    return true;
    LOG_INFO("pixelFmt %d\n", output.frame.pixelFmt);  // CNCODEC_PIX_FMT_NV12
    LOG_INFO("colorSpace %d\n", output.frame.colorSpace);  // CNCODEC_COLOR_SPACE_BT_709
    LOG_INFO("w %d h %d planeNum %d\n", output.frame.width, output.frame.height, output.frame.planeNum);
    char* image_data = (char *)malloc((output.frame.plane[0].size + output.frame.plane[1].size) * sizeof(char));
    if(GetFrameData(output.frame, image_data)){
        cv::Mat img = cv::Mat(output.frame.height * 3 / 2, output.frame.width, CV_8UC1, image_data);
        cv::Mat bgr(output.frame.height, output.frame.width, CV_8UC3);
        cv::cvtColor(img, bgr, cv::COLOR_YUV2BGR_NV12);
        cv::imwrite("output.jpg", bgr);
    } else {
        LOG_ERROR("GetFrameData fail");
        return false;
    }
    return true;
}

bool JpegDecodeContext::GetFrameData(const cncodecFrame& frame, char *image_data) {
    for (uint32_t i = 0; i < frame.planeNum; i++) {
        uint32_t height = 0;
        uint32_t width = 0;
        if (!CnCodecTestFrameBufWrapper::get_plane(width, height, frame, i)) {
            LOG_ERROR("%s, get plane failed\n", __FUNCTION__);
            return false;
        }

        uint32_t plane_size = height * width;
        //LOG_INFO("dump plane[%u], stride %u, w %u, h %u, plane_size(%u), buf size (%u)\n",
        //         i, frame.stride[i], width, height, plane_size, frame.plane[i].size);
        assert(frame.plane[i].size >= plane_size);
        auto va = CnCodecTestDevMemOp::map(frame.plane[i].addr, frame.plane[i].size);
        CnCodecTestDevMemOp::invalidateCache(va, frame.plane[i].size);
        if (frame.stride[i] == width) {
            if(i == 0){
                memcpy(image_data, (void*)va, plane_size);
            } else {
                memcpy(image_data + frame.plane[0].size, (void*)va, plane_size);
            }
            CnCodecTestDevMemOp::unmap(va, frame.plane[i].size);
        } else {
            LOG_ERROR("%s bad stride (%u) < width(%u)\n", __FUNCTION__, frame.stride[i], width);
            CnCodecTestDevMemOp::unmap(va, frame.plane[i].size);
            return false;
        }
    }
    return true;
}

void JpegDecodeContext::RunOneDecodeRound(void) {
    uint32_t input_strm_idx = 0;
    std::for_each(std::begin(streamDatas), std::end(streamDatas), [&] (StreamData data) {
                      if (!Decode(data, input_strm_idx)) {
                          LOG_ERROR("Jpeg Decode failed, input_strm(%d)\n",
                                    input_strm_idx);
                      }
                      input_strm_idx++;
        });

}

void JpegDecodeContext::BuildDecodeCreateInfo(cnjpegDecCreateInfo &info, void* usr_ctx) {
    memset(&info, 0, sizeof(cnjpegDecCreateInfo));
    info.deviceId              = device_id;
    info.instance              = decode_instance_id;
    info.pixelFmt              = pix_fmt;
    info.colorSpace            = color_space;
    info.width                 = width;
    info.height                = height;
    info.enablePreparse        = false;
    info.userContext           = usr_ctx;
    info.inputBufNum      = kDecodeInputBufferBufferNum;
    info.outputBufNum     = kDecodeOutputFrameBufferNum;
    info.suggestedLibAllocBitStrmBufSize = 0;
    info.allocType = CNCODEC_BUF_ALLOC_LIB;
    return;

    LOG_INFO("ch%d jpeg dec, res(w%d h%d, pixfmt (%s), color(%s) "
              "bufnum (in(%d) out(%d), mjpeg preparse(%d), buf alloc(%s)\n",
              decode_channel_id,
              (int)info.width, (int)info.height,
              PixelFormat2String(info.pixelFmt).c_str(),
              ColorSpace2String(info.colorSpace).c_str(),
              (int)info.inputBufNum,
              (int)info.outputBufNum,
              info.enablePreparse,
              (info.allocType == CNCODEC_BUF_ALLOC_LIB)? "lib":"app");
}

bool JpegDecodeContext::Create(void) {
    cnjpegDecCreateInfo create_info;
    BuildDecodeCreateInfo(create_info, (void*)this);
    auto ret = cnjpegDecCreate(&decoder_, CNJPEGDEC_RUN_MODE_SYNC, nullptr, &create_info);
    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("%s, cnjpegDecCreate sync decode fail, ret(%d)\n", __func__, ret);
        return false;
    }
    return true;
}

void JpegDecodeContext::Destroy(void) {
    auto ret = cnjpegDecDestroy(decoder_);
    if (ret != CNCODEC_SUCCESS) {
        LOG_ERROR("%s, cnjpegDecDestroy fail, ret(%d)\n", __func__, ret);
    }
    //LOG_INFO("chan[%02d] jpu[%d] res(%u x %u)\n", decode_channel_id, decode_instance_id, width, height);
}

JpegDecodeContext::~JpegDecodeContext(void) {}