#include <stdint.h>
#include <stdio.h>

static struct {uint8_t buf[1024 * 1024];} lcdpi_spiblock_dma;

static void lcdpi_compress(int *ptr, unsigned short *pixel_buffer, int pixel_buffer_len, unsigned short *codes, unsigned short speedup)
{
	int j;
	int x;
	unsigned short value, last_value, mask;
	int repeated;

	mask = (codes[0] & 0x8000) ^ 0x8000;
	value = (((pixel_buffer[0] & 0xffc0) >> 1) + (pixel_buffer[0] & 0x001f)) | mask;
	lcdpi_spiblock_dma.buf[((*ptr) * 2) + 1] = (value >> 8);
	lcdpi_spiblock_dma.buf[((*ptr) * 2) + 2] = (value) & 0xff;
	(*ptr)++;
	last_value = value;
	repeated = 0;
	for (x = 1; x < pixel_buffer_len; x++) {
		value = (((pixel_buffer[x] & 0xffc0) >> 1) + (pixel_buffer[x] & 0x001f)) | mask;
		if (value != last_value) {
			for (j = 0; j < (repeated / speedup); j++) {
				lcdpi_spiblock_dma.buf[((*ptr) * 2) + 1] = codes[speedup] >> 8;
				lcdpi_spiblock_dma.buf[((*ptr) * 2) + 2] = codes[speedup] &0xff;
				(*ptr)++;
			}

			if(repeated % speedup) {
				lcdpi_spiblock_dma.buf[((*ptr) * 2) + 1] = codes[repeated % speedup] >> 8;
				lcdpi_spiblock_dma.buf[((*ptr) * 2) + 2] = codes[repeated % speedup] &0xff;
				(*ptr)++;
			}

			lcdpi_spiblock_dma.buf[((*ptr) * 2) + 1] = (value >> 8);
			lcdpi_spiblock_dma.buf[((*ptr) * 2) + 2] = (value) & 0xff;
			(*ptr)++;
			last_value = value;
			repeated = 0;
		} else {
			repeated++;
		}
	}
	for (j = 0; j < repeated / speedup; j++) {
		lcdpi_spiblock_dma.buf[((*ptr) * 2) + 1] = codes[speedup] >> 8;
		lcdpi_spiblock_dma.buf[((*ptr) * 2) + 2] = codes[speedup] &0xff;
		(*ptr)++;
	}

	if(repeated % speedup) {
		lcdpi_spiblock_dma.buf[((*ptr) * 2) + 1] = codes[repeated % speedup] >> 8;
		lcdpi_spiblock_dma.buf[((*ptr) * 2) + 2] = codes[repeated % speedup] &0xff;
		(*ptr)++;
	}
}

int main()
{
    int ptr = 0;
    static uint8_t pb[1024 * 1024] __attribute__ ((aligned (2)));
    int pbl = 0;
    int c;
    while((c = getchar()) != EOF) {
        pb[pbl++] = c;
    }
    lcdpi_spiblock_dma.buf[0] = 0x42;
    unsigned short codes[] = {0xaaab, 0x80ff, 0x8cff, 0x8ccf, 0x98c9, 0xaabf, 0xaaaf, 0xaaab};
    for (int i = 0 ; i < 1000 ; i++) {
        ptr = 0;
        lcdpi_compress(&ptr, (unsigned short *) pb, pbl / 2, codes, 7);
    }
    fwrite(lcdpi_spiblock_dma.buf, 1, ptr * 2 + 1, stdout);
}
