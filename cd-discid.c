/*
 * Copyright (c) 1999-2003 Robert Woodcock <rcw@debian.org>
 * Copyright (c) 2009-2013 Timur Birsh <taem@linukz.org>
 * This code is hereby licensed for public consumption under either the
 * GNU GPL v2 or greater, or Larry Wall's Artistic license - your choice.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Porting credits:
 * Solaris: David Champion <dgc@uchicago.edu>
 * FreeBSD: Niels Bakker <niels@bakker.net>
 * OpenBSD: Marcus Daniel <danielm@uni-muenster.de>
 * NetBSD: Chris Gilbert <chris@NetBSD.org>
 * MacOSX: Evan Jones <ejones@uwaterloo.ca> http://www.eng.uwaterloo.ca/~ejones/
 *         Thomas Klausner <tk@giga.or.at>
 * DragonFly: Thomas Klausner <tk@giga.or.at>, http://pkgsrc.se/audio/cd-discid
 * IRIX: https://github.com/canavan, https://github.com/taem/cd-discid/issues/4
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* close() */
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#if defined(linux)
#include <linux/cdrom.h>
#define cdte_track_address      cdte_addr.lba
#define DEVICE_NAME             "/dev/cdrom"

#elif defined(__GNU__)
/* According to Samuel Thibault <sthibault@debian.org>, cd-discid needs this
 * to compile on Debian GNU/Hurd (i386) */
#include <sys/cdrom.h>
#define cdte_track_address      cdte_addr.lba
#define DEVICE_NAME             "/dev/cd0"

#elif defined(sun) && defined(unix) && defined(__SVR4)
#include <sys/cdio.h>
#define CD_MSF_OFFSET   150
#define CD_FRAMES       75
/* According to David Schweikert <dws@ee.ethz.ch>, cd-discid needs this
 * to compile on Solaris */
#define cdte_track_address      cdte_addr.lba
#define DEVICE_NAME             "/dev/vol/aliases/cdrom0"

/* __FreeBSD_kernel__ is needed for properly compiling on Debian GNU/kFreeBSD
   Look at http://glibc-bsd.alioth.debian.org/porting/PORTING for more info */
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <sys/cdio.h>
#define CDROM_LBA               CD_LBA_FORMAT    /* first frame is 0 */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define CD_FRAMES               75               /* per second */
#define CDROM_LEADOUT           0xAA             /* leadout track */
#define CDROMREADTOCHDR         CDIOREADTOCHEADER
#define CDROMREADTOCENTRY       CDIOREADTOCENTRY
#define cdrom_tochdr            ioc_toc_header
#define cdth_trk0               starting_track
#define cdth_trk1               ending_track
#define cdrom_tocentry          ioc_read_toc_single_entry
#define cdte_track              track
#define cdte_format             address_format
#define cdte_track_address      entry.addr.lba
#define DEVICE_NAME             "/dev/cdrom"

#elif defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/cdio.h>
#define CDROM_LBA               CD_LBA_FORMAT    /* first frame is 0 */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define CD_FRAMES               75               /* per second */
#define CDROM_LEADOUT           0xAA             /* leadout track */
#define CDROMREADTOCHDR         CDIOREADTOCHEADER
#define cdrom_tochdr            ioc_toc_header
#define cdth_trk0               starting_track
#define cdth_trk1               ending_track
#define cdrom_tocentry          cd_toc_entry
#define cdte_track              track
#define cdte_track_address      addr.lba
#define DEVICE_NAME             "/dev/cd0a"

#elif defined(__APPLE__)
#include <sys/types.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#define CD_FRAMES               75               /* per second */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define cdrom_tochdr            CDDiscInfo
#define cdth_trk0               numberOfFirstTrack
/* NOTE: Judging by the name here, we might have to do this:
 * hdr.lastTrackNumberInLastSessionMSB << 8 *
 * sizeof(hdr.lastTrackNumberInLastSessionLSB)
 * | hdr.lastTrackNumberInLastSessionLSB; */
#define cdth_trk1               lastTrackNumberInLastSessionLSB
#define cdrom_tocentry          CDTrackInfo
#define cdte_track_address      trackStartAddress
#define DEVICE_NAME             "/dev/rdisk1"

