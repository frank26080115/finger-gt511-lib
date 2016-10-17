This is a library to use the GT-511C1R fingerprint scanner module in an embedded application. Written entirely in C and made portable to all platforms.

It should also work with the GT-511C3 without changes.

It can still be used for Arduino, but I mainly wrote it to work with all microcontrollers supported by GCC. It requires a few HAL wrapper functions (serial port stuff and a system timer for timeouts) to be written by the user before it can work.

Documentation is in the comments, function call descriptions with parameters and returns, etc.

I started with the library provided by SparkFun, https://github.com/sparkfun/Fingerprint_Scanner-TTL , and I almost barfed when I saw how it was written. So I decided to rewrite my own.

License: see LICENSE file on the root of the repo, GPL v3

Copyright (C) 2016 Frank Zhao