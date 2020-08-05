#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <node_api.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include "time.c"
#include "runtime_native.c"
#include "array_size_t.c"
#include "hash.c"

// AVFormatContext *pFormatCtx = NULL;
// int             videoStream;
// AVCodecContext  *pCodecCtx = NULL;
// AVCodec         *pCodec = NULL;
// AVFrame         *pFrame = NULL; 
// AVFrame         *pFrameRGB = NULL;
// int             frameFinished;
// uint8_t         *buffer = NULL;
// 
// struct SwsContext      *sws_ctx = NULL;
// 
// AVRational ctx->time_base;
// 
// bool end_of_stream;
// bool is_video_open = false;

////////////////////////////////////////////////////////////////////////////////////////////////////
//    context manipulation
////////////////////////////////////////////////////////////////////////////////////////////////////
struct Decode_context {
  i32             ctx_idx;
  AVFormatContext *pFormatCtx;
  int             videoStream;
  AVCodecContext  *pCodecCtx;
  AVCodec         *pCodec;
  AVFrame         *pFrame; 
  AVFrame         *pFrameRGB;
  int             frameFinished;
  uint8_t         *buffer;
  
  struct SwsContext *sws_ctx;
  
  AVRational      time_base;
  
  bool end_of_stream;
  bool is_video_open;
  
  bool frame_captured;
};

// struct Decode_context* free_context_fifo = NULL;
// struct Decode_context** alloc_context_hash = NULL;
void* free_context_fifo = NULL;
void** alloc_context_hash = NULL;
int free_context_counter = 0;

void free_decode_context_fifo_ensure_init() {
  if (free_context_fifo == NULL) {
    free_context_fifo = array_size_t_alloc(16);
    alloc_context_hash = hash_size_t_alloc();
  }
}

// non thread-safe
struct Decode_context* decode_context_by_id(int id) {
  return (struct Decode_context*)hash_size_t_get(alloc_context_hash, id);
}

// non thread-safe
napi_value decode_ctx_init(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  struct Decode_context* ctx;
  int ctx_idx;
  free_decode_context_fifo_ensure_init();
  if (array_size_t_length_get(free_context_fifo)) {
    ctx = (struct Decode_context*)array_size_t_pop(free_context_fifo);
    ctx_idx = ctx->ctx_idx;
  } else {
    // alloc
    ctx = (struct Decode_context*)malloc(sizeof(struct Decode_context));
    ctx_idx = free_context_counter++;
    ctx->ctx_idx = ctx_idx;
    alloc_context_hash = hash_size_t_set(alloc_context_hash, ctx_idx, (size_t)ctx);
  }
  
  // clear
  ctx->pFormatCtx = NULL;
  ctx->pCodecCtx  = NULL;
  ctx->pCodec     = NULL;
  ctx->pFrame     = NULL; 
  ctx->pFrameRGB  = NULL;
  ctx->buffer     = NULL;
  
  ctx->sws_ctx    = NULL;
  
  ctx->is_video_open = false;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_idx;
  status = napi_create_int32(env, ctx_idx, &ret_idx);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_idx");
    return ret_dummy;
  }
  
  return ret_idx;
}

