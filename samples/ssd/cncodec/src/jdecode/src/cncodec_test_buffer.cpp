
#include "cncodec_test_buffer.h"
#include "cncodec_test_common.h"
#include "cnrt.h"
#include "cn_codec_memory.h"

#include <cstdlib>
#include <cstring>
#include <utility>
#include <cassert>
#include <algorithm>

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

bool CnCodecTestFrameBufWrapper::get_plane(u32_t& width,
                                           u32_t& height,
                                           const cncodecFrame& frame,
                                           u32_t plane_id) noexcept
{
    auto planars = get_planars(frame.pixelFmt);

    if (plane_id >= CnCodecTestFrameBufWrapper::kMaxPlaneNumber)
    {
        LOG_ERROR("%s overflow(%d)\n", __FUNCTION__, (int)plane_id);
        return false;
    }
    else if ((planars == nullptr) || (planars[plane_id] == PlaneType::INVALID))
    {
        LOG_ERROR("%s planars not found for pixfmt(%d)\n", __FUNCTION__, (int)frame.pixelFmt);
        return false;
    }

    switch (planars[plane_id])
    {
        case PlaneType::I420_PLANE_Y:
        case PlaneType::YV12_PLANE_Y:
        case PlaneType::NV12_PLANE_Y:
        case PlaneType::NV21_PLANE_Y:
            width  = frame.width;
            height = frame.height;
            break;

        case PlaneType::NV12_PLANE_UV:
        case PlaneType::NV21_PLANE_VU:
            width  = frame.width;
            height = frame.height>>1;
            break;

        case PlaneType::I420_PLANE_U:
        case PlaneType::YV12_PLANE_V:
        case PlaneType::I420_PLANE_V:
        case PlaneType::YV12_PLANE_U:
            width  = frame.width>>1;
            height = frame.height>>1;
            break;

        case PlaneType::P010_PLANE_Y:
        case PlaneType::YUV420_10BIT_PLANE_Y:
            width  = frame.width<<1;
            height = frame.height;
            break;

        case PlaneType::P010_PLANE_UV:
            width  = frame.width<<1;
            height = frame.height>>1;
            break;

        case PlaneType::YUV420_10BIT_PLANE_U:
        case PlaneType::YUV420_10BIT_PLANE_V:
            width  = frame.width;
            height = frame.height>>1;
            break;

        case PlaneType::YUYV_PLANE:
        case PlaneType::UYVY_PLANE:
        case PlaneType::YVYU_PLANE:
        case PlaneType::VYUY_PLANE:
            width  = frame.width<<1;
            height = frame.height;
            break;

        case PlaneType::ARGB_PLANE:
        case PlaneType::BGRA_PLANE:
        case PlaneType::RGBA_PLANE:
        case PlaneType::ABGR_PLANE:
        case PlaneType::AYUV_PLANE:
            width  = frame.width<<2;
            height = frame.height;
            break;

        case PlaneType::RGB565_PLANE:
            width  = frame.width*3;
            height = frame.height;
            break;

        case PlaneType::RAW_PLANE:
            width  = frame.width;
            height = frame.height;
            break;

        default:
            LOG_ERROR("%s unkonwn planar type %d\n", __FUNCTION__, (int)planars[plane_id]);
            return false;
    }

    return true;
}