#elif defined(__sgi)
#include <dmedia/cdaudio.h>
#define CD_FRAMES               75               /* per second */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define cdrom_tochdr            CDSTATUS
#define cdth_trk0               first
#define cdth_trk1               last
#define close                   CDclose
struct cdrom_tocentry
{
	int cdte_track_address;
};
#define DEVICE_NAME             "/dev/scsi/sc0d4l0"
#else
#error "Your OS isn't supported yet."
#endif  /* os selection */

int cddb_sum(int n)
{
	/* a number like 2344 becomes 2+3+4+4 (13) */
	int ret = 0;

	while (n > 0) {
		ret = ret + (n % 10);
		n = n / 10;
	}

	return ret;
}

void usage()
{
	fprintf(stderr, "Usage: cd-discid [<option> ...]\n\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --musicbrainz   Output a TOC that is suitable "
		"for calculating the MusicBrainz Disc ID.\n");
	fprintf(stderr, "  --help          Show this message.\n");
	fprintf(stderr, "  --version       Show the version.\n");
	fprintf(stderr, "  devicename      CD-ROM block device name that "
		"contains the CD to be queried.\n");
}

int main(int argc, char *argv[])
{
	int len;
	int i, totaltime;
	long int cksum = 0;
	int musicbrainz = 0;
	unsigned char last = 1;
	char *devicename = DEVICE_NAME;
	struct cdrom_tocentry *TocEntry;
#ifndef __sgi
	int drive;
	struct cdrom_tochdr hdr;
#else
	CDPLAYER *drive;
	CDTRACKINFO info;
	cdrom_tochdr hdr;
#endif
	char *command = argv[0];

#if defined(__OpenBSD__) || defined(__NetBSD__)
	struct ioc_read_toc_entry t;
#elif defined(__APPLE__)
	dk_cd_read_disc_info_t discInfoParams;
#endif

	if (argc == 2 && !strcmp(argv[1], "--help")) {
		usage();
		exit(EXIT_SUCCESS);
	}
	if (argc == 2 && !strcmp(argv[1], "--version")) {
		fprintf(stderr, "cd-discid %s.\n", VERSION);
		exit(EXIT_SUCCESS);
	}
	if (argc >= 2 && !strcmp(argv[1], "--musicbrainz")) {
		musicbrainz = 1;
		argc--;
		argv++;
	}
	if (argc == 2)
		devicename = argv[1];
	else if (argc > 2) {
		usage();
		exit(EXIT_FAILURE);
	}

#if defined(__sgi)
	drive = CDopen(devicename, "r");
	if (drive == 0) {
#else
	drive = open(devicename, O_RDONLY | O_NONBLOCK);
	if (drive < 0) {
#endif
		fprintf(stderr, "%s: %s: ", command, devicename);
		perror("open");
		exit(EXIT_FAILURE);
	}

#if defined(__APPLE__)
	memset(&discInfoParams, 0, sizeof(discInfoParams));
	discInfoParams.buffer = &hdr;
	discInfoParams.bufferLength = sizeof(hdr);
	if (ioctl(drive, DKIOCCDREADDISCINFO, &discInfoParams) < 0
	    || discInfoParams.bufferLength != sizeof(hdr)) {
		fprintf(stderr, "%s: %s: ", command, devicename);
		perror("DKIOCCDREADDISCINFO");
		exit(EXIT_FAILURE);
	}
#elif defined(__sgi)
	if (CDgetstatus(drive, &hdr) == 0) {
		perror("CDROMREADTOCHDR");
		exit(EXIT_FAILURE);
	}
#else
	if (ioctl(drive, CDROMREADTOCHDR, &hdr) < 0) {
		fprintf(stderr, "%s: %s: ", command, devicename);
		perror("CDROMREADTOCHDR");
		exit(EXIT_FAILURE);
	}
#endif

	last = hdr.cdth_trk1;

	len = (last + 1) * sizeof(struct cdrom_tocentry);

	TocEntry = malloc(len);
	if (!TocEntry) {
		fprintf(stderr,
			"%s: %s: Can't allocate memory for TOC entries\n",
			command, devicename);
		exit(EXIT_FAILURE);
	}

#if defined(__OpenBSD__)
	t.starting_track = 0;
#elif defined(__NetBSD__)
	t.starting_track = 1;
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
	t.address_format = CDROM_LBA;
	t.data_len = len;
	t.data = TocEntry;
	memset(TocEntry, 0, len);

	if (ioctl(drive, CDIOREADTOCENTRYS, (char*)&t) < 0) {
		fprintf(stderr, "%s: %s: ", command, devicename);
		perror("CDIOREADTOCENTRYS");
	}
#elif defined(__APPLE__)
	dk_cd_read_track_info_t trackInfoParams;
	memset(&trackInfoParams, 0, sizeof(trackInfoParams));
	trackInfoParams.addressType = kCDTrackInfoAddressTypeTrackNumber;
	trackInfoParams.bufferLength = sizeof(*TocEntry);

	for (i = 0; i < last; i++) {
		trackInfoParams.address = i + 1;
		trackInfoParams.buffer = &TocEntry[i];

		if (ioctl(drive, DKIOCCDREADTRACKINFO, &trackInfoParams) < 0) {
			fprintf(stderr, "%s: %s: ", command, devicename);
			perror("DKIOCCDREADTRACKINFO");
		}
	}

	/* MacOS X on G5-based systems does not report valid info for
	 * TocEntry[last-1].lastRecordedAddress + 1, so we compute the start
	 * of leadout from the start+length of the last track instead
	 */
	TocEntry[last].cdte_track_address = htonl(ntohl(TocEntry[last-1].trackSize) + ntohl(TocEntry[last-1].trackStartAddress));
#elif defined(__sgi)
	for (i = 0; i < last; i++) {
		if (CDgettrackinfo(drive, i + 1, &info) == 0) {
			fprintf(stderr, "CDgettrackinfo failed on track %i: %s\n", i + 1, devicename);
		}
		TocEntry[i].cdte_track_address = info.start_min*60*CD_FRAMES + info.start_sec*CD_FRAMES + info.start_frame;
	}
	TocEntry[last].cdte_track_address = TocEntry[last - 1].cdte_track_address + info.total_min*60*CD_FRAMES + info.total_sec*CD_FRAMES + info.total_frame;
#else   /* FreeBSD, Linux, Solaris */
	for (i = 0; i < last; i++) {
		/* tracks start with 1, but I must start with 0 on OpenBSD */
		TocEntry[i].cdte_track = i + 1;
		TocEntry[i].cdte_format = CDROM_LBA;
		if (ioctl(drive, CDROMREADTOCENTRY, &TocEntry[i]) < 0) {
			fprintf(stderr, "%s: %s: ", command, devicename);
			perror("CDROMREADTOCENTRY");
		}
	}

	TocEntry[last].cdte_track = CDROM_LEADOUT;
	TocEntry[last].cdte_format = CDROM_LBA;
	if (ioctl(drive, CDROMREADTOCENTRY, &TocEntry[i]) < 0) {
		fprintf(stderr, "%s: %s: ", command, devicename);
		perror("CDROMREADTOCENTRY");
	}
#endif

	/* release file handle */
	close(drive);

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__APPLE__)
	TocEntry[i].cdte_track_address = ntohl(TocEntry[i].cdte_track_address);
#endif

	for (i = 0; i < last; i++) {
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__APPLE__)
		TocEntry[i].cdte_track_address = ntohl(TocEntry[i].cdte_track_address);
#endif
		cksum += cddb_sum((TocEntry[i].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);
	}

	totaltime = ((TocEntry[last].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES) -
		((TocEntry[0].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);

	if (musicbrainz) {
		/* print first track number (1), last track number, and lead-out track offset */
		printf("1 %d %d", last, TocEntry[last].cdte_track_address + CD_MSF_OFFSET);
	} else {
		/* print discid, and last track number */
		printf("%08lx %d", (cksum % 0xff) << 24 | totaltime << 8 | last, last);
	}

	/* print frame offsets of all tracks */
	for (i = 0; i < last; i++)
		printf(" %d", TocEntry[i].cdte_track_address + CD_MSF_OFFSET);

	/* print length of disc in seconds */
	if (!musicbrainz)
		printf(" %d", (TocEntry[last].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);

	/* print final new-line */
	printf("\n");

	free(TocEntry);

	return 0;
}
