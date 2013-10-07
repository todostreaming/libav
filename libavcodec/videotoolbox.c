/*
 * VideoToolbox hardware acceleration
 *
 * copyright (c) 2012 Sebastien Zwickert
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "videotoolbox.h"
#include "libavutil/avutil.h"
#include "bytestream.h"
#include "h264.h"
#include "mpegvideo.h"

static void videotoolbox_write_mp4_descr_length(PutByteContext *pb, int length)
{
    int i;
    uint8_t b;

    for (i = 3; i >= 0; i--) {
        b = (length >> (i * 7)) & 0x7F;
        if (i != 0)
            b |= 0x80;

        bytestream2_put_byteu(pb, b);
    }
}

static CFDataRef videotoolbox_esds_extradata_create(AVCodecContext *avctx)
{
    CFDataRef data;
    uint8_t *rw_extradata;
    PutByteContext pb;
    int full_size = 3 + 5 + 13 + 5 + avctx->extradata_size + 3;
    // ES_DescrTag data + DecoderConfigDescrTag + data + DecSpecificInfoTag + size + SLConfigDescriptor
    int config_size = 13 + 5 + avctx->extradata_size;
    int padding = 12;
    int s;

    if (!(rw_extradata = av_mallocz(full_size + padding)))
        return NULL;

    bytestream2_init_writer(&pb, rw_extradata, full_size + padding);
    bytestream2_put_byteu(&pb, 0);        // version
    bytestream2_put_ne24(&pb, 0);         // flags

    // elementary stream descriptor
    bytestream2_put_byteu(&pb, 0x03);     // ES_DescrTag
    videotoolbox_write_mp4_descr_length(&pb, full_size);
    bytestream2_put_ne16(&pb, 0);         // esid
    bytestream2_put_byteu(&pb, 0);        // stream priority (0-32)

    // decoder configuration descriptor
    bytestream2_put_byteu(&pb, 0x04);     // DecoderConfigDescrTag
    videotoolbox_write_mp4_descr_length(&pb, config_size);
    bytestream2_put_byteu(&pb, 32);       // object type indication. 32 = CODEC_ID_MPEG4
    bytestream2_put_byteu(&pb, 0x11);     // stream type
    bytestream2_put_ne24(&pb, 0);         // buffer size
    bytestream2_put_ne32(&pb, 0);         // max bitrate
    bytestream2_put_ne32(&pb, 0);         // avg bitrate

    // decoder specific descriptor
    bytestream2_put_byteu(&pb, 0x05);     ///< DecSpecificInfoTag
    videotoolbox_write_mp4_descr_length(&pb, avctx->extradata_size);

    bytestream2_put_buffer(&pb, avctx->extradata, avctx->extradata_size);

    // SLConfigDescriptor
    bytestream2_put_byteu(&pb, 0x06);     // SLConfigDescrTag
    bytestream2_put_byteu(&pb, 0x01);     // length
    bytestream2_put_byteu(&pb, 0x02);     //

    s = (int)(pb.buffer_end - pb.buffer_start);

    data = CFDataCreate(kCFAllocatorDefault, rw_extradata, s);

    av_freep(&rw_extradata);
    return data;
}

static CFDataRef videotoolbox_avcc_extradata_create(AVCodecContext *avctx)
{
    CFDataRef data;
    /* Each VCL NAL in the bistream sent to the decoder
     * is preceded by a 4 bytes length header.
     * Change the avcC atom header if needed, to signal headers of 4 bytes. */
    if (avctx->extradata_size >= 4 && (avctx->extradata[4] & 0x03) != 0x03) {
        uint8_t *rw_extradata;

        if (!(rw_extradata = av_malloc(avctx->extradata_size)))
            return NULL;

        memcpy(rw_extradata, avctx->extradata, avctx->extradata_size);

        rw_extradata[4] |= 0x03;

        data = CFDataCreate(kCFAllocatorDefault, rw_extradata, avctx->extradata_size);

        av_freep(&rw_extradata);
    } else {
        data = CFDataCreate(kCFAllocatorDefault, avctx->extradata, avctx->extradata_size);
    }

    return data;
}

