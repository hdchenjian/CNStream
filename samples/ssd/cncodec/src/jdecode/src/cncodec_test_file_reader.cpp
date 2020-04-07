#include <assert.h>
#include <utility>

#include "cncodec_test_common.h"
#include "cncodec_test_file_reader.h"

using namespace std;

std::string readMode2String(enum FileReadMode mode)
{
    switch (mode)
    {
        case FileReadMode::DEC_NALU:
            return "NALU";

        case FileReadMode::DEC_IVF:
            return "IVF";

        case FileReadMode::DEC_CHUNK:
            return "CHUNK";

        case FileReadMode::ENC_FRAME:
            return "YUV";

        default:
            break;
    }

    return "Unknown Stream Read mode";
}

enum FileReadMode string2ReadMode(const std::string &modeString)
{
    if (modeString == "NALU")
    {
        return FileReadMode::DEC_NALU;
    }
    else if (modeString =="IVF")
    {
        return FileReadMode::DEC_IVF;
    }
    else if (modeString == "CHUNK")
    {
        return FileReadMode::DEC_CHUNK;
    }
    else if (modeString == "YUV")
    {
        return FileReadMode::ENC_FRAME;
    }

    return FileReadMode::STRM_READ_MODE_MAX;
}

bool VideoStreamReader::readFrame(StreamData &data)
{
    uint32_t avail_size = strm_buf_wp_ - strm_buf_rp_;

    data.start_addr = strm_buf_rp_;
    data.eos = false;

    if (avail_size <= single_read_size_)
    {
        is_strm_buf_exhaust_ = true;
    }

    if (!is_strm_buf_exhaust_)
    {
        strm_buf_rp_ += single_read_size_;
        data.end_addr_plus1 = strm_buf_rp_;
    }
    else if (src_video_ifstrm_.eof())
    {
        data.end_addr_plus1 = strm_buf_wp_;
        data.eos = true;

        strm_buf_rp_ = strm_buf_wp_;
        is_eos_delivered_ = true;
    }
    else if (avail_size == single_read_size_)
    {
        strm_buf_rp_ += single_read_size_;
        data.end_addr_plus1 = strm_buf_rp_;
    }
    else
    {
        return false;
    }

    return true;
}

bool VideoStreamReader::readChunk(StreamData  &data)
{
    data.start_addr = strm_buf_rp_;
    data.end_addr_plus1 = strm_buf_wp_;
    data.eos = src_video_ifstrm_.eof();

    if (data.eos)
    {
        is_eos_delivered_ = true;
    }

    strm_buf_rp_ = strm_buf_wp_;
    is_strm_buf_exhaust_ = true;
    return true;
}

#define IS_NALU_START_CODE1(x) ((*(int*)(x) & 0x00ffffff) == 0x00010000)
#define IS_NALU_START_CODE(x)  (*(int*)(x) == 0x01000000)

static constexpr u32_t kNaluHeaderSize1{4U};
static constexpr u32_t kNaluHeaderSize2{3U};

enum class cncodecH264NaluType
{
    H264_NALU_TYPE_SLICE = 1,
    H264_NALU_TYPE_DPA   = 2,
    H264_NALU_TYPE_DPB,
    H264_NALU_TYPE_DPC,
    H264_NALU_TYPE_IDR,
    H264_NALU_TYPE_SEI,
    H264_NALU_TYPE_SPS,
    H264_NALU_TYPE_PPS,
    H264_NALU_TYPE_AUD,
    H264_NALU_TYPE_EOSEQ,
    H264_NALU_TYPE_EOSTREAM,
    H264_NALU_TYPE_FILL,
    H264_NALU_TYPE_MAX,
};

enum class cncodecH265NaluType
{
    H265_NALU_TYPE_VPS   = 32,
    H265_NALU_TYPE_SPS   = 33,
    H265_NALU_TYPE_PPS   = 34,
    H265_NALU_TYPE_AUD   = 35,
    H265_NALU_TYPE_EOSEQ = 36,
    H265_NALU_TYPE_MAX,
};