static bool allocFrameDevMemory(cncodecFrame& frame, u32_t alignment)
{
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

CnCodecTestFrameBufWrapper::CnCodecTestFrameBufWrapper(cncodecPixelFormat format,
                                                       cncodecColorSpace space,
                                                       u32_t width,
                                                       u32_t height,
                                                       u32_t alignment,
                                                       u32_t channel,
                                                       u32_t device_id) noexcept:
  alignment_(alignment),
  frame_(unique_ptr<cncodecFrame>{new cncodecFrame})
{
    memset(frame_.get(), 0, sizeof(*frame_));

    frame_->pixelFmt   = format;
    frame_->colorSpace = space;
    frame_->width      = width;
    frame_->height     = height;
    frame_->channel    = channel;
    frame_->deviceId   = device_id;
    allocFrameDevMemory(*frame_, alignment_);
}

CnCodecTestFrameBufWrapper::CnCodecTestFrameBufWrapper(const cncodecFrame &frame) noexcept:
  is_external_buf_(true),
  frame_(unique_ptr<cncodecFrame>{new cncodecFrame})
{
    memcpy(frame_.get(), &frame, sizeof(frame));
}

CnCodecTestFrameBufWrapper::~CnCodecTestFrameBufWrapper()
{
    if (!is_external_buf_)
    {
        for (u32_t i = 0; i < frame_->planeNum; i++)
        {
            CnCodecTestDevMemOp::free(frame_->plane[i].addr);
        }
    }
}

cncodecFrame* CnCodecTestFrameBufMgr::frame(u32_t frame_idx) const noexcept
{
    if (frame_idx < frames_.size())
    {
        return frames_[frame_idx]->get_frame();
    }
    return nullptr;
}

cncodecFrame* CnCodecTestFrameBufMgr::get_all_frames(void) noexcept
{
    if (frames_.size() <= 0)
    {
        return nullptr;
    }
    else
    {
        frames_mirror_ = std::unique_ptr<cncodecFrame[]> {new cncodecFrame[frames_.size()]};
        for (u32_t i = 0; i < frames_.size(); i++)
        {
            memcpy(&frames_mirror_[i], frame(i), sizeof(cncodecFrame));
        }
    }

    return frames_mirror_.get();
}

CnCodecTestFrameBufMgr::CnCodecTestFrameBufMgr(cncodecPixelFormat format,
                                               cncodecColorSpace space,
                                               u32_t width,
                                               u32_t height,
                                               u32_t alignment,
                                               u32_t channel,
                                               u32_t device_id,
                                               u32_t count) noexcept
{
    frames_.reserve(count);

    for (u32_t i = 0; i < count; i++)
    {
        frames_.emplace_back(new CnCodecTestFrameBufWrapper(format, space, width,
            height, alignment, channel, device_id));
    }
}

CnCodecTestFrameBufMgr::CnCodecTestFrameBufMgr(const CnCodecTestFrameBufMgr& other) noexcept
{
    frames_.reserve(other.size());

    std::for_each(std::begin(other.frames_), std::end(other.frames_),
    [&](const unique_ptr<CnCodecTestFrameBufWrapper>& frame_wrap)
    {
        if ((frame_wrap != nullptr) && (frame_wrap->get_frame() != nullptr))
        {
            frames_.emplace_back(new CnCodecTestFrameBufWrapper(*(frame_wrap->get_frame())));
        }
    });
}

static bool allocBitStrmDevMemory(cncodecDevMemory &dev_mem,
                                  u32_t size,
                                  u32_t alignment,
                                  u32_t channel,
                                  u32_t device_id)
{
    cncodecDevMemory new_mem;

    new_mem.size = align(size, alignment);

    try
    {
        new_mem.addr = CnCodecTestDevMemOp::alloc(new_mem.size, channel, device_id);
    }
    catch (const std::bad_alloc&)
    {
       LOG_ERROR("%s, alloc buffer failed, throw bad_alloc\n", __FUNCTION__);
       throw;
    }

    if (dev_mem.addr != 0)
    {
        CnCodecTestDevMemOp::free(dev_mem.addr);
    }

    dev_mem = new_mem;
    return true;
}

cncodecDevMemory* CnCodecTestBitStreamBufMgr::dev_mem(u32_t idx) const noexcept
{
    if (idx < dev_mems_.size())
    {
        return dev_mems_[idx]->get_mem();
    }
    return nullptr;
}

cncodecDevMemory* CnCodecTestBitStreamBufMgr::get_all_strm_buffers(void) noexcept
{
    if (dev_mems_.size() <= 0)
    {
        return nullptr;
    }
    else
    {
        dev_mems_mirror_ = std::unique_ptr<cncodecDevMemory[]> {new cncodecDevMemory[dev_mems_.size()]};

        for (u32_t i = 0; i < dev_mems_.size(); i++)
        {
            auto src  = dev_mem(i);
            assert(src != nullptr);
            memcpy(&dev_mems_mirror_[i], src, sizeof(cncodecDevMemory));
        }
    }

    return dev_mems_mirror_.get();
}

CnCodecTestDevMemoryWrapper::CnCodecTestDevMemoryWrapper(u32_t size,
                                                         u32_t alignment,
                                                         u32_t channel,
                                                         u32_t device_id) noexcept:
  alignment_(alignment),
  device_id_(device_id),
  channel_(channel),
  memory_(unique_ptr<cncodecDevMemory> {new cncodecDevMemory})
{
    memset(memory_.get(), 0, sizeof(cncodecDevMemory));

    allocBitStrmDevMemory(*memory_, size, alignment_, channel_, device_id_);
}

CnCodecTestDevMemoryWrapper::~CnCodecTestDevMemoryWrapper()
{
    CnCodecTestDevMemOp::free(memory_->addr);
}

CnCodecTestBitStreamBufMgr::CnCodecTestBitStreamBufMgr(u32_t size,
                                                       u32_t alignment,
                                                       u32_t channel,
                                                       u32_t device_id,
                                                       u32_t count) noexcept
{
    dev_mems_.reserve(count);

    for (u32_t i = 0; i < count; ++i)
    {
        dev_mems_.emplace_back(new CnCodecTestDevMemoryWrapper(size, alignment, channel, device_id));
    }
}

void CnCodecTestDevMemOp::cnrt_init(u32_t device) noexcept
{
    cnrtDev_t cnrt_dev;
    u32_t count = 0;

    if (cnrtInit(0) != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("cnrtInit failed!\n");
    }
    else if (cnrtGetDeviceCount(&count) != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("cnrtGetDeviceCount failed!\n");
    }
    else if(cnrtGetDeviceHandle(&cnrt_dev, device) != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("cnrtGetDeviceHandle failed!\n");
    }
    else if(cnrtSetCurrentDevice(cnrt_dev) != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("cnrtSetCurrentDevice failed!\n");
    }
}

u64_t CnCodecTestDevMemOp::alloc(u32_t size, u32_t channel, u32_t device_id) noexcept
{
    u64_t dev_addr = 0;

    if (cnrtSetCurrentChannel((cnrtChannelType_t)channel) != CNRT_RET_SUCCESS)
    {
        LOG_ERROR("cnrtSetCurrentChannel failed!\n");
    }

    auto ret = cnrtMallocFrameBuffer((void **)(&dev_addr), size);
    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("alloc size%d) failed(%d)!\n", size, ret);
    }

    return dev_addr;
}

