Proof of Concept hardware accelerated H264 encoder for sunxi
============================================================

This is a first demo of the hardware encoding capabilities of sunxi SoCs.

*It is just a proof of concept and not intended for production use!*

Limitations:
------------

- only I Frames
- fixed QP (=30)
- only raw input and bytestream output
- ... many more

mplayer doesn't like the output, maybe there is still some error.
Please use ffplay to test.

Usage:
------

    make

    ffmpeg -i <inputfile> -vf pad="trunc((iw+15)/16)*16" -pix_fmt nv12 \
       -f rawvideo pipe: | ./h264enc - <width> <height> <outputfile>

It is *imortant* that the input data is in nv12 format and has a width multiple
of 16, this is ensured by the "-vf pad" above.

For example:

    ffmpeg -i bigbuckbunny.mpg -vf pad="trunc((iw+15)/16)*16" -pix_fmt nv12 \
       -f rawvideo pipe: | ./h264enc - 854 480 bigbuckbunny.264
    
    ffplay bigbuckbunny.264
