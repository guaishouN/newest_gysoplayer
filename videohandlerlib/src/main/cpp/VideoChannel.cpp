//
// Created by GuaishouN on 2020/4/25.
//
#include "VideoChannel.h"

/**
 * 丢packet包
 */
void dropAVPacket(queue<AVPacket *> &q) {
    while (!q.empty()) {
        AVPacket *packet = q.front();
        //AV_PKT_FLAG_KEY 关键帧
        if (packet->flags != AV_PKT_FLAG_KEY) {
            BaseChannel::releaseAVPacket(&packet);
            q.pop();
        } else {
            break;
        }
    }
}

/**
 * 丢frame包
 */
void dropAVFrame(queue<AVFrame *> &q) {
    if (!q.front()) {
        AVFrame *frame = q.front();
        BaseChannel::releaseAVFrame(&frame);
        q.pop();
    }
}

VideoChannel::VideoChannel(int stream_index, AVCodecContext *pContext, AVRational time_base,
                           int fps) :
        BaseChannel(stream_index, pContext, time_base) {
    this->fps = fps;
    packets.setSyncCallback(dropAVPacket);
    frames.setSyncCallback(dropAVFrame);
}

VideoChannel::~VideoChannel() {

}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void *threadVideoDecode(void *agrs) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(agrs);
    videoChannel->video_decode();
    return 0;
}

void *threadVideoPlay(void *agrs) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(agrs);
    videoChannel->video_play();
    return 0;
}

void VideoChannel::start() {
    isPlaying = 1;
    packets.setWork(1);
    frames.setWork(1);
    //解码线程和播放线程
    pthread_create(&pid_video_decode, 0, threadVideoDecode, this);
    pthread_create(&pid_video_play, 0, threadVideoPlay, this);
}

void VideoChannel::stop() {
    isPlaying = 0;
    packets.setWork(0);
    frames.setWork(0);
    pthread_join(pid_video_decode, 0);
    pthread_join(pid_video_play, 0);
}

void VideoChannel::video_decode() {
    AVPacket *packet = 0;
    while (isPlaying) {
        /**
         * 队列控制
         */
        if (isPlaying && frames.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        int ret = packets.pop(packet);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        ret = avcodec_send_packet(pContext, packet);
        if (ret) {
            break;
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(pContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret != 0) {
            releaseAVFrame(&frame);
            break;
        }
        frames.push(frame);
        if (packet) {
            releaseAVPacket(&packet);
        }
    }
    releaseAVPacket(&packet);
}

void save2file(AVFrame *frame) {
    FILE *file = fopen("/data/user/0/com.gyso.ndklearnapplication/files/output.yuv", "wb");
    if (file) {
        for (int i = 0; i < frame->height; i++) {
            fwrite(frame->data[0] + i * frame->linesize[0], 1, frame->width, file);
        }
        for (int i = 0; i < frame->height / 2; i++) {
            fwrite(frame->data[1] + i * frame->linesize[1], 1, frame->width / 2, file);
        }
        for (int i = 0; i < frame->height / 2; i++) {
            fwrite(frame->data[2] + i * frame->linesize[2], 1, frame->width / 2, file);
        }
        fclose(file);
    }
}

int count = 1;

void save2file(uint8_t *nv21_data_buffer, size_t size) {
    FILE *file = fopen("/data/user/0/com.gyso.ndklearnapplication/files/out.yuv", "wb");
    if (!file) {
        LOGE("Failed to open file");
        return;
    }

    // 写入 NV21 数据到文件
    size_t written = fwrite(nv21_data_buffer, 1, size, file);
    if (written != size) {
        LOGD("Failed to write all data to file. Written: %zu, Expected: %zu\n", written, size);
    }
    fclose(file);
}



void yuv420p_to_nv21(AVFrame *frame, uint8_t *nv21_data) {
    int width = frame->width;
    int height = frame->height;

    // 1. 复制 Y 平面
    memcpy(nv21_data, frame->data[0], width * height);

    // 2. 交替复制 UV 平面 (NV21 格式是交替存储 V 和 U)
    uint8_t *u_plane = frame->data[1];  // U 平面起始位置
    uint8_t *v_plane = frame->data[2];  // V 平面起始位置
    int uv_stride = frame->linesize[1]; // U 和 V 平面 stride

    // NV21 的 UV 平面起始位置

    uint8_t *nv21_uv = nv21_data + width * height;

    // 交替存储 V 和 U 数据
    for (int i = 0; i < height / 2; i++) {
        for (int j = 0; j < width / 2; j++) {
            // NV21 中 V 在前，U 在后
            *nv21_uv++ = v_plane[i * uv_stride + j];
            *nv21_uv++ = u_plane[i * uv_stride + j];
        }
    }
}
int frameNum=0;
void VideoChannel::video_play() {
    //播放
    uint8_t *dst_data[4];
    int dst_linesize[4];
    AVFrame *frame = 0;
    SwsContext *swsContext = sws_getContext(
            pContext->width,
            pContext->height,
            pContext->pix_fmt,
            pContext->width,
            pContext->height,
            AV_PIX_FMT_RGBA,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
    );
    //给显示缓存申请内存
    av_image_alloc(
            dst_data,
            dst_linesize,
            pContext->width,
            pContext->height,
            AV_PIX_FMT_RGBA,
            1
    );
    uint8_t *nv21_data_buffer = nullptr;
    int ow = -1;
    int oh = -1;
    size_t nv21_buffer_size = -1;
    int ret = 0;
    while (isPlaying) {
        while(frames.size()>0){
            ret = frames.pop(frame);
        }
        if(!ret){
            continue;
        }
        if (!frame) {
            continue;
        }
        if (nv21_data_buffer == nullptr or ow != frame->width or oh != frame->height) {
            if(nv21_data_buffer){
                free(nv21_data_buffer);
            }
            nv21_buffer_size = frame->width * frame->height * 3 / 2;
            nv21_data_buffer = (uint8_t *) malloc(nv21_buffer_size);
            ow = frame->width;
            oh = frame->height;
            LOGE("video size change %dx%d ",ow,oh)
        }
//        //yuv ->rgba
//        sws_scale(
//                swsContext,
//                frame->data,
//                frame->linesize,
//                0,
//                pContext->height,
//                dst_data,
//                dst_linesize
//        );
        /**
         * 音视频同步 单位微秒us
         */

//      //宽高数据
//        save2file(frame);
        yuv420p_to_nv21(frame, nv21_data_buffer);
//        save2file(nv21_data_buffer, nv21_buffer_size);
        renderCallback(
                dst_data[0],
                ow,
                oh,
                dst_linesize[0],
                nv21_data_buffer,
                nv21_buffer_size
        );
        releaseAVFrame(&frame);
    }
    free(nv21_data_buffer);
    releaseAVFrame(&frame);
    isPlaying = 0;
    av_freep(&dst_data[0]);
    sws_freeContext(swsContext);
}