static CMSampleBufferRef videotoolbox_sample_buffer_create(CMFormatDescriptionRef fmt_desc,
                                                           void *buffer,
                                                           int size)
{
    OSStatus status;
    CMBlockBufferRef  block_buf;
    CMSampleBufferRef sample_buf;

    block_buf  = NULL;
    sample_buf = NULL;

    status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,// structureAllocator
                                                buffer,             // memoryBlock
                                                size,               // blockLength
                                                kCFAllocatorNull,   // blockAllocator
                                                NULL,               // customBlockSource
                                                0,                  // offsetToData
                                                size,               // dataLength
                                                0,                  // flags
                                                &block_buf);

    if (!status) {
        status = CMSampleBufferCreate(kCFAllocatorDefault,  // allocator
                                      block_buf,            // dataBuffer
                                      TRUE,                 // dataReady
                                      0,                    // makeDataReadyCallback
                                      0,                    // makeDataReadyRefcon
                                      fmt_desc,             // formatDescription
                                      1,                    // numSamples
                                      0,                    // numSampleTimingEntries
                                      NULL,                 // sampleTimingArray
                                      0,                    // numSampleSizeEntries
                                      NULL,                 // sampleSizeArray
                                      &sample_buf);
    }

    if (block_buf)
        CFRelease(block_buf);

    return sample_buf;
}

static void videotoolbox_bitstream_release(VideotoolboxContext *vt_ctx)
{
    av_freep(&vt_ctx->priv_bitstream);
    vt_ctx->priv_bitstream_size = 0;
}

static void videotoolbox_decoder_callback(void *vt_hw_ctx,
                                          void *sourceFrameRefCon,
                                          OSStatus status,
                                          VTDecodeInfoFlags flags,
                                          CVImageBufferRef image_buffer,
                                          CMTime pts,
                                          CMTime duration)
{
    VideotoolboxContext *vt_ctx = vt_hw_ctx;
    vt_ctx->cv_buffer = NULL;

    if (!image_buffer) {
        av_log(NULL, AV_LOG_DEBUG, "vt decoder cb: output image buffer is null\n");
        return;
    }

    if (vt_ctx->cv_pix_fmt != CVPixelBufferGetPixelFormatType(image_buffer)) {
        av_log(NULL, AV_LOG_DEBUG, "vt decoder cb: output pixel format error\n");
        return;
    }

    vt_ctx->cv_buffer = CVPixelBufferRetain(image_buffer);
}

static OSStatus videotoolbox_session_decode_frame(VideotoolboxContext *vt_ctx)
{
    OSStatus status;
    CMSampleBufferRef sample_buf;

    sample_buf = videotoolbox_sample_buffer_create(vt_ctx->cm_fmt_desc,
                                                   vt_ctx->priv_bitstream,
                                                   vt_ctx->priv_bitstream_size);

    if (!sample_buf)
        return -1;

    status = VTDecompressionSessionDecodeFrame(vt_ctx->session,
                                               sample_buf,
                                               0,               // decodeFlags
                                               NULL,            // sourceFrameRefCon
                                               0);              // infoFlagsOut
    if (status == noErr)
        status = VTDecompressionSessionWaitForAsynchronousFrames(vt_ctx->session);

    CFRelease(sample_buf);

    return status;
}

static int videotoolbox_buffer_copy(VideotoolboxContext *vt_ctx,
                                    const uint8_t *buffer,
                                    uint32_t size)
{
    av_fast_malloc(&vt_ctx->priv_bitstream,
                   &vt_ctx->priv_allocated_size,
                   size);

    if (!vt_ctx->priv_bitstream)
        return AVERROR(ENOMEM);

    memcpy(vt_ctx->priv_bitstream, buffer, size);

    vt_ctx->priv_bitstream_size = size;

    return 0;
}

