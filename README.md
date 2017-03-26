# DABlin – capital DAB experience

DABlin plays a DAB/DAB+ audio service – either from a received live
transmission or from a stored ensemble recording (frame-aligned ETI-NI).
Both DAB (MP2) and DAB+ (AAC-LC, HE-AAC, HE-AAC v2) services are
supported.

The GTK GUI version in addition supports the data applications Dynamic Label
and MOT Slideshow (if used by the selected service).


## Screenshots

### GTK GUI version
![Screenshot of the GTK GUI version](https://basicmaster.de/dab/DABlin.png)

### Console version
![Screenshot of the console version](https://basicmaster.de/dab/DABlin_console.png)


## Requirements

### General

A recent GCC (with C++11 support) and CMake are required.

The following libraries are required:

* mpg123 (1.14.0 or higher)
* FAAD2
* SDL2

The GTK GUI version in addition requires:

* gtkmm

On Debian or Ubuntu, mpg123, FAAD2, SDL2 and gtkmm are packaged and all the required files
can be installed using aptitude or apt-get, for example:

```
sudo apt-get install libmpg123-dev libfaad-dev libsdl2-dev libgtkmm-3.0-dev
```

On Fedora, mpg123, SDL2, and gtkmm are all packaged and can be installed thus:

```
sudo dnf install mpg123-devel SDL2-devel gtkmm30-devel
```

FAAD2 is not packaged in the main Fedora repository, but it is available in
[RPM Fusion repository](https://rpmfusion.org/). Once you have added RPM Fusion
to the repositories, FAAD2 may be installed by:

```
sudo dnf install faad2-devel
```

If you do not wish to, or cannot, add the RPM Fusion repositories, you will have
to download FAAD2, perhaps from [here](http://www.audiocoding.com/faad2.html), and build
and install manually.


### Alternative DAB+ decoder

Instead of using FAAD2, DAB+ channels can be decoded with [FDK-AAC](https://github.com/mstorsjo/fdk-aac).
You can also use the modified version [fdk-aac-dabplus](https://github.com/Opendigitalradio/fdk-aac-dabplus) of
OpenDigitalradio itself, if already installed.

On Debian and Ubuntu, you can install FDK-AAC with:

```
sudo apt-get install libfdk-aac-dev
```

On Fedora, RPM Fusion is again needed and, if used, you can:

```
sudo dnf install fdk-aac-devel
```

When the alternative AAC decoder is used, the FAAD2 library mentioned
above is no longer required.

After installing the library, to use FDK-AAC instead of FAAD2, you have to
have `-DUSE_FDK-AAC=1` as part of the `cmake` command.

### Audio output

The SDL2 library is used for audio output, but you can instead choose to
output the decoded audio in plain PCM for further processing (e.g. for
forwarding to a streaming server).

In case you only want PCM output, you can disable SDL output and
therefore omit the SDL2 library prerequisite. You then also have to
have `-DDISABLE_SDL=1` as part of the `cmake` command.


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

If the gtkmm library is available both the console and GTK GUI executables will
be built. If the gtkmm library is not available only the console executable will
be built.

You can use, for example, the following command sequence in order to compile and
install DABlin:

```
mkdir build
cd build
cmake ..
make
sudo make install
```


## Usage

The console executable is called `dablin`, the GTK GUI executable
`dablin_gtk`. Use `-h` to get an overview of all available options.

(Currently no desktop files are installed so it is not easy to start DABlin
directly from GNOME Shell. For now, at least, start DABlin from a console.)

DABlin processes frame-aligned DAB ETI-NI recordings. If no filename is
specified, `stdin` is used for input.
You just have to specify the regarding Service ID (SID). The GUI version
does not necessarily need this - in that case initially no service is
played until one is chosen.

If you want to play a live station, you can use `dab2eti` from [dabtools](https://github.com/Opendigitalradio/dabtools)
(ODR maintained fork) and transfer the ETI live stream via pipe, e.g.:

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

With the console version, instead of the desired service it is also
possible to directly request a specific subchannel by using `-r` (for
DAB) or `-R` (for DAB+).

In case of the GTK GUI version the desired channel may not be specified. To
avoid the huge channel list containing all possible DAB channels, one
can also state the desired channels (separated by comma) which shall be
displayed within the channel list.
```
dablin_gtk -d ~/bin/dab2eti -c 11D -C 5C,7B,11A,11C,11D -s 0xd911
```

Using `dab2eti` the E4000 tuner is recommended as auto gain is supported
with it. If you want/have to use a gain value you can specify it using
`-g`.

In addition to the respective button, the GTK GUI version also allows the
keyboard shortcut `m` to toggle muting the audio.


## Status output

During (re-)synchronisation status messages are shown. Also dropped
Superframes or AU are mentioned.

If the Reed Solomon FEC was used to correct bytes of a Superframe, this
is mentioned by messages of the format `(3+)` in cyan color. This
shorter format is used as those messages occure several times with
borderline reception. The digit refers to the number of corrected bytes
within the Superframe while a plus (if present) indicates that at least
one byte was incorrectable.

When a FIB is discarded (due to failed CRC check), this is indicated by a
`(FIB)` message in yellow color.

Audio Units (AUs) with invalid CRC are mentioned with short format
messages like `(AU #2)` in red color, indicating that the CRC check on
AU No. 2 failed and hence the AU was dismissed.


## Standards

DABlin implements (at least partly) the following DAB standards:

### General
* ETSI EN 300 401 (DAB system)
* ETSI TS 101 756 (Registered tables)
* ETSI TS 103 466 (DAB audio)
* ETSI TS 102 563 (DAB+ audio)
* ETSI ETS 300 799 (ETI)

### Data applications
* ETSI EN 301 234 (MOT)
* ETSI TS 101 499 (MOT Slideshow)


## TODO

At the moment, DABlin is kind of a rudimentary tool for the playback of
DAB/DAB+ services. It is planned to add support for further Program
Aided Data (PAD) features.

### Slideshow

The GTK GUI version supports the MOT Slideshow if used by the selected
service. The slide window is initially hidden but appears as soon as the
first slide has been received completely and without errors.

Currently the following limitations apply:

* slideshows in a separate subchannel are not supported (just X-PAD);
* for MOT the hardcoded (default) X-PAD Application Types 12/13 are used;
* the TriggerTime field is not processed (except the value Now)


## License

This software is licensed under the GNU General Public License Version 3
(please see the file COPYING for further details).
![GPLv3 Image](https://www.gnu.org/graphics/gplv3-88x31.png)

*Please note that the included FEC lib by KA9Q has a separate license!*

DABlin - capital DAB experience
Copyright (C) 2015-2017 Stefan Pöschel

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