static inline bool IsAuxNaluType(u32_t codec, u8_t data) noexcept
{
    if (codec == CNCODEC_H264)
    {
        auto nalu_type = static_cast<cncodecH264NaluType>(data & 0x1f);

        if ((nalu_type == cncodecH264NaluType::H264_NALU_TYPE_SEI) ||
            (nalu_type == cncodecH264NaluType::H264_NALU_TYPE_SPS) ||
            (nalu_type == cncodecH264NaluType::H264_NALU_TYPE_PPS) ||
            (nalu_type == cncodecH264NaluType::H264_NALU_TYPE_AUD))
        {
            return true;
        }
    }
    else if (codec == CNCODEC_HEVC)
    {
        auto nalu_type = static_cast<cncodecH265NaluType>((data & 0x7E) >> 1);

        if ((nalu_type == cncodecH265NaluType::H265_NALU_TYPE_PPS) ||
            (nalu_type == cncodecH265NaluType::H265_NALU_TYPE_VPS) ||
            (nalu_type == cncodecH265NaluType::H265_NALU_TYPE_SPS) ||
            (nalu_type == cncodecH265NaluType::H265_NALU_TYPE_AUD))
        {
            return true;
        }
    }
    else
    {
        return false;
    }

    return false;
}

bool VideoStreamReader::readNalu(StreamData &data)
{
    const uint8_t * rp  = strm_buf_rp_;
    bool firstNaluFound = false;
    bool is_vcl_data    = false;

    assert(rp != nullptr);

    while ((rp + kNaluHeaderSize1) <= strm_buf_wp_)
    {
        if (IS_NALU_START_CODE(rp) || IS_NALU_START_CODE1(rp))
        {
            u32_t off = IS_NALU_START_CODE(rp) ? kNaluHeaderSize1 : kNaluHeaderSize2;

            if ((rp + off == strm_buf_wp_) ||
                ((rp + off < strm_buf_wp_) && !IsAuxNaluType(codec_, *(rp + off))))
            {
                is_vcl_data = true;
            }

            if (!firstNaluFound)
            {
                data.start_addr = rp;
                firstNaluFound  = true;

                if (IS_NALU_START_CODE(rp))
                {
                    rp += kNaluHeaderSize1;
                }
                else
                {
                    rp += kNaluHeaderSize2;
                }
            }
            else
            {
                if (is_vcl_data)
                {
                    data.end_addr_plus1 = rp;
                    strm_buf_rp_ = rp;
                    return true;
                }
                else
                {
                    rp++;
                }
            }
        }
        else
        {
            rp++;
        }
    }

    is_strm_buf_exhaust_ = true;

    return false;
}

bool VideoStreamReader::readIvf(StreamData &data)
{
    const uint8_t * rp = strm_buf_rp_;
    const int ivf_file_header_size = 32;
    const int ivf_frame_header_size = 12;

    assert(rp != nullptr);

    auto is_vp8_vp9_stream = [rp]() ->bool
    {
        return (rp[0] == 'D') && (rp[1] == 'K') && (rp[2] == 'I') && (rp[3] == 'F');
    };

    if (is_read_start_)
    {
        if ((rp + ivf_file_header_size) <= strm_buf_wp_)
        {
            if (is_vp8_vp9_stream())
            {
                is_read_start_ = false;
            }
            else
            {
                LOG_ERROR("not VP8/VP9 stream header <DKIF>\n");
                is_error_ivf_header = true;
                return false;
            }

            rp += ivf_file_header_size;
        }
        else
        {
            is_strm_buf_exhaust_ = true;
            return false;
        }
    }

    if ((rp + ivf_frame_header_size) <= strm_buf_wp_)
    {
        uint32_t buffer_size = (rp[3]<<24) + (rp[2]<<16) + (rp[1]<<8) + rp[0];
        rp += ivf_frame_header_size;
        data.start_addr = rp;

        if ((rp + buffer_size) <= strm_buf_wp_)
        {
            rp += buffer_size;
            data.end_addr_plus1 = rp;
            strm_buf_rp_ = rp;
            return true;
        }
    }

    is_strm_buf_exhaust_ = true;
    return false;
}

