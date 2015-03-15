# status #

Inferno kernels can be [downloaded](http://code.google.com/p/inferno-ds/downloads/list) and booted.

The working devices that are used by limbo programs are:
  * input: buttons and touch screen
  * lcd screens: lower and upper (see [screen.c](http://code.google.com/p/inferno-ds/source/browse/trunk/screen.c))
  * sound: play .iaf with pcm encoded files (see [devaudio.c](http://code.google.com/p/inferno-ds/source/browse/trunk/devaudio.c))
  * storage: use devroot (consumes precious ram) or instead use sd card (see [devdldi.c](http://code.google.com/p/inferno-ds/source/browse/trunk/devdldi.c))

luckily, there is enough work to be done!

See below & [GsocProjectIdeas](GsocProjectIdeas.md) for some of the port goals,
also take a peek at [TechnicalBackground](TechnicalBackground.md) to see the what the hardware provides.

# todo #

some short-term todo's (work in progress):

  1. wifi: ap scan, stats and tx working (see [ethernds.c](http://code.google.com/p/inferno-ds/source/browse/trunk/ethernds.c))
  1. properly update the screen, to remove pixel corruption.
  1. alternatives to access both screens with the existing input.
  1. polish more sharp edges:  memory protection, cache management (jit) ...