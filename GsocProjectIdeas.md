# Abstract #

One of the best aspects of the Nintendo DS is its wifi support. Since much of Inferno's novelty and power comes from using its Styx protocol over the network, working wifi on the Inferno DS port would be a killer app for the DS. Styx over wifi gives Inferno the ability to use other computing resources transparently from the DS. Users can access computational grids, devices like cameras and even other system's screens, mice and keyboards transparently from the DS. This has the potential to act as a "force multiplier", taking the DS's novel interface but relative lack of computing power and letting the DS take advantage of other more powerful resources.

With this motivation in mind we are looking for a Google Summer of Code student to implement wifi support.

# Motivation #

Here, are a few ideas/examples that exploit the cool features of the DS and Inferno
when they're brought together.

Think of the following examples:

## Accessing media ##

set up a auth/cmd/styx server on the laptop, and on the DS do:
```
	mount laptop /n/remote
	bind /n/remote/cmd /cmd

	# then play and aif, wav, mp3, ogg files
	# using the tools/progs available on the laptop
	os -m /cmd madplay < file.mp3
	os -m /cmd ogg123 < file.ogg
	os -m /cmd auplay < file.wav
```

## Distributed apps ##
```
	cpu laptop
	# then run any commands at laptop
	
	cpug wm/wm
	# and get a working wm/wm running at the laptop
	# and manage resources as if you where at the laptop

```

## Browsing ##
```
	charon http://www.google.com
```

## Voip ##
```
	mount laptop /n/remote
	stream /dev/audio /n/remote/audio
	# then you've cross-conected both audio ends
```
> and what you get?

> ![http://www.fotosearch.com/comp/IMP/IMP161/tin-telephone-~-INGPCYHE0013.jpg](http://www.fotosearch.com/comp/IMP/IMP161/tin-telephone-~-INGPCYHE0013.jpg)

## Touch/Input based text entry ##
See Caerwyn's quong [code](http://code.google.com/p/inferno-lab/source/detail?r=70)
to implement a method similar to
[HexInput](http://www.strout.net/info/ideas/hexinput.html)
for text entry.

This can be used as an alternative to
[wm/keyboard(1)](http://www.vitanuova.com/inferno/man/1/keyboard.html).

![http://farm4.static.flickr.com/3125/2681867639_b4efa8a151_o_d.png](http://farm4.static.flickr.com/3125/2681867639_b4efa8a151_o_d.png)

source: http://www.flickr.com/photos/caerwyn/2681867639/

## ... ##

_(your own uses/examples here using: venti, midi ...)_

# What to do #

**Code**: see
[dslinux](http://dslinux.gits.kiev.ua/trunk/linux-2.6.x/arch/arm/mach-nds/arm7/) arm7/wifi.c code and addapt it to have wifi working on the inferno-ds.

So the plan for a student project would be:

  1. get fifo ipc working: **done**
  1. get [arm7/wifi.c](http://code.google.com/p/inferno-ds/source/browse/trunk/arm7/wifi.c) working: **done**
  1. write a ([ethernds.c](http://code.google.com/p/inferno-ds/source/browse/trunk/ethernds.c)) to interface [arm7/wifi.c](http://code.google.com/p/inferno-ds/source/browse/trunk/arm7/wifi.c): **wip** _(in short: (3) = (1) + (2))_


**Testing**, at the beginning the implementation can be tested with emus,
and at the same time directly on the hardware,
by using wifime share the nds files over the wifi:

  * http://masscat.afraid.org/ninds/rt2570.php
  * http://masscat.afraid.org/ninds/wifi_apps.php
  * (see the also the TechnicalBackground wiki page),
> the Nintendo DS Usb wifi dongle, can be used also to set an ap with the rt2570usb.