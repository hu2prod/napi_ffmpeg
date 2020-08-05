# napi_ffmpeg
NOTE. Actually this is not ffmpeg, but libav which is almost equal ffmpeg \
NOTE encoding video is not implemented yet\
decode video example

    // NOTE. all frames is in mcs (microseconds)
    
    mod = require("napi_ffmpeg")
    // different videos should decode with different contexts (if in parallel)
    var ctx = mod.ffmpeg_decode_ctx_init();
    
    // returns si
    vat ret = mod.ffmpeg_decode_start(ctx, "test/"+file)
    console.log(ret);
    var [size_x, size_y, px_size] = ret;
    
    // static alloc
    buf = Buffer.alloc(px_size*size_x*size_y); 
    
    // will seek nearest keyframe earlier than this moment in mcs
    // return value keyframe mcs
    ret = mod.ffmpeg_decode_seek(ctx, 5339000)
    console.log("ffmpeg_decode_seek", ret);
    
    // buffer is result
    // ret is frame mcs
    ret = mod.ffmpeg_decode_frame(ctx, buf)
    fs.writeFileSync("look.raw", buf);
    console.log("ffmpeg_decode_frame", ret);