// non thread-safe
napi_value decode_ctx_free(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 id;
  status = napi_get_value_int32(env, argv[0], &id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of id");
    return ret_dummy;
  }
  
  struct Decode_context* ctx = decode_context_by_id(id);
  if (!ctx) {
    napi_throw_error(env, NULL, "Invalid id");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  free_context_fifo = array_size_t_push(free_context_fifo, (size_t)ctx);
  
  return ret_dummy;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//    decode
////////////////////////////////////////////////////////////////////////////////////////////////////

void frame_decode(struct Decode_context* ctx) {
  // do not free this packet ever
  AVPacket packet;
  while(av_read_frame(ctx->pFormatCtx, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == ctx->videoStream) {
      // Decode video frame
      avcodec_decode_video2(ctx->pCodecCtx, ctx->pFrame, &ctx->frameFinished, &packet);
      if (ctx->frameFinished) {
        return;
      }
    }
  }
  ctx->end_of_stream = true;
}

napi_value decode_start(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 2;
  napi_value argv[2];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 id;
  status = napi_get_value_int32(env, argv[0], &id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of id");
    return ret_dummy;
  }
  
  struct Decode_context* ctx = decode_context_by_id(id);
  if (!ctx) {
    napi_throw_error(env, NULL, "Invalid id");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  size_t file_path_len;
  status = napi_get_value_string_utf8(env, argv[1], NULL, 0, &file_path_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Can't get file_path_len");
    return ret_dummy;
  }
  
  size_t tmp;
  char *file_path = malloc(file_path_len);
  status = napi_get_value_string_utf8(env, argv[1], file_path, file_path_len+1, &tmp);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Can't get file_path");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  ctx->end_of_stream = false;
  ctx->frameFinished = false;
  if (avformat_open_input(&ctx->pFormatCtx, file_path, NULL, NULL) != 0) {
    napi_throw_error(env, NULL, "avformat_open_input error");
    return ret_dummy;
  }
  
  if (avformat_find_stream_info(ctx->pFormatCtx, NULL) < 0) {
    napi_throw_error(env, NULL, "avformat_find_stream_info error");
    return ret_dummy;
  }
  
  // Dump information about file onto standard error
  // av_dump_format(ctx->pFormatCtx, 0, file_path, 0);
  // Find the first video stream
  ctx->videoStream = -1;
  for(u32 i = 0; i < ctx->pFormatCtx->nb_streams; i++) {
    if (ctx->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      ctx->videoStream = i;
      break;
    }
  }
  if (ctx->videoStream == -1) {
    napi_throw_error(env, NULL, "Didn't find a video stream");
    return ret_dummy;
  }
  
  // Get a pointer to the codec context for the video stream
  ctx->pCodecCtx = ctx->pFormatCtx->streams[ctx->videoStream]->codec;
  
  // Find the decoder for the video stream
  ctx->pCodec = avcodec_find_decoder(ctx->pCodecCtx->codec_id);
  if (ctx->pCodec == NULL) {
    napi_throw_error(env, NULL, "Unsupported codec!");
    return ret_dummy;
  }
  
  AVDictionary    *optionsDict = NULL;
  // Open codec
  if (avcodec_open2(ctx->pCodecCtx, ctx->pCodec, &optionsDict) < 0) {
    napi_throw_error(env, NULL, "avcodec_open2 error");
    return ret_dummy;
  }
  
  ctx->time_base = ctx->pFormatCtx->streams[ctx->videoStream]->time_base;
  
  ctx->pFrame = av_frame_alloc();
  ctx->pFrameRGB = av_frame_alloc();
  if (ctx->pFrameRGB == NULL) {
    napi_throw_error(env, NULL, "av_frame_alloc error");
    return ret_dummy;
  }
  
  int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, ctx->pCodecCtx->width, ctx->pCodecCtx->height);
  ctx->buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
  
  ctx->sws_ctx =
    sws_getContext
    (
        ctx->pCodecCtx->width,
        ctx->pCodecCtx->height,
        ctx->pCodecCtx->pix_fmt,
        ctx->pCodecCtx->width,
        ctx->pCodecCtx->height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );
  
  // Assign appropriate parts of buffer to image planes in pFrameRGB
  // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
  // of AVPicture
  avpicture_fill((AVPicture *)ctx->pFrameRGB, ctx->buffer, AV_PIX_FMT_RGB24, ctx->pCodecCtx->width, ctx->pCodecCtx->height);
  
  ctx->is_video_open = true;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  napi_value ret_arr;
  status = napi_create_array(env, &ret_arr);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
    return ret_dummy;
  }
  
  
  // width
  napi_value ret_width;
  status = napi_create_int32(env, ctx->pCodecCtx->width, &ret_width);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_width");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 0, ret_width);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_width assign");
    return ret_dummy;
  }
  // height
  napi_value ret_height;
  status = napi_create_int32(env, ctx->pCodecCtx->height, &ret_height);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_height");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 1, ret_height);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_height assign");
    return ret_dummy;
  }
  
  // byte size = 3
  napi_value ret_byte_size;
  status = napi_create_int32(env, 3, &ret_byte_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_byte_size");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 2, ret_byte_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_byte_size assign");
    return ret_dummy;
  }
  
  
  return ret_arr;
}