static int videotoolbox_h264_start_frame(AVCodecContext *avctx,
                                         av_unused const uint8_t *buffer,
                                         av_unused uint32_t size)
{
    VideotoolboxContext *vt_ctx = avctx->hwaccel_context;

    if (!vt_ctx->session)
        return -1;

    vt_ctx->priv_bitstream_size = 0;

    return 0;
}

static int videotoolbox_h264_decode_slice(AVCodecContext *avctx,
                                          const uint8_t *buffer,
                                          uint32_t size)
{
    VideotoolboxContext *vt_ctx = avctx->hwaccel_context;
    void *tmp;

    if (!vt_ctx->session)
        return -1;

    tmp = av_fast_realloc(vt_ctx->priv_bitstream,
                          &vt_ctx->priv_allocated_size,
                          vt_ctx->priv_bitstream_size+size+4);
    if (!tmp)
        return AVERROR(ENOMEM);

    vt_ctx->priv_bitstream = tmp;

    AV_WB32(vt_ctx->priv_bitstream + vt_ctx->priv_bitstream_size, size);
    memcpy(vt_ctx->priv_bitstream + vt_ctx->priv_bitstream_size + 4, buffer, size);

    vt_ctx->priv_bitstream_size += size + 4;

    return 0;
}

static int videotoolbox_mpeg_start_frame(AVCodecContext *avctx,
                                         const uint8_t *buffer,
                                         uint32_t size)
{
    VideotoolboxContext *vt_ctx = avctx->hwaccel_context;

    if (!vt_ctx->session)
        return -1;

    return videotoolbox_buffer_copy(vt_ctx, buffer, size);
}

static int videotoolbox_mpeg_decode_slice(AVCodecContext *avctx,
                                          const uint8_t *buffer,
                                          uint32_t size)
{
    VideotoolboxContext *vt_ctx = avctx->hwaccel_context;

    if (!vt_ctx->session)
        return -1;

    return 0;
}

static void videotoolbox_render(VideotoolboxContext *vt, AVFrame *f)
{
    int i, j;

    CVPixelBufferLockBaseAddress(vt->cv_buffer, 0);

    for (i = 0; i < 4 && f->linesize[i]; i++) {
        uint8_t *dst = f->data[i];
        uint8_t *src = CVPixelBufferGetBaseAddressOfPlane(vt->cv_buffer, i);
        int linesize = CVPixelBufferGetBytesPerRowOfPlane(vt->cv_buffer, i);

        for (j = 0; j < f->height; i++) {
            memcpy(dst, src, f->width);
            dst += f->linesize[i];
            src += linesize;
        }
    }

    CVPixelBufferUnlockBaseAddress(vt->cv_buffer, 0);
}

static int videotoolbox_end_frame(AVCodecContext *avctx)
{
    int status;
    VideotoolboxContext *vt = avctx->hwaccel_context;
    MpegEncContext *s = avctx->priv_data;
    AVFrame *frame = &s->current_picture_ptr->f;

    if (!vt->session || !vt->priv_bitstream)
        return -1;

    status = videotoolbox_session_decode_frame(vt);

    if (status) {
        av_log(avctx, AV_LOG_ERROR, "Failed to decode frame (%d)\n", status);
        return status;
    }

    videotoolbox_render(vt, frame);

    return status;
}

