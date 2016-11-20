# DABlin - capital DAB experience

DABlin plays a DAB/DAB+ audio service - either from a received live
transmission or from a stored ensemble recording (frame-aligned ETI-NI).
Both DAB (MP2) and DAB+ (AAC-LC, HE-AAC, HE-AAC v2) services are
supported.

The GTK version in addition supports the data applications Dynamic Label
and MOT Slideshow (if present).


## Screenshots

### GTK version
![Screenshot of the GTK version](https://basicmaster.de/dab/DABlin.png)

### Console version
![Screenshot of the console version](https://basicmaster.de/dab/DABlin_console.png)


## Requirements

A recent GCC (with C++11 support) and GNU Make are required. Furthermore
CMake, if used for compilation (see below).

The following libraries are needed in addition:

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


### Alternative DAB+ decoder

Instead using FAAD2, DAB+ channels can be decoded with [FDK-AAC](https://github.com/mstorsjo/fdk-aac).
You can also use the modified version [fdk-aac-dabplus](https://github.com/Opendigitalradio/fdk-aac-dabplus) of
OpenDigitalradio itself, if already installed.

When the alternative AAC decoder is used, the FAAD2 library mentioned
above is no longer required.

After installing the lib, you have to:
* insert `USE_FDK-AAC=1` after the `make` call (when using Make) OR
* insert `-D USE_FDK-AAC=1` after the `cmake` call (when using CMake)

### Audio output

The SDL2 library is used for audio output, but you can instead choose to
output the decoded audio in plain PCM for further processing (e.g. for
forwarding to a streaming server).

In case you only want PCM output, you can disable SDL output and
therefore omit the SDL2 library prerequisite. You then also have to:
* insert `DISABLE_SDL=1` after the `make` call (when using Make) OR
* insert `-D DISABLE_SDL=1` after the `cmake` call (when using CMake)

### Surround sound

Services with surround sound are only decoded from their Mono/Stereo
core, as unfortunately there is no FOSS AAC decoder which supports the
required Spatial Audio Coding (SAC) extension of MPEG Surround at the
moment.


## Precompiled packages

Some users kindly provide precompiled DABlin packages on their own:

* [openSUSE](https://build.opensuse.org/package/show/home:mnhauke:ODR-mmbTools/dablin) (by Martin Hauke)
* [CentOS](https://build.opensuse.org/package/show/home:radiorabe:dab/dablin) (by [Radio Bern RaBe 95.6](http://rabe.ch)); [more info](https://github.com/radiorabe/centos-rpm-dablin)


## Compilation

The console version will be built in any case while the GTK GUI is built
only if `gtkmm` is available.

### Using Make

To compile and install DABlin, just type:
```
make
sudo make install
```

### Using CMake

You can use e.g. the following command sequence in order to compile and
install DABlin:
```
mkdir build
cd build
cmake ..
make
sudo make install
```


## Usage

You can either use the console version `dablin` or the GTK GUI version
`dablin_gtk`. Use `-h` to get an overview of all available options.

DABlin processes frame-aligned DAB ETI-NI recordings. If no filename is
specified, `stdin` is used for input.
You just have to specify the regarding Service ID (SID). The GUI version
does not necessarily need this - in that case initially no service is
played until one is chosen.

If you want to play a live station, you can use `dab2eti` from [dabtools](https://github.com/basicmaster/dabtools)
(optimized fork) and transfer the ETI live stream via pipe, e.g.:
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
with it. If you want/have to use a gain value you can specify it using
`-g`.


## Status output

During (re-)synchronisation status messages are output. Also dropped
Superframes or AU are mentioned.

If the Reed Solomon FEC was used to correct bytes of a Superframe, this
is mentioned by messages of the format `(3+)` in cyan color. This 
shorter format is used as those messages occure several times with
borderline reception. The digit refers to the number of corrected bytes
within the Superframe while a plus (if present) indicates that at least
one byte was incorrectable.

Audio Units (AUs) with invalid CRC are mentioned with short format
messages like `(AU #2)` in red color, indicating that the CRC check on
AU No. 2 failed and hence the AU was dismissed.


## TODO

At the moment, DABlin is kind of a rudimentary tool for the playback of
DAB/DAB+ services. It is planned to add support for further Program
Aided Data (PAD) features.

### Slideshow

The GTK version supports the MOT Slideshow if used by the current
service. The slide window is initially hidden but appears as soon as the
first slide has been received completely and without errors.

Currently the following limitations apply:
* slideshows in a separate subchannel are not supported (just X-PAD)
* for MOT the hardcoded (default) X-PAD Application Types 12/13 are used
* the MOT header extension is not parsed/checked (e.g. for TriggerTime)


## License

(please see the file COPYING for further details)

DABlin - capital DAB experience
Copyright (C) 2015-2016 Stefan Pöschel

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
