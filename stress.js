#!/usr/bin/env node
mod = require('./build/Release/module');
fs = require("fs")

var file_list = fs.readdirSync("test");

var ctx_list = [];
file = file_list[0]

var ctx = mod.decode_ctx_init();
ctx_list.push(ctx);

var buf;
// for(var i=0;i<10000;i++) {
for(var i=0;i<1;i++) {
  // console.log(i);
  ret = mod.decode_start(ctx, "test/"+file)
  // console.log(ret);
  var [size_x, size_y, px_size] = ret;
  
  if (!buf)
    buf = Buffer.alloc(px_size*size_x*size_y);
  for(var j=0;j<10000;j++) {
    ret = mod.decode_frame(ctx, buf)
    
    ret = mod.decode_seek(ctx, 5339000)
  }
  mod.decode_finish(ctx)
}