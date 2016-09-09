# cd-discid

cd-discid is a backend utility to get CDDB discid information from a CD-ROM disc.

Using cd-discid is very simple. It accepts a command line parameter, the
device name of the CD-ROM drive to look up. For example:

```
$ cd-discid /dev/cdrom
7c0b8b0b 11 150 23115 42165 60015 79512 101560 118757 136605 159492 176067 198875 2957
```

The information returned is, in order:

* 32-bit hex CDDB disc-id. The first byte of this is the track checksum, the
  next two are the length of the CD in seconds, and the last is the number
  of tracks
* Number of tracks
* Frame offset of each track
* Second offset for the leadout track (length of the CD in seconds)

The (lack of) inspiration behind this output format was the CDDB database
server. It accepts requests like so:

```
http://freedb.freedb.org/~cddb/cddb.cgi?cmd=cddb+query+7c0b8b0b+11+150+23115+42165+60015+79512+101560+118757+136605+159492+176067+198875+2957&hello=user+hostname+program+version&proto=3
```

There is **--musicbrainz** option which outputs a TOC that is suitable for
calculating the MusicBrainz disc id. This feature is courtesy of
Lars Magne Ingebrigtsen.

cd-discid was developed with attention to the CDDB specifications, available
at http://www.freedb.org/sections.php?name=Sections?sop=viewarticle&artid=28,
using portions of their sample code.

cd-discid should work in Linux, Solaris, FreeBSD, OpenBSD, NetBSD, MacOS X
and DragonFly.


## Installation

To compile cd-discid you need the recent version of standards compliant
C compiler. To build binary, type in terminal:

```
$ make
$ make install
```

Second command will install cd-discid files using prefix `/usr/local`. To change
prefix, append to the command: *PREFIX=/my/dir*.


## TODO

Output ID3v2 MCDI (Music CD Identifier) frames. Since these are binary in
nature it is unclear what the best method of outputting these should be.


## Development

If you have patches, please fork cd-discid at
[GitHub](https://github.com/taem/cd-discid) or simply send your patches by e-mail (see
below). If you found a bug, please report it at https://github.com/taem/cd-discid/issues.


## Homepage

Latest program version can be found at http://linukz.org/cd-discid.shtml.


## License

cd-discid is distributed under the terms GNU General Public License Version 2
or greater, or Larry Wall's Artistic license. Please see COPYING for full
GNU GPL text.


## Author

Original author is [Robert Woodcock](mailto:rcw@debian.org).
Later [Timur Birsh](mailto:taem@linukz.org) picked up maintenance.
