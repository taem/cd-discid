/*
 * Copyright (c) 1999-2003 Robert Woodcock <rcw@debian.org>
 * This code is hereby licensed for public consumption under either the
 * GNU GPL v2 or greater, or Larry Wall's Artistic license - your choice.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>

/* Porting credits:
 * Solaris: David Champion <dgc@uchicago.edu>
 * FreeBSD: Niels Bakker <niels@bakker.net>
 * OpenBSD: Marcus Daniel <danielm@uni-muenster.de>
 * NetBSD: Chris Gilbert <chris@NetBSD.org>
 * MacOSX: Evan Jones <ejones@uwaterloo.ca> http://www.eng.uwaterloo.ca/~ejones/
 */

#if defined(linux)

#include <linux/cdrom.h>
#define		cdte_track_address	cdte_addr.lba

#elif defined(sun) && defined(unix) && defined(__SVR4)

#include <sys/cdio.h>
#define CD_MSF_OFFSET	150
#define CD_FRAMES	75
/* According to David Schweikert <dws@ee.ethz.ch>, cd-discid needs this
 * to compile on Solaris */
#define cdte_track_address cdte_addr.lba

#elif defined(__FreeBSD__)

#include <sys/cdio.h>
#define        CDROM_LBA       CD_LBA_FORMAT   /* first frame is 0 */
#define        CD_MSF_OFFSET   150     /* MSF offset of first frame */
#define        CD_FRAMES       75      /* per second */
#define        CDROM_LEADOUT   0xAA    /* leadout track */
#define        CDROMREADTOCHDR         CDIOREADTOCHEADER
#define        CDROMREADTOCENTRY       CDIOREADTOCENTRY
#define        cdrom_tochdr    ioc_toc_header
#define        cdth_trk0       starting_track
#define        cdth_trk1       ending_track
#define        cdrom_tocentry  ioc_read_toc_single_entry
#define        cdte_track      track
#define        cdte_format     address_format
#define        cdte_track_address	entry.addr.lba

#elif defined(__OpenBSD__) || defined(__NetBSD__)

#include <sys/cdio.h>
#define        CDROM_LBA       CD_LBA_FORMAT   /* first frame is 0 */
#define        CD_MSF_OFFSET   150     /* MSF offset of first frame */
#define        CD_FRAMES       75      /* per second */
#define        CDROM_LEADOUT   0xAA    /* leadout track */
#define        CDROMREADTOCHDR         CDIOREADTOCHEADER
#define        cdrom_tochdr    ioc_toc_header
#define        cdth_trk0       starting_track
#define        cdth_trk1       ending_track
#define        cdrom_tocentry  cd_toc_entry
#define        cdte_track      track
#define        cdte_track_address       addr.lba

#elif defined(__APPLE__)

#include <sys/types.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#define        CD_FRAMES       75      /* per second */
#define        CD_MSF_OFFSET   150     /* MSF offset of first frame */
#define        cdrom_tochdr    CDDiscInfo
#define        cdth_trk0       numberOfFirstTrack
/* NOTE: Judging by the name here, we might have to do this:
 * hdr.lastTrackNumberInLastSessionMSB << 8 *
 * sizeof(hdr.lastTrackNumberInLastSessionLSB)
 * | hdr.lastTrackNumberInLastSessionLSB; */
#define        cdth_trk1       lastTrackNumberInLastSessionLSB
#define        cdrom_tocentry  CDTrackInfo
#define	       cdte_track_address trackStartAddress

#else
# error "Your OS isn't supported yet."
#endif	/* os selection */