void  CnCodecTestDevMemOp::free(u64_t pa) noexcept
{
    auto ret = cnrtFree((void *)pa);

    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("free pa(0x%lx) failed, ret(%d)\n", pa, ret);
    }
}

void* CnCodecTestDevMemOp::map(u64_t pa, u32_t size) noexcept
{
    void *va = nullptr;
    auto ret = cncodecMap((void **)(&va), pa, size);

    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("map pa(0x%lx), size(%d), failed ret(%d)\n", pa, size, ret);
    }

    return va;
}

void CnCodecTestDevMemOp::unmap(void* va, u32_t size) noexcept
{
    auto ret = cncodecUnmap(va, size);
    if (ret!= CNRT_RET_SUCCESS)
    {
        LOG_FATAL("unmap va(%p), size(%u), failed, ret(%d)\n", va, size, ret);
    }
}

void CnCodecTestDevMemOp::invalidateCache(void* va, u32_t size) noexcept
{
    auto ret = cncodecInvalidateCache(va, size);
    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("invalidateCache va(%p), size(%d), failed, ret(%d)\n", va, size, ret);
    }
}

void CnCodecTestDevMemOp::flushCache(void* va, u32_t size) noexcept
{
    auto ret = cncodecFlushCache(va, size);
    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("flushCache va(%p), size(%d), failed, ret(%d)\n", va, size, ret);
    }
}

bool CnCodecTestDevMemOp::memcpyH2D(u64_t dst, void* src, u32_t size) noexcept
{
    auto ret = cnrtMemcpy((void *)dst, src, size, CNRT_MEM_TRANS_DIR_HOST2DEV);
    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("memcpyH2D dsc(0x%lx), src(%p), size(%d), failed, ret(%d)\n", dst, src, size, ret);
        return false;
    }

    return true;
}

bool  CnCodecTestDevMemOp::memcpyD2H(void* dst, u64_t src, u32_t size) noexcept
{
    auto ret = cnrtMemcpy(dst, (void *)src, size, CNRT_MEM_TRANS_DIR_DEV2HOST);
    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("memcpyD2H dsc(0x%p), src(%lx), size(%d), failed, ret(%d)\n", dst, src, size, ret);
        return false;
    }
    return true;
}

bool  CnCodecTestDevMemOp::memcpyD2D(u64_t dst, u64_t src, u32_t size) noexcept
{
    auto ret = cnrtMemcpy((void *)dst, (void *)src, size, CNRT_MEM_TRANS_DIR_DEV2DEV);
    if (ret != CNRT_RET_SUCCESS)
    {
        LOG_FATAL("memcpyD2D dsc(0x%lx), src(%lx), size(%d), failed, ret(%d)\n", dst, src, size, ret);
        return false;
    }
    return true;
}