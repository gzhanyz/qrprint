#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qrcodegen.h"

// Function prototypes
static void printQr(const uint8_t qrcode[]);
static int doSegment(uint8_t *bytes, size_t size, char *title);
static void drawQr(char *filename, const uint8_t qrcode[]);

#define SEGMENT_SIZE 2300 // Medium level Error correction

// The main application program.
int main(int argc, char *argv[])
{
    FILE *fptr;
    uint8_t buffer[SEGMENT_SIZE];

    if (argc != 2)
    {
        printf("usage %s filename\n", argv[0]);
        exit(-1);
    }

    if ((fptr = fopen(argv[1], "rb")) == NULL)
    {
        printf("Error! opening file");

        // Program exits if the file pointer returns NULL.
        exit(1);
    }

    int i = 0;
    while (!feof(fptr))
    {
        int n = fread(buffer, 1, sizeof(buffer), fptr);
        printf("read %d bytes", n);
        char title[80];
        sprintf(title, "SEG%d-%d", i, n);
        if (n > 0)
        {
            doSegment(buffer, n, title);
        }
        i++;
    }
    fclose(fptr);

    return 0;
}

static int doSegment(uint8_t *bytes, size_t size, char *title)
{
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
    uint8_t *segBuf0 = malloc(qrcodegen_calcSegmentBufferSize(qrcodegen_Mode_BYTE, size) * sizeof(uint8_t));
    uint8_t *segBuf1 = malloc(qrcodegen_calcSegmentBufferSize(qrcodegen_Mode_ALPHANUMERIC, strlen(title)) * sizeof(uint8_t));
    struct qrcodegen_Segment segs[] = {
        qrcodegen_makeBytes(bytes, size, segBuf0),
        qrcodegen_makeAlphanumeric(title, segBuf1),
    };
    bool ok = qrcodegen_encodeSegments(segs, sizeof(segs) / sizeof(segs[0]), qrcodegen_Ecc_LOW, tempBuffer, qrcode);
    free(segBuf0);
    free(segBuf1);
    if (ok) {
        printQr(qrcode);
        char filename[80];
        sprintf(filename, "%s.bmp", title);
        drawQr(filename, qrcode);
    }
}

/*---- Utilities ----*/

// Prints the given QR Code to the console.
static void printQr(const uint8_t qrcode[])
{
    int size = qrcodegen_getSize(qrcode);
    int border = 4;
    for (int y = -border; y < size + border; y++)
    {
        for (int x = -border; x < size + border; x++)
        {
            fputs((qrcodegen_getModule(qrcode, x, y) ? "##" : "  "), stdout);
        }
        fputs("\n", stdout);
    }
    fputs("\n", stdout);
}

static void drawQr(char *filename, const uint8_t qrcode[])
{
    int size = qrcodegen_getSize(qrcode);
    int border = 4;
    int WIDTH = (size + border*2)*2;
    int HEIGHT = WIDTH;

    unsigned int headers[13];
    FILE *outfile;
    int extrabytes;
    int paddedsize;
    int x;
    int y;
    int n;
    int red, green, blue;

    extrabytes = 4 - ((WIDTH * 3) % 4); // How many bytes of padding to add to each
                                        // horizontal line - the size of which must
                                        // be a multiple of 4 bytes.
    if (extrabytes == 4)
        extrabytes = 0;

    paddedsize = ((WIDTH * 3) + extrabytes) * HEIGHT;

    // Headers...
    // Note that the "BM" identifier in bytes 0 and 1 is NOT included in these "headers".

    headers[0] = paddedsize + 54; // bfSize (whole file size)
    headers[1] = 0;               // bfReserved (both)
    headers[2] = 54;              // bfOffbits
    headers[3] = 40;              // biSize
    headers[4] = WIDTH;           // biWidth
    headers[5] = HEIGHT;          // biHeight

    // Would have biPlanes and biBitCount in position 6, but they're shorts.
    // It's easier to write them out separately (see below) than pretend
    // they're a single int, especially with endian issues...

    headers[7] = 0;          // biCompression
    headers[8] = paddedsize; // biSizeImage
    headers[9] = 0;          // biXPelsPerMeter
    headers[10] = 0;         // biYPelsPerMeter
    headers[11] = 0;         // biClrUsed
    headers[12] = 0;         // biClrImportant

    outfile = fopen(filename, "wb");

    //
    // Headers begin...
    // When printing ints and shorts, we write out 1 character at a time to avoid endian issues.
    //

    fprintf(outfile, "BM");

    for (n = 0; n <= 5; n++)
    {
        fprintf(outfile, "%c", headers[n] & 0x000000FF);
        fprintf(outfile, "%c", (headers[n] & 0x0000FF00) >> 8);
        fprintf(outfile, "%c", (headers[n] & 0x00FF0000) >> 16);
        fprintf(outfile, "%c", (headers[n] & (unsigned int)0xFF000000) >> 24);
    }

    // These next 4 characters are for the biPlanes and biBitCount fields.

    fprintf(outfile, "%c", 1);
    fprintf(outfile, "%c", 0);
    fprintf(outfile, "%c", 24);
    fprintf(outfile, "%c", 0);

    for (n = 7; n <= 12; n++)
    {
        fprintf(outfile, "%c", headers[n] & 0x000000FF);
        fprintf(outfile, "%c", (headers[n] & 0x0000FF00) >> 8);
        fprintf(outfile, "%c", (headers[n] & 0x00FF0000) >> 16);
        fprintf(outfile, "%c", (headers[n] & (unsigned int)0xFF000000) >> 24);
    }

    //
    // Headers done, now write the data...
    //

    for (y = HEIGHT - 1; y >= 0; y--) // BMP image format is written from bottom to top...
    {
        for (x = 0; x <= WIDTH - 1; x++)
        {
            // COVERT BMP coordinate to QR
            int x_qr, y_qr;
            x_qr = (x/2) - 4;
            y_qr = (y/2) - 4;
            bool black = qrcodegen_getModule(qrcode, x_qr, y_qr);
            if (black) {
                red = 0;
                green = 0;
                blue = 0;
            } else {
                red = 255;
                green = 255;
                blue = 255;
            }
            // Also, it's written in (b,g,r) format...

            fprintf(outfile, "%c", blue);
            fprintf(outfile, "%c", green);
            fprintf(outfile, "%c", red);
        }
        if (extrabytes) // See above - BMP lines must be of lengths divisible by 4.
        {
            for (n = 1; n <= extrabytes; n++)
            {
                fprintf(outfile, "%c", 0);
            }
        }
    }

    fclose(outfile);
    return;
}