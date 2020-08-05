# napi_video
NOTE encoding video is not implemented yet\
decode video example

    // NOTE. all frames is in mcs (microseconds)
    
    mod = require("napi_video")
    // different videos should decode with different contexts (if in parallel)
    var ctx = mod.decode_ctx_init();
    
    // returns si
    vat ret = mod.decode_start(ctx, "test/"+file)
    console.log(ret);
    var [size_x, size_y, px_size] = ret;
    
    // static alloc
    buf = Buffer.alloc(px_size*size_x*size_y); 
    
    // will seek nearest keyframe earlier than this moment in mcs
    // return value keyframe mcs
    ret = mod.decode_seek(ctx, 5339000)
    console.log("decode_seek", ret);
    
    // buffer is result
    // ret is frame mcs
    ret = mod.decode_frame(ctx, buf)
    fs.writeFileSync("look.raw", buf);
    console.log("decode_frame", ret);
