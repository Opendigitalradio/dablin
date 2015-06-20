DABlin - capital DAB experience
===============================

DABlin plays a DAB/DAB+ audio service - either from a received live
transmission or from a stored ensemble recording (frame-aligned ETI-NI).
Both DAB (MP2) and DAB+ (AAC-LC, HE-AAC, HE-AAC v2) services are
supported.

![Screenshot of the GTK version](http://www.basicmaster.de/dab/DABlin.png)

Requirements
------------
Besides a recent GCC (with C++11 support) and GNU Make, the following
libraries are needed:

* ka9q-fec (tested with https://github.com/Opendigitalradio/ka9q-fec)
* mpg123
* FAAD2
* SDL2

On e.g. Ubuntu 14.04 you can install them (except ka9q-fec) via:
```
sudo apt-get install libmpg123-dev libfaad-dev libsdl2-dev
```


For the GTK GUI, you furthermore need:
* gtkmm

Package installation on e.g. Ubuntu 14.04:
```
sudo apt-get install libgtkmm-3.0-dev
```


Instead using FAAD2, DAB+ channels can be decoded with [FDK-AAC](https://github.com/mstorsjo/fdk-aac).
After installing it, you have to enable the regarding line in the
Makefile.

Compilation
------------
To compile DABlin, just type `make`. If you only want to compile the
console or the GTK version, you can use `make dablin` and/or
`make dablin_gtk`.


Usage
-----

You can either use the console version `dablin` or the GTK GUI version
`dablin_gtk`.
DABlin processes frame-aligned DAB ETI-NI recordings. If no filename is
specified, `stdin` is used for input.
You just have to specify the regarding Service ID (SID). The GUI version
does not necessarily need this - in that case initially no service is
played until one is chosen.

If you want to play a live station, you can use `dab2eti` from [dabtools](https://github.com/linuxstb/dabtools)
and transfer the ETI live stream via pipe, e.g.:
```
dab2eti 216928000 | dablin_gtk
```

You can also replay an existing ETI-NI recording, e.g.:
```
dablin -s 0xd911 mux.eti
```


It is possible to let DABlin invoke `dab2eti`. You therefore just have
to specify the path to the `dab2eti` binary and the desired channel.
```
dablin -d ~/bin/dab2eti -c 11D -s 0xd911
```

In case of the GTK version the desired channel may not be specified. To
avoid the huge channel list containing all possible DAB channels, one
can also state the desired channels (separated by comma) which shall be
displayed within the channel list.
```
dablin_gtk -d ~/bin/dab2eti -c 11D -C 5C,7B,11A,11C,11D -s 0xd911
```

Using `dab2eti` the E4000 tuner is recommended as auto gain is supported
with it.


Status output
-------------
During (re-)synchronisation status messages are output. Also dropped
Superframes or AU are mentioned.

If the Reed Solomon FEC was used to correct bytes of a Superframe, this
is mentioned by messages of the format `(3+)`. This shorter format is
used as those messages occure several times with borderline reception.
The digit refers to the number of corrected bytes within the Superframe
while a plus (if present) indicates that at least one byte was
incorrectable.


TODO
----
At the moment, DABlin is kind of a rudimentary tool for the playback of
DAB/DAB+ services. It is planned to add support for Program Aided Data
(PAD) like MOT Slideshow et cetera.


License
-------
(please see the file COPYING for further details)

DABlin - capital DAB experience
Copyright (C) 2015 Stefan Pöschel

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
