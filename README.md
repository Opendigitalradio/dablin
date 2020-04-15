# DABlin – capital DAB experience

DABlin plays a DAB/DAB+ audio service – from a live transmission or from
a stored ensemble recording (ETI-NI, or EDI AF with ETI). Both DAB (MP2)
and DAB+ (AAC-LC, HE-AAC, HE-AAC v2) services are supported.

The GTK GUI version in addition supports the data applications Dynamic
Label and MOT Slideshow (if used by the selected service).


## Screenshots

### GTK GUI version
![Screenshot of the GTK GUI version](https://basicmaster.de/dab/DABlin.png)

### Console version
![Screenshot of the console version](https://basicmaster.de/dab/DABlin_console.png)


## Requirements

### General

Besides Git a C/C++ compiler (with C++11 support) and CMake are required to
build DABlin.

On Debian or Ubuntu, the respective packages (with GCC as C/C++ compiler) can be
installed using aptitude or apt-get, for example:

```sh
sudo apt-get install git gcc g++ cmake
```


### Libraries

The following libraries are required:

* mpg123 (1.14.0 or higher)
* FAAD2
* SDL2

The GTK GUI version in addition requires:

* gtkmm

Usually the `glibc` implementation of `iconv` is available. If this is
not the case, in addition `libiconv` is required.

In rare cases, the target architecture does not support *atomics*. In
such a case, DABlin is linked against GCC's `libatomic`. This lib
usually is an (indirect) dependency of GCC itself.

On Debian or Ubuntu, mpg123, FAAD2, SDL2 and gtkmm are packaged and installed
with:

```sh
sudo apt-get install libmpg123-dev libfaad-dev libsdl2-dev libgtkmm-3.0-dev
```

On Fedora, mpg123, SDL2, and gtkmm are all packaged and can be installed thus:

```sh
sudo dnf install mpg123-devel SDL2-devel gtkmm30-devel
```

FAAD2 is not packaged in the main Fedora repository, but it is available in
[RPM Fusion repository](https://rpmfusion.org/). Once you have added RPM Fusion
to the repositories, FAAD2 may be installed by:

```sh
sudo dnf install faad2-devel
```

If you do not wish to, or cannot, add the RPM Fusion repositories, you will have
to download FAAD2, perhaps from [here](http://www.audiocoding.com/faad2.html), and build
and install manually.


### Alternative DAB+ decoder

Instead of using FAAD2, DAB+ channels can be decoded with [FDK-AAC](https://github.com/mstorsjo/fdk-aac).
You can also use [OpenDigitalradio's fork](https://github.com/Opendigitalradio/fdk-aac), if already installed.

On Debian and Ubuntu, you can install FDK-AAC with:

```sh
sudo apt-get install libfdk-aac-dev
```

On Fedora, RPM Fusion is again needed and, if used, you can:

```sh
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

To enable the PCM output to `stdout`, the `-p` parameter has to be used.

It is also possible to disable the output of any decoded audio and
instead output the current service as an untouched MP2/AAC stream to
`stdout`. This can be achieved by using the `-u` parameter.


### Surround sound

Services with surround sound are only decoded from their Mono/Stereo
core, as unfortunately there is no FOSS AAC decoder which supports the
required Spatial Audio Coding (SAC) extension of MPEG Surround at the
moment.


## Precompiled packages and source-based Linux distributions

Official precompiled packages are available for the following Linux
distributions (kindly maintained by Gürkan Myczko):

* [Debian](https://packages.debian.org/dablin)
* [Ubuntu](https://launchpad.net/ubuntu/+source/dablin)

On Ubuntu 18.04 you can simply install DABlin from the official package
sources (note that the GitHub version may be newer):

```sh
sudo apt-get install dablin
```

Some users kindly provide precompiled packages on their own:

* [openSUSE](https://build.opensuse.org/package/show/home:mnhauke:ODR-mmbTools/dablin) (by Martin Hauke)
* [CentOS](https://build.opensuse.org/package/show/home:radiorabe:dab/dablin) (by [Radio Bern RaBe 95.6](http://rabe.ch)); [more info](https://github.com/radiorabe/centos-rpm-dablin)

For other distributions you may also want to check the [Repology page](https://repology.org/metapackage/dablin).

Source-based Linux distributions:

* [Gentoo](https://github.com/paraenggu/delicious-absurdities-overlay/tree/master/media-sound/dablin) (by Christian Affolter, as part of his [delicious-absurdities-overlay](https://github.com/paraenggu/delicious-absurdities-overlay))


## Compilation

If the gtkmm library is available both the console and GTK GUI executables will
be built. If the gtkmm library is not available only the console executable will
be built.

To fetch the DABlin source code, execute the following commmands:

```sh
git clone https://github.com/Opendigitalradio/dablin.git
cd dablin
```

Note that by default the `master` branch is cloned which contains the
current stable version. The development takes place in the `next` branch
which can instead be cloned by appending `-b next` to the end of the
above `git clone` command line.

You can use, for example, the following command sequence in order to compile and
install DABlin:

```sh
mkdir build
cd build
cmake ..
make
sudo make install
```


### Windows (Cygwin)

DABlin can also be compiled on Windows using [Cygwin](https://cygwin.com/). The following
Cygwin packages are required:

General:
- git
- make
- cmake
- gcc-core
- gcc-g++

DABlin specific (using FDK-AAC for DAB+ services):
- libmpg123-devel
- libfdk-aac-devel
- libSDL2-devel
- libiconv-devel

In addition for the GTK version:
- libgtkmm3.0-devel

Note that the GTK version requires an X server to run e.g. Cygwin/X!

Also note that Cygwin neither needs nor allows to `sudo` commands, so
just execute them without that prefix.

Unfortunately the Cygwin package of FDK-AAC doesn't seem to have been
compiled with SBR support, so using [FAAD2](http://www.audiocoding.com/faad2.html) for DAB+ services is
recommended. However FAAD2 has to be compiled and installed by hand, as
there is no Cygwin package. This requires the following additional
packages to be installed:
- autoconf
- automake
- libtool

![Screenshot of the console version on Windows (Cygwin)](https://basicmaster.de/dab/DABlin_console_cygwin.png)

When Cygwin is installed, all the aforementioned packages can be
pre-selected for installation by calling Cygwin's `setup-<arch>.exe`
with the following parameter:

```sh
-P git,make,cmake,gcc-core,gcc-g++,libmpg123-devel,libfdk-aac-devel,libSDL2-devel,libiconv-devel,libgtkmm3.0-devel,autoconf,automake,libtool
```


### macOS

On macOS, the development environment can be installed by running
`xcode-select`. This installs Git and a C/C++ compiler (clang). All
other packages and development libraries can be installed using a
package manager such as [Homebrew](https://brew.sh), for example:

```sh
xcode-select --install
brew install pkg-config cmake gtkmm gtkmm3 adwaita-icon-theme sdl2 fftw faad2 mpg123
```


## Usage

The console executable is called `dablin`, the GTK GUI executable
`dablin_gtk`. Use `-h` to get an overview of all available options.

(Currently no desktop files are installed so it is not easy to start DABlin
directly from GNOME Shell. For now, at least, start DABlin from a console.)

DABlin processes DAB ETI-NI or EDI recordings/streams (which may be
frame-aligned or not i.e. any data before/after an ETI/EDI frame is
discarded). If no filename is specified, `stdin` is used for input.

You just should specify the service ID (SID) of the desired service
using `-s` - otherwise initially no service is played. The GUI version
of course does not necessarily need this.

You can replay an existing recording as follows:

```sh
dablin -s 0xd911 mux.eti
```

In this case a progress indicator and the current position is displayed.

As an alternative a service label can be specified with the `-l` option.
Note that if the label contains spaces, it has to be enclosed by quotes
(or the spaces be properly escaped):

```sh
dablin -l "SWR1 RP" mux.eti
```

The parameter `-1` (the number one, not the small letter L) simply plays
the first service found.

With the console version, instead of the desired service it is also
possible to directly request a specific sub-channel by using `-r` (for
DAB) or `-R` (for DAB+).

Note that the console output always shows the programme type just using
RDS PTYs despite the actually used international table ID (which should
work in nearly all cases). The GTK version in contrast always shows the
correct programme type, based on the transmitted international table ID.

Dynamic FIC messages can be suppressed using `-F` (currently affects
dynamic PTY only).


### Date/Time

The console version shows the related parameters as they are received:
While the Local Time Offset (LTO) is shown upon any change, the UTC
date/time is shown once.

The GTK version in contrast starts to display the local date/time as
soon as both mentioned values have been received. The used clock is then
resynchronised upon further received UTC date/time.


### Announcement support/switching

In terms of announcement support/switching the console version shows the
separate details. The GUI combines the received data and in general
shows which announcements the current service supports.

During a matching announcement, the corresponding type is highlighted in
yellow. An announcement that would lead to a (temporary) switch to a
different subchannel leads to cyan highlighting instead e.g. if traffic
news of a different channel shall affect also listeners of a different
service (which does not have its own announcements).


### Hotkeys (GTK GUI version)

To switch the channel instead of the service, press `Ctrl` in addition.

Hotkey           | Meaning
-----------------|---------------------------------------------
`m`              | Enable/disable audio mute
`r`              | Start/stop recording
`Ctrl` + `c`     | Copy DL text or Slideshow slide to clipboard
`-`              | Switch to previous service
`+`              | Switch to next service
`1`..`0`         | Switch to 1st..10th service
`Alt` + `1`..`0` | Switch to 11th..20th service
`Ctrl` + `Space` | Stop/resume decoding the current channel/service


### DAB live reception

If you want to play a live station, you can use `dab2eti` from [dabtools](https://github.com/Opendigitalradio/dabtools)
(ODR maintained fork) and transfer the ETI live stream via pipe, e.g.:

```sh
dab2eti 216928000 | dablin_gtk
```

It is possible to let DABlin invoke `dab2eti` or any other DAB live
source that outputs ETI-NI. The respective binary is then called with
the necessary parameters, including the frequency and an optional gain
value.

You therefore just have to specify the path to the `dab2eti` binary and
the desired channel.

```sh
dablin -d ~/bin/dab2eti -c 11D -s 0xd911
```

Using `dab2eti` the E4000 tuner is recommended as auto gain is supported
with it. If you want/have to use a gain value you can specify it using
`-g`. Instead of auto gain, the default gain can be set using `-G` (only
affects `eti-cmdline` at the moment).

Instead of `dab2eti` the tool `eti-cmdline` by Jan van Katwijk can be
used, as it has more sensitive reception (however the CPU load is higher
compared to `dab2eti`) and does not require a E4000 tuner for auto gain.
It is part of his [eti-stuff](https://github.com/JvanKatwijk/eti-stuff).

In addition to specifying the path to the respective binary you also
have to change the DAB live source type accordingly by using `-D`.

```sh
dablin -d ~/bin/eti-cmdline-rtlsdr -D eti-cmdline -c 11D -s 0xd911
```

When enclosed in quotes, you can also pass command line parameters to the
binary, e.g. to set some frequency correction (here: +40 ppm):

```sh
dablin -d "~/bin/eti-cmdline-rtlsdr -P 40" -D eti-cmdline -c 11D -s 0xd911
```

In case of the GTK GUI version the desired channel may not be specified. To
avoid the huge channel list containing all possible DAB channels, one
can also state the desired channels (separated by comma) which shall be
displayed within the channel list. Hereby the specified (general) gain
value can also be overwritten for a channel by adding the desired value
after a colon, e.g. `5C:-54`.

```sh
dablin_gtk -d ~/bin/dab2eti -c 11D -C 5C,7B,11A,11C,11D -s 0xd911
```

It may happen that an ETI live stream is interrupted (e.g. transponder
lock lost). Later when the stream recovers, DABlin "catches up" on the
stream and plays all (available) ETI frames until again in sync. This
can lead to several audio buffer overflow messages and respective
audible artifacts.

The `-I` parameter disables the described catch-up behaviour and instead
resyncs to the stream after an interruption i.e. continues to play the
later received ETI frames in realtime. However this means that the
playback is delayed by the amount of all previous interruptions i.e. the
news will start some seconds/minutes later compared to live reception
because of that.

The GTK GUI version also allows to stop decoding the current
channel/service by using the stop button next to the channel combobox.
If desired, decoding can then be resumed using the same button again.

The `stdin` input can also be used for live reception or playback of an
EDI AF stream/recording (containing ETI) e.g. with Netcat, Wget or cURL.
Hereby the AF layer may also be enclosed with a File IO layer (usually
the case for stored EDI transmissions).

Examples:

```sh
nc 10.0.0.128 9201 | dablin_gtk -f edi -I
```

```sh
wget -q -O - https://edistream.irt.de/services/3 | dablin_gtk -f edi -I -1
```

```sh
curl -s https://edistream.irt.de/services/3 | dablin_gtk -f edi -I -1
```

Playing EDI content the `-I` can be required to prevent ongoing hiccups
during playback. In the last two cases, a single service EDI stream for
testing purposes is used, so the `-1` parameter is used to select the
first found service. 


### Recording a service

With the GTK GUI version the current service can be recorded into a
file. To start/stop a recording, the red record button has to be
clicked, or the corresponding keyboard shortcut to be pressed. During a
recording neither can channel/service be changed, nor can DABlin be
closed.

By default all recordings are stored to `/tmp`. This can be changed by
using the `-r` parameter. The filename of a recording contains the
timestamp of the start of the recording and the service name, e.g.
`2018-09-02 - 17-53-54 - SWR3.aac`.

All recordings are lossless: A DAB service is directly stored in MP2
format. A DAB+ service is packaged into the LATM/LOAS container format.
This format is necessary in order to signal the special 960 samples per
frame transformation to the AAC decoder for playback.

As both formats are streamable, a recording can already be played while
the actual recording is still in progress.

The AAC recordings can be player e.g. by VLC player. FFmpeg will play
them as well, but currently does not support the SBR extension together
with the 960 samples per frame transformation and will thus only play
the AAC core then.

It is possible to enable a recording prebuffer which continuously caches
a certain period of time. This allows to record e.g. an already running
song or bloopers. When an actual recording is started, the complete
prebuffer content is initially written to the recording file. The
prebuffer size in seconds is specified using `-P` e.g. `-P 600` for ten
minutes.


### Secondary component audio services

Some ensembles may contain audio services that consist of additional
"sub services" called secondary components, in addition to the primary
component. That secondary components can initially be selected by using
`-x` in addition to `-s`.

In the GTK version in the service list such components are shown
prefixed with `» ` (e.g. `» BBC R5LiveSportX`). Meanwhile the related
primary component is suffixed with ` »` (e.g. `BBC Radio 5 Live »`).


## Status output

While playback a number of status messages may appear. Some are quite common
(enclosed with round brackets) e.g. due to bad reception, while others are
rather unlikely to occur (enclosed with square brackets).

### Common

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

MP2 frames with invalid CRC (MP2's CRC only - not DAB's ScF-CRC) are
discarded, which is indicated by a `(CRC)` message in red color.

Audio Units (AUs) with invalid CRC are mentioned with short format
messages like `(AU #2)` in red color, indicating that the CRC check on
AU No. 2 failed and hence the AU was dismissed.

When the decoding of an AU nevertheless fails, this is indicated by an
`(AAC)` message in magenta color. However in that case the AAC decoder
may output audio samples anyway.

### Uncommon

If the announced X-PAD length of a DAB+ service does not match the available
X-PAD length i.e. if it falls below, a red `[X-PAD len]` message is shown and
the X-PAD is discarded. However not all X-PADs may be affected and hence it may
happen that the Dynamic Label can be processed but the MOT Slideshow cannot.

To anyhow process affected X-PADs, a loose mode can be enabled by using
the `-L` parameter.


## Standards

DABlin implements (at least partly) the following DAB standards:

### General
* ETSI EN 300 401 (DAB system)
* ETSI TS 101 756 (Registered tables)
* ETSI TS 103 466 (DAB audio)
* ETSI TS 102 563 (DAB+ audio)
* ETSI ETS 300 799 (ETI)
* ETSI TS 102 693 (EDI) together with ETSI TS 102 821 (DCP)

### Data applications
* ETSI EN 301 234 (MOT)
* ETSI TS 101 499 (MOT Slideshow)


## TODO

At the moment, DABlin is kind of a rudimentary tool for the playback of
DAB/DAB+ services. It is planned to add support for further Program
Aided Data (PAD) features.

### Slideshow

The GTK GUI version supports the MOT Slideshow. If Slideshow is enabled
and the current service signals to transmit a Slideshow, the Slideshow
window is displayed. It shows a slide after it has been received
completely and without errors.

Currently the following limitations apply:

* slideshows in a separate sub-channel are not supported (just X-PAD);
* the TriggerTime field does not support values other than Now


## License

This software is licensed under the GNU General Public License Version 3
(please see the file COPYING for further details).
![GPLv3 Image](https://www.gnu.org/graphics/gplv3-88x31.png)

*Please note that the included FEC lib by KA9Q has a separate license!*

DABlin - capital DAB experience
Copyright (C) 2015-2020 Stefan Pöschel

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