napi_value decode_finish(napi_env env, napi_callback_info info) {
  napi_status status;
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 id;
  status = napi_get_value_int32(env, argv[0], &id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of id");
    return ret_dummy;
  }
  
  struct Decode_context* ctx = decode_context_by_id(id);
  if (!ctx) {
    napi_throw_error(env, NULL, "Invalid id");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  if (!ctx->is_video_open) {
    napi_throw_error(env, NULL, "decode_finish !ctx->is_video_open");
    return ret_dummy;
  }
  // NOTE we NOT free packet with av_free_packet
  // av_free_packet(&ctx->packet);
  // Free the RGB image
  av_free(ctx->buffer);
  av_free(ctx->pFrameRGB);
  
  // Free the YUV frame
  av_free(ctx->pFrame);
  
  // Close the codec
  avcodec_close(ctx->pCodecCtx);
  
  // Close the video file
  avformat_close_input(&ctx->pFormatCtx);
  ctx->is_video_open = false;
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  return ret_dummy;
}

napi_value decode_seek(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 2;
  napi_value argv[2];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 id;
  status = napi_get_value_int32(env, argv[0], &id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of id");
    return ret_dummy;
  }
  
  struct Decode_context* ctx = decode_context_by_id(id);
  if (!ctx) {
    napi_throw_error(env, NULL, "Invalid id");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i64 ff_dst;
  status = napi_get_value_int64(env, argv[1], &ff_dst);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i64 was passed as argument of ff_dst");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  if (!ctx->is_video_open) {
    napi_throw_error(env, NULL, "decode_seek !ctx->is_video_open");
    return ret_dummy;
  }
  
  ctx->end_of_stream = false;
  if (!ctx->frameFinished) {
    frame_decode(ctx);
  }
  int64_t seek_target = av_rescale_q(ff_dst, AV_TIME_BASE_Q, ctx->time_base);
  if (av_seek_frame(ctx->pFormatCtx, ctx->videoStream, seek_target, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD) < 0) {
    napi_throw_error(env, NULL, "av_seek_frame error");
    return ret_dummy;
  }
  
  frame_decode(ctx);
  i64 t = av_frame_get_best_effort_timestamp(ctx->pFrame);
  t = av_rescale_q(t, ctx->time_base, AV_TIME_BASE_Q);
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_frame;
  status = napi_create_int64(env, t, &ret_frame);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_frame");
    return ret_dummy;
  }
  
  return ret_frame;
}