static CFDictionaryRef videotoolbox_decoder_config_create(CMVideoCodecType codec_type,
                                                          AVCodecContext *avctx)
{
    CFMutableDictionaryRef config_info = NULL;
    if (avctx->extradata_size) {
        CFMutableDictionaryRef avc_info;
        CFDataRef data;

        config_info = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                1,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
        avc_info = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             1,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);

        switch (codec_type) {
        case kCMVideoCodecType_MPEG4Video :
            data = videotoolbox_esds_extradata_create(avctx);
            if (data)
                CFDictionarySetValue(avc_info, CFSTR("esds"), data);
            break;
        case kCMVideoCodecType_H264 :
            data = videotoolbox_avcc_extradata_create(avctx);
            if (data)
                CFDictionarySetValue(avc_info, CFSTR("avcC"), data);
            break;
        default:
            break;
        }

        CFDictionarySetValue(config_info,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             avc_info);

        if (data)
            CFRelease(data);
        CFRelease(avc_info);
    }
    return config_info;
}

static CFDictionaryRef videotoolbox_buffer_attributes_create(int width,
                                                             int height,
                                                             OSType pix_fmt)
{
    CFMutableDictionaryRef buffer_attributes;
    CFMutableDictionaryRef io_surface_properties;
    CFNumberRef cv_pix_fmt;
    CFNumberRef w;
    CFNumberRef h;

    w = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &width);
    h = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &height);
    cv_pix_fmt = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pix_fmt);

    buffer_attributes = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                  4,
                                                  &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks);
    io_surface_properties = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                      0,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferPixelFormatTypeKey,
                         cv_pix_fmt);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferIOSurfacePropertiesKey,
                         io_surface_properties);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferWidthKey,
                         w);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferHeightKey,
                         h);

    CFRelease(io_surface_properties);
    CFRelease(cv_pix_fmt);
    CFRelease(w);
    CFRelease(h);

    return buffer_attributes;
}

static CMVideoFormatDescriptionRef videotoolbox_format_desc_create(CMVideoCodecType codec_type,
                                                                   CFDictionaryRef decoder_spec,
                                                                   int width,
                                                                   int height)
{
    CMFormatDescriptionRef cm_fmt_desc;
    OSStatus status;

    status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                            codec_type,
                                            width,
                                            height,
                                            decoder_spec,
                                            // Dictionary of extension
                                            &cm_fmt_desc);

    if (status)
        return NULL;

    return cm_fmt_desc;
}



// Return 0 on success < 0 on failure.

static int videotoolbox_session_create(AVCodecContext *avctx, OSType pix_fmt)
{
    OSStatus status;
    VTDecompressionOutputCallbackRecord decoder_cb;
    VideotoolboxContext *vt_ctx = avctx->hwaccel_context;
    CFDictionaryRef decoder_spec;
    CFDictionaryRef buf_attr;

    int width  = avctx->width;
    int height = avctx->height;

    switch (avctx->codec_id) {
    case AV_CODEC_ID_H264 :
        vt_ctx->cm_codec_type = kCMVideoCodecType_H264;
        break;
    case AV_CODEC_ID_MPEG2VIDEO :
        vt_ctx->cm_codec_type = kCMVideoCodecType_MPEG2Video;
        break;
    case AV_CODEC_ID_MPEG4 :
    case AV_CODEC_ID_H263 :
        vt_ctx->cm_codec_type = kCMVideoCodecType_MPEG4Video;
        break;
    default :
        break;
    }

    vt_ctx->cv_pix_fmt = pix_fmt;

    decoder_spec = videotoolbox_decoder_config_create(vt_ctx->cm_codec_type,
                                                      avctx);
    vt_ctx->cm_fmt_desc =
        videotoolbox_format_desc_create(vt_ctx->cm_codec_type, decoder_spec,
                                        width, height);

    if (!vt_ctx->cm_fmt_desc) {
        if (decoder_spec)
            CFRelease(decoder_spec);

        av_log(avctx, AV_LOG_ERROR, "format description creation failed\n");
        return -1;
    }

    buf_attr = videotoolbox_buffer_attributes_create(width,
                                                     height,
                                                     vt_ctx->cv_pix_fmt);

    decoder_cb.decompressionOutputCallback = videotoolbox_decoder_callback;
    decoder_cb.decompressionOutputRefCon   = vt_ctx;

    status = VTDecompressionSessionCreate(NULL,                // allocator
                                          vt_ctx->cm_fmt_desc, // videoFormatDescription
                                          decoder_spec,        // videoDecoderSpecification
                                          buf_attr,            // destinationImageBufferAttributes
                                          &decoder_cb,         // outputCallback
                                          &vt_ctx->session);   // decompressionSessionOut

    if (decoder_spec)
        CFRelease(decoder_spec);
    if (buf_attr)
        CFRelease(buf_attr);

    return status;
}

