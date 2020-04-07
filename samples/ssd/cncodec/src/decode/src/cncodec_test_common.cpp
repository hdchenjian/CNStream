#include"cncodec_test_common.h"

std::string CodecType2String(cncodecType codec)
{
    switch (codec)
    {
        case CNCODEC_MPEG2: return "MPEG2";
        case CNCODEC_MPEG4: return "MPEG4";
        case CNCODEC_H264:  return "H264";
        case CNCODEC_HEVC:  return "H265";
        case CNCODEC_JPEG:  return "JPEG";
        case CNCODEC_VP8:   return "VP8";
        case CNCODEC_VP9:   return "VP9";
        case CNCODEC_AVS:   return "AVS";
        default: break;
    }

    return "Not support";
}

std::string PixelFormat2String(cncodecPixelFormat fmt)
{
    switch (fmt)
    {
        case CNCODEC_PIX_FMT_NV12: return "NV12";
        case CNCODEC_PIX_FMT_YV12: return "YV12";
        case CNCODEC_PIX_FMT_YUYV: return "YUYV";
        case CNCODEC_PIX_FMT_UYVY: return "UYVY";
        case CNCODEC_PIX_FMT_P010: return "P010";
        case CNCODEC_PIX_FMT_I420: return "I420";
        case CNCODEC_PIX_FMT_NV21: return "NV21";
        case CNCODEC_PIX_FMT_YUV420_10BIT: return "YUV420";
        case CNCODEC_PIX_FMT_YUV444_10BIT: return "YVU444";
        case CNCODEC_PIX_FMT_ARGB: return "ARGB";
        case CNCODEC_PIX_FMT_ABGR: return "ABGR";
        case CNCODEC_PIX_FMT_BGRA: return "BGRA";
        case CNCODEC_PIX_FMT_RGBA: return "RGBA";
        case CNCODEC_PIX_FMT_RAW:  return "RAW";
        default: break;
    }

    return "unkonwn pixel format";
}

std::string ColorSpace2String(cncodecColorSpace space)
{
    switch (space)
    {
        case CNCODEC_COLOR_SPACE_BT_709: return "BT709";
        case CNCODEC_COLOR_SPACE_BT_601: return "BT601";
        case CNCODEC_COLOR_SPACE_BT_2020: return "BT2020";
        case CNCODEC_COLOR_SPACE_BT_601_ER: return "BT601ER";
        case CNCODEC_COLOR_SPACE_BT_709_ER: return "BT709ER";
        default: break;
    }

    return "Invalid Color Space";
}

cncodecPixelFormat ParsePixelFormat(const std::string &surface_fmt_str)
{
    if (surface_fmt_str == "NV12")
    {
        return CNCODEC_PIX_FMT_NV12;
    }
    else if (surface_fmt_str == "YV12")
    {
        return CNCODEC_PIX_FMT_YV12;
    }
    else if (surface_fmt_str == "YUYV")
    {
        return CNCODEC_PIX_FMT_YUYV;
    }
    else if (surface_fmt_str == "UYVY")
    {
        return CNCODEC_PIX_FMT_UYVY;
    }
    else if (surface_fmt_str == "P010")
    {
        return CNCODEC_PIX_FMT_P010;
    }
    else if (surface_fmt_str == "I420")
    {
        return CNCODEC_PIX_FMT_I420;
    }
    else if (surface_fmt_str == "NV21")
    {
        return CNCODEC_PIX_FMT_NV21;
    }
    else if (surface_fmt_str == "YUV420_10BIT")
    {
        return CNCODEC_PIX_FMT_YUV420_10BIT;
    }
    else if (surface_fmt_str == "YUV444_10BIT")
    {
        return CNCODEC_PIX_FMT_YUV444_10BIT;
    }
    else if (surface_fmt_str == "ARGB")
    {
        return CNCODEC_PIX_FMT_ARGB;
    }
    else if (surface_fmt_str == "ABGR")
    {
        return CNCODEC_PIX_FMT_ABGR;
    }
    else if (surface_fmt_str == "RGBA")
    {
        return CNCODEC_PIX_FMT_RGBA;
    } else if (surface_fmt_str == "BGRA") {
        return CNCODEC_PIX_FMT_BGRA;
    }

    return CNCODEC_PIX_FMT_NV12;
}

cncodecType ParseCodecType(const std::string &codec_str)
{
    if ((codec_str == "MPEG2") || (codec_str == "MPEG"))
    {
        return CNCODEC_MPEG2;
    }
    else if ((codec_str == "MPEG4") || (codec_str == "MP4"))
    {
        return CNCODEC_MPEG2;
    }
    else if (codec_str == "H264")
    {
        return CNCODEC_H264;
    }
    else if ((codec_str == "H265") || (codec_str == "HEVC"))
    {
        return CNCODEC_HEVC;
    }
    else if (codec_str == "JPEG")
    {
        return CNCODEC_JPEG;
    }
    else if (codec_str == "VP8")
    {
        return CNCODEC_VP8;
    }
    else if (codec_str == "VP9")
    {
        return CNCODEC_VP9;
    }
    else if (codec_str == "AVS")
    {
        return CNCODEC_AVS;
    }

    return CNCODEC_TOTAL_COUNT;
}

int CalculateFramePicSize(int width, int height, cncodecPixelFormat pix_fmt)
{
    int size = width * height;

    switch (pix_fmt)
    {
        case CNCODEC_PIX_FMT_NV12:
        case CNCODEC_PIX_FMT_NV21:
        case CNCODEC_PIX_FMT_I420:
        case CNCODEC_PIX_FMT_YV12:
            size += (size >> 1);
            break;

        case CNCODEC_PIX_FMT_YUYV:
        case CNCODEC_PIX_FMT_UYVY:
            size = (size << 1);
            break;

        case CNCODEC_PIX_FMT_ARGB:
        case CNCODEC_PIX_FMT_RGBA:
        case CNCODEC_PIX_FMT_ABGR:
        case CNCODEC_PIX_FMT_BGRA:
            size = (size << 2);
            break;

        case CNCODEC_PIX_FMT_P010:
            size += (size << 1);
            break;

        default:
            LOG_ERROR("encode planar fmt(%d) not support\n", (int)pix_fmt);
            break;
    }

    return size;
}
