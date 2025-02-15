# "Lossy" QOI compressor - The “Quite OK Image Format” lossy image compression

Single-file MIT licensed library for C/C++

Original repository https://github.com/phoboslab/qoi

## Example Usage

- [qoiconv_cpr.c](https://github.com/Raven1996/qoi-compressor/blob/master/qoiconv_cpr.c)
converts between png/jpeg <> (lossy) qoi

The default setting is suitable for most pictures. Use `-mul` when you care less
about translucent quality. Raising `-hi` may tolerant some JPEG artifacts. Try 
raising `-lo` when image has base noise.

## Limitations

The QOI file format allows for huge images with up to 18 exa-pixels. A streaming
en-/decoder can handle these with minimal RAM requirements, assuming there is 
enough storage space.

This particular implementation of QOI however is limited to images with a 
maximum size of 400 million pixels. It will safely refuse to en-/decode anything
larger than that. This is not a streaming en-/decoder. It loads the whole image 
file into RAM before doing any work and is not extensively optimized for 
performance (but it's still very fast).

The "lossy" QOI compressor does not intend to compress at a high speed. Please 
use with care.