int cddb_sum (int n)
{
	/* a number like 2344 becomes 2+3+4+4 (13) */
	int ret=0;

	while (n > 0) {
		ret = ret + (n % 10);
		n = n / 10;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int len;
	int drive, i, totaltime;
	long int cksum=0;
	unsigned char first=1, last=1;
	struct cdrom_tochdr hdr;
	struct cdrom_tocentry *TocEntry;
#if defined(__OpenBSD__) || defined(__NetBSD__)
	struct ioc_read_toc_entry t;
#elif defined(__APPLE__)
	dk_cd_read_disc_info_t discInfoParams;
#endif

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <devicename>\n", argv[0]);
		exit(1);
	}

	drive = open(argv[1], O_RDONLY | O_NONBLOCK);
	if (drive < 0) {
		fprintf(stderr, "cd-discid: %s: ", argv[1]);
		perror("open");
		exit(1);
	}

#if defined(__APPLE__)
	memset(&discInfoParams, 0, sizeof(discInfoParams));
	discInfoParams.buffer = &hdr;
	discInfoParams.bufferLength = sizeof(hdr);
	if (ioctl(drive, DKIOCCDREADDISCINFO, &discInfoParams) < 0
		|| discInfoParams.bufferLength != sizeof(hdr)) {
		fprintf(stderr, "cd-discid: %s: ", argv[1]);
		perror("DKIOCCDREADDISCINFO");
		exit(1);
	}
#else
	if (ioctl(drive, CDROMREADTOCHDR, &hdr) < 0) {
		fprintf(stderr, "cd-discid: %s: ", argv[1]);
		perror("CDROMREADTOCHDR");
		exit(1);
	}
#endif

	first = hdr.cdth_trk0;
	last = hdr.cdth_trk1;

	len = (last + 1) * sizeof(struct cdrom_tocentry);

	TocEntry = malloc(len);
	if (!TocEntry) {
		fprintf(stderr,
			"cd-discid: %s: Can't allocate memory for TOC entries\n",
			argv[1]);
		exit(1);
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
	
	if (ioctl(drive, CDIOREADTOCENTRYS, (char *) &t) < 0) {
		fprintf(stderr, "cd-discid: %s: ", argv[1]);
		perror("CDIOREADTOCENTRYS");
	}
#elif defined(__APPLE__)
	dk_cd_read_track_info_t trackInfoParams;
	memset( &trackInfoParams, 0, sizeof( trackInfoParams ) );
	trackInfoParams.addressType = kCDTrackInfoAddressTypeTrackNumber;
	trackInfoParams.bufferLength = sizeof( *TocEntry );
        
	for (i = 0; i < last; i++) {
		trackInfoParams.address = i + 1;
		trackInfoParams.buffer = &TocEntry[i];

		if (ioctl(drive, DKIOCCDREADTRACKINFO, &trackInfoParams) < 0) {
			fprintf(stderr, "cd-discid: %s: ", argv[1]);
			perror("DKIOCCDREADTRACKINFO");
		}
	}

	/* MacOS X on G5-based systems does not report valid info for
	 * TocEntry[last-1].lastRecordedAddress + 1, so we compute the start
	 * of leadout from the start+length of the last track instead
	 */
	TocEntry[last].cdte_track_address = TocEntry[last-1].trackSize + TocEntry[last-1].trackStartAddress;
#else /* FreeBSD, Linux, Solaris */
	for (i=0; i < last; i++) {
		/* tracks start with 1, but I must start with 0 on OpenBSD */
		TocEntry[i].cdte_track = i + 1;
		TocEntry[i].cdte_format = CDROM_LBA;
		if (ioctl(drive, CDROMREADTOCENTRY, &TocEntry[i]) < 0) {
			fprintf(stderr, "cd-discid: %s: ", argv[1]);
			perror("CDROMREADTOCENTRY");
		}
	}

	TocEntry[last].cdte_track = CDROM_LEADOUT;
	TocEntry[last].cdte_format = CDROM_LBA;
	if (ioctl(drive, CDROMREADTOCENTRY, &TocEntry[i]) < 0) {
		fprintf(stderr, "cd-discid: %s: ", argv[1]);
		perror("CDROMREADTOCENTRY");
	}
#endif

#if defined(__FreeBSD__)
	TocEntry[i].cdte_track_address = ntohl(TocEntry[i].cdte_track_address);
#endif       

	for (i=0; i < last; i++) {
#if defined(__FreeBSD__)
		TocEntry[i].cdte_track_address = ntohl(TocEntry[i].cdte_track_address);
#endif
		cksum += cddb_sum((TocEntry[i].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);
	}

	totaltime = ((TocEntry[last].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES) -
		    ((TocEntry[0].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);

	/* print discid */
	printf("%08lx", (cksum % 0xff) << 24 | totaltime << 8 | last);

	/* print number of tracks */
	printf(" %d", last);

	/* print frame offsets of all tracks */
	for (i = 0; i < last; i++) {
		printf(" %d", TocEntry[i].cdte_track_address + CD_MSF_OFFSET);
	}
	
	/* print length of disc in seconds */
        printf(" %d\n", (TocEntry[last].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);

        free(TocEntry);

        return 0;
}
