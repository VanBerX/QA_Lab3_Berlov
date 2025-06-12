#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifndef IMGSIZE_DEMO_MAIN
#include "imgsize.h"
#endif

enum { min_size = 24, jpeg_chunk_hdr = 12 };

int extract_image_dimensions(const char *fn, int *width, int *height)
{
    int fd, size, rc;
    unsigned char buf[min_size];

    fd = open(fn, O_RDONLY);
    if(fd == -1)
        return 0;
    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if(size < min_size) {
        close(fd);
        return 0;
    }

    /* GIF dimensions are within the first 10 bytes of the file;
       PNG dimensions are within the first 24 bytes of the file;
       JPEG dimensions are computed by scanning through jpeg chunks.
       In all the three formats, the file is at least 24 bytes long,
       so we'll read these 24 bytes
     */
    rc = read(fd, buf, min_size);
    if(rc != min_size) {
        close(fd);
        return 0;
    }

    /* now we detect what it is */

    if(buf[0]=='G' && buf[1]=='I' && buf[2]=='F') {
        /* probably this is GIF */
        close(fd);

        /* The first 3 bytes are the signature ("GIF"),
           the next 3 bytes is the version number.
           Bytes 6,7 and 8,9 are the dimensions, little-endian byte-order
         */
        *width = buf[6] + (buf[7]<<8);
        *height = buf[8] + (buf[9]<<8);
        return 1;
    }

    if(buf[0]==0x89 && buf[1]=='P' && buf[2]=='N' && buf[3]=='G' &&
        buf[4]==0x0D && buf[5]==0x0A && buf[6]==0x1A && buf[7]==0x0A &&
        buf[12]=='I' && buf[13]=='H' && buf[14]=='D' && buf[15]=='R')
    {
        /* looks like PNG */
        close(fd);

        /* PNG: the first frame must be the IHDR frame, which stores
           dimensions, as 4-byte integers, big-endian byte order
         */
        *width = (buf[16]<<24) + (buf[17]<<16) + (buf[18]<<8) + buf[19];
        *height = (buf[20]<<24) + (buf[21]<<16) + (buf[22]<<8) + buf[23];
        return 1;
    }

    if(buf[0]==0xFF && buf[1]==0xD8 && buf[2]==0xFF) {
        /* perhaps it's a jpeg */

        long pos = 2;

        /* now we need to follow the sequence of chunks, looking for the
           one belonging to the SOFn family, which holds the dimensions.
           We never need more than 12 bytes of a chunk; for the first
           one (after that FFD8), it is already in memory, simply shift it
         */
        memmove(buf, buf+2, jpeg_chunk_hdr);

        for(;;) {
            int csz;

            if(buf[0] != 0xFF) {
                /* the file is broken */
                close(fd);
                return 0;
            }
            if(buf[1] >= 0xC0 && buf[1] <= 0xCF &&
                buf[1] != 0xC4 && buf[1] != 0xC8 && buf[1] != 0xCC)
            {
                /* here is the SOFn! */
                close(fd);  /* we've read enough */
                  /* the dims are 16-bit, big-endian, at buf+5, buf+7 */
                *height = (buf[5]<<8) + buf[6];
                *width = (buf[7]<<8) + buf[8];
                return 1;
            }
            /* chunk payload length is in 2nd and 3rd bytes of the chunk,
               big endian, but not always...
             */
            if(buf[1] >= 0xD0 && buf[1] <= 0xD9)
                csz = 0;  /* RSTn, SOI, EOI */
            else
            if(buf[1] == 0xDD)
                csz = 4;  /* DRI */
            else
                csz = (buf[2]<<8)+buf[3];
            if(pos+12 > size)
                break;
            pos += 2 + csz;
            lseek(fd, pos, SEEK_SET);
            rc = read(fd, buf, jpeg_chunk_hdr);
            if(rc != jpeg_chunk_hdr) {
                  /* if there's less than that up to the end of file
                     and we still haven't got the SOFn, we will not.
                   */
                close(fd);
                return 0;
            }
        }
    }

    close(fd);
    return 0;
}

#ifdef IMGSIZE_DEMO_MAIN

#include <stdio.h>

int main(int argc, const char *const *argv)
{
    int i, res, h, w, success;
    success = 1;
    for(i = 1; i < argc; i++) {
        res = extract_image_dimensions(argv[i], &w, &h);
        if(res) {
            printf("%s:\t\t%dx%d\n", argv[i], w, h);
        } else {
            printf("%s: ERROR\n", argv[i]);
            success = 0;
        }
    }
    return success ? 0 : 1;
}

#endif