VideoStreamReader::VideoStreamReader(const std::string &input_filename,
                                     enum FileReadMode mode,
                                     uint32_t min_strm_buf_size,
                                     uint32_t tot_strm_buf_size,
                                     cncodecType codec):
   read_mode_(mode),
   single_read_size_(min_strm_buf_size),
   strm_buf_tot_size_(tot_strm_buf_size),
   strm_buf_(new(std::nothrow) uint8_t[strm_buf_tot_size_]),
   strm_buf_wp_(strm_buf_.get()),
   strm_buf_rp_(strm_buf_wp_),
   codec_(codec)
{
    LOG_TRACE("strmfilename %s, single_read_size%u strm_buf_tot_size %u  codec %s\n",
               input_filename.c_str(), single_read_size_, strm_buf_tot_size_, CodecType2String(codec_).c_str());

    assert(strm_buf_wp_ != nullptr);
    src_video_ifstrm_.open(input_filename, ios::binary);
    if (!src_video_ifstrm_.good())
    {
        LOG_ERROR("open file %s failed!!!!!!!!!\n", input_filename.c_str());
    }
}

VideoStreamReader::~VideoStreamReader()
{
   if (src_video_ifstrm_.is_open())
   {
       src_video_ifstrm_.close();
   }
}

void VideoStreamReader::sendEOS(StreamData & data)
{
    if (src_video_ifstrm_.eof() && (!is_eos_delivered_))
    {
       readChunk(data);
    }
}

void VideoStreamReader::refreshStreamBuffer(void)
{
    if ((!is_strm_buf_exhaust_) || src_video_ifstrm_.eof())
    {
        LOG_ERROR("eof or exhausted\n");
        return;
    }
    else if ((strm_buf_rp_ < strm_buf_wp_) && src_video_ifstrm_.good ())
    {
        long last_remain_size = strm_buf_wp_ - strm_buf_rp_;
        src_video_ifstrm_.seekg(-last_remain_size, ios::cur);
        LOG_DEBUG("seek size %ld\n", last_remain_size);
    }

    strm_buf_wp_ = strm_buf_.get();
    strm_buf_rp_ = strm_buf_wp_;

    if (src_video_ifstrm_.good())
    {
        src_video_ifstrm_.read(reinterpret_cast<char*>(strm_buf_wp_),
                               strm_buf_tot_size_);

        strm_buf_wp_  += src_video_ifstrm_.gcount();
        tryReadNextByteToCheckEosAlreadyReached();
        LOG_VERBOSE("file Read size %d, eof(%d)\n",
                    (int)(strm_buf_wp_-strm_buf_rp_), src_video_ifstrm_.eof());
    }

    if (strm_buf_wp_ > strm_buf_rp_)
    {
        is_strm_buf_exhaust_ = false;
    }
}

void VideoStreamReader::tryReadNextByteToCheckEosAlreadyReached (void)
{
    if (src_video_ifstrm_.good ())
    {
         char tmp[1];
         src_video_ifstrm_.read (tmp, 1);
         if (src_video_ifstrm_.good ())
         {
             src_video_ifstrm_.seekg (-1, ios::cur);
         }
    }
}

bool VideoStreamReader:: readData(StreamData & data)
{
    if (is_strm_buf_exhaust_)
    {
        return false;
    }

    data.eos = false;
    switch (read_mode_)
    {
        case FileReadMode::DEC_CHUNK:
            return readChunk(data);
        case FileReadMode::DEC_IVF:
            return readIvf(data);
        case FileReadMode::DEC_NALU:
            return readNalu(data);
        case FileReadMode::ENC_FRAME:
            return readFrame(data);
        default:
            LOG_ERROR("Un supported file read mode %s\n",
                readMode2String(read_mode_).c_str());
            break;
    }

    return false;
}

void VideoStreamReader::reset(void)
{
    if (src_video_ifstrm_.is_open() && src_video_ifstrm_.eof())
    {
        src_video_ifstrm_.clear();
        src_video_ifstrm_.seekg(0, ios::beg);
    }

    is_eos_delivered_    = false;
    strm_buf_wp_         = strm_buf_.get();
    strm_buf_rp_         = strm_buf_wp_;
    is_strm_buf_exhaust_ = true;
    is_read_start_       = true;
}

bool VideoStreamReader::getStreamData(StreamData &data)
{
    data.eos            = false;
    data.start_addr     = nullptr;
    data.end_addr_plus1 = nullptr;

    while (!is_eos_delivered_)
    {
        if (is_strm_buf_exhaust_)
        {
            if (src_video_ifstrm_.eof())
            {
                sendEOS(data);
                break;
            }
            else
            {
                refreshStreamBuffer();
            }
        }

        if(is_error_ivf_header)
        {
            return false;
        }

        if (!is_strm_buf_exhaust_ && readData(data))
        {
            break;
        }
    }
    return true;
}
