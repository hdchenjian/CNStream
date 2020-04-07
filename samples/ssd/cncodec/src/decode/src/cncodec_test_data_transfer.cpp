#include "cncodec_test_data_transfer.h"
#include "cncodec_test_common.h"
#include <assert.h>
#include <cstring>
#include <utility>

using namespace std;

bool DataTransferManager::validateShadowBuffer(uint32_t size)
{
    if (size <= shadow_buf_size_)
    {
        return true;
    }

    shadow_buf_ = std::move(std::unique_ptr<char[]>(new (std::nothrow) char[size]));
    if (shadow_buf_ == nullptr)
    {
       shadow_buf_size_ = 0;
       return false;
    }

    shadow_buf_size_ = size;
    return true;
}

bool DataTransferManager::SaveFrame2File(ofstream& dst_out_strm, const cncodecFrame& frame)
{
    if (!dst_out_strm.good()) {
        return true;
    }

    for (uint32_t i = 0; i < frame.planeNum; i++) {
        uint32_t height = 0;
        uint32_t width = 0;

        if (!CnCodecTestFrameBufWrapper::get_plane(width, height, frame, i)) {
            LOG_ERROR("%s, get plane failed\n", __FUNCTION__);
            return false;
        }

        uint32_t plane_size = height * width;

        LOG_INFO("dump plane[%u], stride %u, w %u, h %u, plane_size(%u), buf size (%u)\n",
                 i, frame.stride[i], width, height, plane_size, frame.plane[i].size);

        assert(frame.plane[i].size >= plane_size);

        auto va = CnCodecTestDevMemOp::map(frame.plane[i].addr, frame.plane[i].size);
        CnCodecTestDevMemOp::invalidateCache(va, frame.plane[i].size);

        if (frame.stride[i] == width) {
            dst_out_strm.write((const char*)va, plane_size);
        } else if (frame.stride[i] > width) {
            if (!validateShadowBuffer(frame.plane[i].size)) {
                LOG_ERROR("%s, validateShadowBuffer failed\n", __FUNCTION__);
                return false;
            }

            char* dst_buf = shadow_buf_.get();
            for (uint32_t step = 0, k = 0; k < height; k++) {
                memcpy(dst_buf, (const char*)va + step, width);

                step    += frame.stride[i];
                dst_buf += width;
            }
            dst_out_strm.write((const char*)shadow_buf_.get(), plane_size);
        }
        else {
            LOG_ERROR("%s bad stride (%u) < width(%u)\n", __FUNCTION__, frame.stride[i], width);
        }

        CnCodecTestDevMemOp::unmap(va, frame.plane[i].size);
    }

    return true;
}

bool DataTransferManager::copyPlanar2StrideAlignedShadowBuffer(const char* &dst_buf_addr,
                                                               const char* src_buf_addr,
                                                               uint32_t width,
                                                               uint32_t height,
                                                               uint32_t stride)
{
    if (width >= stride)
    {
        dst_buf_addr = src_buf_addr;
        return true;
    }
    else if (!validateShadowBuffer(stride * height))
    {
        LOG_ERROR("%s validateShadowBuffer failed\n", __FUNCTION__);
        return false;
    }

    char* dst = shadow_buf_.get();
    const char* src = src_buf_addr;

    assert(src_buf_addr != nullptr);

    memset(dst, 0, (stride * height));
    for (uint32_t i = 0; i < height; i++)
    {
        memcpy(dst, src, width);
        src += width;
        dst += stride;
    }

    dst_buf_addr = shadow_buf_.get();
    return true;
}

bool DataTransferManager::LoadFrame2Encode(cncodecFrame &frame, const char* data)
{
    CNCODEC_TEST_CHECK_NULL_PTR_RET(data, false);

    for (uint32_t i = 0; i < frame.planeNum; i++)
    {
        const char* frame_data_start = data;

        uint32_t height = 0;
        uint32_t width = 0;

        if (!CnCodecTestFrameBufWrapper::get_plane(width, height, frame, i))
        {
            LOG_ERROR("%s, get plane failed\n", __FUNCTION__);
            return false;
        }
        else if (width != frame.stride[i])
        {
            copyPlanar2StrideAlignedShadowBuffer(frame_data_start,
                                                 data,
                                                 width,
                                                 height,
                                                 frame.stride[i]);
             LOG_VERBOSE("stride align case: stride%d, w%d, h%d, srcBuf %p, alignedBuf %p\n",
                         frame.stride[i], width,  height, frame_data_start, data);
        }
        assert(frame_data_start != nullptr);

        CnCodecTestDevMemOp::memcpyH2D(frame.plane[i].addr, (void*)frame_data_start, height * frame.stride[i]);
        data += height * width;
    }
    return true;
}

bool DataTransferManager::SaveStream2File(ofstream &dst_out_strm,
                                          const cncodecDevMemory& dev_mem,
                                          uint32_t bit_strm_size,
                                          uint32_t bit_strm_offset)
{
    if (dev_mem.size < (bit_strm_size + bit_strm_offset))
    {
        LOG_ERROR("mem size %u, not enough(strm size %u, ofst %u)\n",
        dev_mem.size, bit_strm_size, bit_strm_offset);
        return false;
    }
    else if (dst_out_strm.good())
    {
        auto va = CnCodecTestDevMemOp::map(dev_mem.addr, dev_mem.size);
        CNCODEC_TEST_CHECK_NULL_PTR_RET(va, false);
        CnCodecTestDevMemOp::invalidateCache(va, dev_mem.size);

        dst_out_strm.write((char *)((u64_t)va+bit_strm_offset), bit_strm_size);

        CnCodecTestDevMemOp::unmap(va, dev_mem.size);
    }

    return true;
}