// = next + copy
napi_value decode_frame(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 2;
  napi_value argv[2];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 id;
  status = napi_get_value_int32(env, argv[0], &id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of id");
    return ret_dummy;
  }
  
  struct Decode_context* ctx = decode_context_by_id(id);
  if (!ctx) {
    napi_throw_error(env, NULL, "Invalid id");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  u8* data_dst;
  size_t data_dst_len;
  status = napi_get_buffer_info(env, argv[1], (void**)&data_dst, &data_dst_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  if (data_dst_len < (size_t)(3*ctx->pCodecCtx->width*ctx->pCodecCtx->height)) {
    napi_throw_error(env, NULL, "data_dst_len < (size_t)3*ctx->pCodecCtx->width*ctx->pCodecCtx->height");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // work
  if (!ctx->is_video_open) {
    napi_throw_error(env, NULL, "decode_frame !ctx->is_video_open");
    return ret_dummy;
  }
  
  i64 t = -1;
  ctx->frame_captured = false;
  frame_decode(ctx);
  
  if (ctx->frameFinished) {
    sws_scale(
      ctx->sws_ctx,
      (uint8_t const * const *)ctx->pFrame->data,
      ctx->pFrame->linesize,
      0,
      ctx->pCodecCtx->height,
      ctx->pFrameRGB->data,
      ctx->pFrameRGB->linesize
    );
    t = av_frame_get_best_effort_timestamp(ctx->pFrame);
    t = av_rescale_q(t, ctx->time_base, AV_TIME_BASE_Q);
    ctx->frame_captured = true;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_t;
  if (ctx->end_of_stream) {
    status = napi_create_int64(env, -2, &ret_t);
  } else if (!ctx->frame_captured) {
    status = napi_create_int64(env, -1, &ret_t);
  } else {
    // copy
    u8* dst = data_dst;
    u8* src = ctx->pFrameRGB->data[0];
    size_t copy_size= 3*ctx->pCodecCtx->width;
    size_t src_step = ctx->pFrameRGB->linesize[0];
    size_t dst_step = 3*ctx->pCodecCtx->width;
    int height = ctx->pCodecCtx->height;
    for(int y = 0; y < height; y++) {
      memcpy(dst, src, copy_size);
      src += src_step;
      dst += dst_step;
    }
    
    status = napi_create_int64(env, t, &ret_t);
  }
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_t napi_create_int64");
    return ret_dummy;
  }
  
  return ret_t;
}

// separated API
napi_value decode_frame_next(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 id;
  status = napi_get_value_int32(env, argv[0], &id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of id");
    return ret_dummy;
  }
  
  struct Decode_context* ctx = decode_context_by_id(id);
  if (!ctx) {
    napi_throw_error(env, NULL, "Invalid id");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // work
  if (!ctx->is_video_open) {
    napi_throw_error(env, NULL, "decode_frame_next !ctx->is_video_open");
    return ret_dummy;
  }
  
  i64 t = -1;
  ctx->frame_captured = false;
  frame_decode(ctx);
  
  if (ctx->frameFinished) {
    // potential boost
    // sws_scale(
    //   ctx->sws_ctx,
    //   (uint8_t const * const *)ctx->pFrame->data,
    //   ctx->pFrame->linesize,
    //   0,
    //   ctx->pCodecCtx->height,
    //   ctx->pFrameRGB->data,
    //   ctx->pFrameRGB->linesize
    // );
    t = av_frame_get_best_effort_timestamp(ctx->pFrame);
    t = av_rescale_q(t, ctx->time_base, AV_TIME_BASE_Q);
    ctx->frame_captured = true;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_t;
  if (ctx->end_of_stream) {
    status = napi_create_int64(env, -2, &ret_t);
  } else if (!ctx->frame_captured) {
    status = napi_create_int64(env, -1, &ret_t);
  } else {
    // NO copy
    status = napi_create_int64(env, t, &ret_t);
  }
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_t napi_create_int64");
    return ret_dummy;
  }
  
  return ret_t;
}

napi_value decode_frame_copy(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 2;
  napi_value argv[2];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  i32 id;
  status = napi_get_value_int32(env, argv[0], &id);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of id");
    return ret_dummy;
  }
  
  struct Decode_context* ctx = decode_context_by_id(id);
  if (!ctx) {
    napi_throw_error(env, NULL, "Invalid id");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  u8* data_dst;
  size_t data_dst_len;
  status = napi_get_buffer_info(env, argv[1], (void**)&data_dst, &data_dst_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  if (data_dst_len < (size_t)3*ctx->pCodecCtx->width*ctx->pCodecCtx->height) {
    napi_throw_error(env, NULL, "data_dst_len < (size_t)3*ctx->pCodecCtx->width*ctx->pCodecCtx->height");
    return ret_dummy;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // work
  if (!ctx->is_video_open) {
    napi_throw_error(env, NULL, "decode_frame_copy !ctx->is_video_open");
    return ret_dummy;
  }
  
  if (ctx->frame_captured) {
    sws_scale(
      ctx->sws_ctx,
      (uint8_t const * const *)ctx->pFrame->data,
      ctx->pFrame->linesize,
      0,
      ctx->pCodecCtx->height,
      ctx->pFrameRGB->data,
      ctx->pFrameRGB->linesize
    );
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_t;
  if (ctx->end_of_stream) {
    status = napi_create_int64(env, -2, &ret_t);
  } else if (!ctx->frame_captured) {
    status = napi_create_int64(env, -1, &ret_t);
  } else {
    // copy
    u8* dst = data_dst;
    u8* src = ctx->pFrameRGB->data[0];
    size_t copy_size= 3*ctx->pCodecCtx->width;
    size_t src_step = ctx->pFrameRGB->linesize[0];
    size_t dst_step = 3*ctx->pCodecCtx->width;
    int height = ctx->pCodecCtx->height;
    for(int y = 0; y < height; y++) {
      memcpy(dst, src, copy_size);
      src += src_step;
      dst += dst_step;
    }
    
    status = napi_create_int64(env, 0, &ret_t);
  }
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_t napi_create_int64");
    return ret_dummy;
  }
  
  return ret_t;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;
  
  __alloc_init();
  av_register_all();
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_ctx_init, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_ctx_init", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_ctx_free, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_ctx_free", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_start, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_start", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_finish, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_finish", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_seek, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_seek", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_frame, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_frame", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_frame_next, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_frame_next", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, decode_frame_copy, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "decode_frame_copy", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
