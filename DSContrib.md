# Introduction #

There're a couple of resources around to ease development: svn, downloads, wiki and group list.

The idea/purpose is to use them in a easy way to examine, follow and understand.

# Rules #

Here is the plan, when a important milestone is reached:

  1. [[svn](http://code.google.com/p/inferno-ds/source/list)] commit changes to svn **[[NOTE](NOTE.md)]**
  1. [[group list](http://groups.google.com/group/inferno-ds)] write a post describing what's been done, like: Subject "inferno-ds usable on desmume (rev 67)"
  1. [[download](http://code.google.com/p/inferno-ds/downloads/)] upload a nds file to the downloads section, like filename: "isds\_r67.nds".

**[[NOTE](NOTE.md)]**: remember to reference the svn revision number of the changes in the 2) & 3) steps, so the downloads, the group posts and the changes performed to the code can be tracked without confusion.

# Examples #
That's more or less what we've been doing lately, see:

  * http://groups.google.com/group/inferno-ds/t/2053198167d994aa
  * http://groups.google.com/group/inferno-ds/t/aa4a1d6fe7182270

# BUG reports #

Before submitting a bug report take a look at:
``[How to Report Bugs Effectively](http://www.chiark.greenend.org.uk/~sgtatham/bugs.html)''
if you're short of time read the '**Summary**' at the bottom of the document.

# emulators #

use of DS emus: desmume & no$gba (which Npe has encouraged me to use).

NOTE that emus don't replace the hardware, but they make the first steps of adding new code and testing a bit faster, so keep in mind that the purpose is "to go for the real thing" (hardware).

Wifi networking also helps to decrease the code/compile/test cycle time.

As one doesn't have to copy the programs to the storage card and power up the ds
and launch the nds images, just put the images in a directory and use wifime to made
them reachable through wifi.

# git-svn #

**Command-Line Access**

`git svn clone --username user -s https://inferno-ds.googlecode.com/svn ds` _(members)_

`git svn clone -s  http://inferno-ds.googlecode.com/svn ds-read-only` _(non members)_

more info: [Develop with Git on a Google Code Project](http://google-opensource.blogspot.com/2008/05/develop-with-git-on-google-code-project.html)