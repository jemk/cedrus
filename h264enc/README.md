Proof of Concept hardware accelerated H264 encoder for sunxi
============================================================

This is a basic example for using the H264 hardware encoder for sunxi SoCs.

*It is just a proof of concept and not recommended for production use!*

Limitations:
------------

- no B frames
- constant QP
- only raw nv12 input and bytestream output
- ... many more

The old mplayer doesn't seem to like the raw bytestream output, maybe our
bytestream is still a little bit erroneous.


Usage:
------

    make

    ffmpeg -i <inputfile> -pix_fmt nv12 \
       -vf pad="width=trunc((iw+15)/16)*16:height=trunc((ih+15)/16)*16" \
       -f rawvideo pipe: | ./h264enc - <width> <height> <outputfile>

It is *important* that the input data is in nv12 format and has a width and
height multiple of 16, this is ensured by the "-vf pad" above.

For example:

    ffmpeg -i bigbuckbunny.mpg -pix_fmt nv12 \
       -vf pad="width=trunc((iw+15)/16)*16:height=trunc((ih+15)/16)*16" \
       -f rawvideo pipe: | ./h264enc - 854 480 bigbuckbunny.264

    ffplay bigbuckbunny.264
