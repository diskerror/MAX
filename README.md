# Cycling74 Max Externals (v6.1/v7)

## cheb
An implementation of a Chebyshev recursive filter. It only generates a list of coefficients that can then be sent to an “iir~” object. The output can also be sent to a Max multi-slider to watch how the coefficients change with cutoff frequency, poles, and ripple settings. It can output a coefficient list in one of two orders, either "aabab…" or "aaa…bb…".

This is an implementation of the algorithm presented by [Stephen W. Smith in his book “The Scientist and Engineer's Guide to Digital Signal Processing” 2nd edition](http://www.dspguide.com).

## iir~
This will do a IIR or recursive convolution based on an input list of float or double precision coefficients. Coefficients can be in one of two orders, either "aabab…" or "aaa…bb…". Handles both 32- and 64-bit MSP streams.

This version includes my first attempt to remove the “zipper” effect. This has made algorithm more unstable at the extremes of frequency. Future versions will have a settable ramp time.

# XCode Project Setup
```
https://cycling74.com/forums/topic/writing-external-xcode-6-empty-project/
```
This also works with XCode 7.