static int videotoolbox_init(AVCodecContext *avctx)
{
    VideotoolboxContext *vt = av_malloc(sizeof(*vt)); //FIXME move to the open

    if (!vt)
        return AVERROR(ENOMEM);

    avctx->hwaccel_context = vt;
    avctx->pix_fmt = AV_PIX_FMT_YUYV422;

    return videotoolbox_session_create(avctx, kCVPixelFormatType_422YpCbCr8);

}

static void videotoolbox_close(AVCodecContext *avctx)
{
    VideotoolboxContext *vt_ctx = avctx->hwaccel_context;

    if (!vt_ctx)
        return;

    if (vt_ctx->cm_fmt_desc)
        CFRelease(vt_ctx->cm_fmt_desc);

    if (vt_ctx->session)
        VTDecompressionSessionInvalidate(vt_ctx->session);

    videotoolbox_bitstream_release(vt_ctx);
}

#if CONFIG_H264_VIDEOTOOLBOX_HWACCEL
AVHWAccel ff_h264_videotoolbox_hwaccel = {
    .name           = "h264_videotoolbox",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_H264,
    .pix_fmt        = AV_PIX_FMT_VIDEOTOOLBOX,
    .init           = videotoolbox_init,
    .close          = videotoolbox_close,
    .start_frame    = videotoolbox_h264_start_frame,
    .decode_slice   = videotoolbox_h264_decode_slice,
    .end_frame      = videotoolbox_end_frame,
};
#endif

#if CONFIG_MPEG2_VIDEOTOOLBOX_HWACCEL
AVHWAccel ff_mpeg2_videotoolbox_hwaccel = {
    .name           = "mpeg2_videotoolbox",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MPEG2VIDEO,
    .pix_fmt        = AV_PIX_FMT_VIDEOTOOLBOX,
    .init           = videotoolbox_init,
    .close          = videotoolbox_close,
    .start_frame    = videotoolbox_mpeg_start_frame,
    .decode_slice   = videotoolbox_mpeg_decode_slice,
    .end_frame      = videotoolbox_end_frame,
};
#endif

#if CONFIG_MPEG4_VIDEOTOOLBOX_HWACCEL
AVHWAccel ff_mpeg4_videotoolbox_hwaccel = {
    .name           = "mpeg4_videotoolbox",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MPEG4,
    .pix_fmt        = AV_PIX_FMT_VIDEOTOOLBOX,
    .init           = videotoolbox_init,
    .close          = videotoolbox_close,
    .start_frame    = videotoolbox_mpeg_start_frame,
    .decode_slice   = videotoolbox_mpeg_decode_slice,
    .end_frame      = videotoolbox_end_frame,
};
#endif

#if CONFIG_H263_VIDEOTOOLBOX_HWACCEL
AVHWAccel ff_h263_videotoolbox_hwaccel = {
    .name           = "h263_videotoolbox",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_H263,
    .pix_fmt        = AV_PIX_FMT_VIDEOTOOLBOX,
    .init           = videotoolbox_init,
    .close          = videotoolbox_close,
    .start_frame    = videotoolbox_mpeg_start_frame,
    .decode_slice   = videotoolbox_mpeg_decode_slice,
    .end_frame      = videotoolbox_end_frame,
};
#endif
