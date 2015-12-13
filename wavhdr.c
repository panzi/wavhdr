#include <stdint.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#define PUT_BYTES(PTR, BYTES, SIZE) \
	memcpy(PTR, BYTES, SIZE); \
	PTR += SIZE;

#define PUT_U16(PTR, VALUE) \
	*(PTR ++) = (uint8_t)((uint16_t)(VALUE) & 0xFF); \
	*(PTR ++) = (uint8_t)(((uint16_t)(VALUE) >>  8) & 0xFF);

#define PUT_U32(PTR, VALUE) \
	*(PTR ++) = (uint8_t)((uint32_t)(VALUE) & 0xFF); \
	*(PTR ++) = (uint8_t)(((uint32_t)(VALUE) >>  8) & 0xFF); \
	*(PTR ++) = (uint8_t)(((uint32_t)(VALUE) >> 16) & 0xFF); \
	*(PTR ++) = (uint8_t)(((uint32_t)(VALUE) >> 24) & 0xFF);

#define RIFF_WAVE_HEADER_SIZE 44
#define DEFAULT_CHANNELS 2
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BITS_PER_SAMPLE 16

static unsigned int to_full_byte(int bits) {
	int rem = bits % 8;
	return rem == 0 ? bits : bits + (8 - rem);
}

static void usage(int argc, char *argv[]) {
	printf("usage: %s [options] <file...>\n", argc > 0 ? argv[0] : "wavhdr");
}

static void help(int argc, char *argv[]) {
	printf("Prepend WAV header to file. Saves result into a new file appending .wav\n\n");
	usage(argc, argv);
	printf("\n"
	       "OPTIONS:\n"
	       "\n"
	       "\t-h, --help                   print this help message\n"
	       "\t-c, --channels=CHANNELS      default: %d\n"
	       "\t-r, --sample-rate=RATE       default: %d\n"
	       "\t-b, --bits-per-sample=BPS    default: %d\n"
	       "\t\n",
	       DEFAULT_CHANNELS, DEFAULT_SAMPLE_RATE, DEFAULT_BITS_PER_SAMPLE);
}

int main(int argc, char *argv[]) {
	char buf[RIFF_WAVE_HEADER_SIZE];
	char *ptr = buf;

	int status = 0;

	long number = 0;
	char *endptr = NULL;

	uint16_t channels        = DEFAULT_CHANNELS;
	uint32_t sample_rate     = DEFAULT_SAMPLE_RATE;
	uint16_t bits_per_sample = DEFAULT_BITS_PER_SAMPLE;

	size_t namesize = 0;
	char *namebuf = NULL;
	int fd_in  = -1;
	int fd_out = -1;
	struct stat st;

	static struct option long_options[] = {
		{"help",            no_argument,       0, 'h'},
		{"channels",        required_argument, 0, 'c'},
		{"sample-rate",     required_argument, 0, 'r'},
		{"bits-per-sample", required_argument, 0, 'b'},
		{0, 0, 0, 0}
	};

	for (;;) {
		int c = getopt_long(argc, argv, "hc:r:b:", long_options, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'h':
			help(argc, argv);
			return 0;

		case 'c':
			endptr = NULL;
			number = strtol(optarg, &endptr, 10);
			if (!*optarg || *endptr) {
				if (errno == 0) {
					errno = EINVAL;
				}
				perror(optarg);
				goto error;
			}
			if (number <= 0 || number > UINT16_MAX) {
				fprintf(stderr, "%s: Number out of range\n", optarg);
				goto error;
			}
			channels = number;
			break;

		case 'r':
			endptr = NULL;
			number = strtol(optarg, &endptr, 10);
			if (!*optarg || *endptr) {
				if (errno == 0) {
					errno = EINVAL;
				}
				perror(optarg);
				goto error;
			}
			if (number <= 0 || number > UINT32_MAX) {
				fprintf(stderr, "%s: Number out of range\n", optarg);
				goto error;
			}
			sample_rate = number;
			break;

		case 'b':
			endptr = NULL;
			number = strtol(optarg, &endptr, 10);
			if (!*optarg || *endptr) {
				if (errno == 0) {
					errno = EINVAL;
				}
				perror(optarg);
				goto error;
			}
			if (number <= 0 || number > UINT16_MAX) {
				fprintf(stderr, "%s: Number out of range\n", optarg);
				goto error;
			}
			bits_per_sample = number;
			break;

		case '?':
			usage(argc, argv);
			return 1;
		}
	}

	const unsigned int ceil_bits_per_sample = to_full_byte(bits_per_sample);
	const unsigned int bytes_per_sample     = ceil_bits_per_sample / 8;

	uint16_t block_align = channels * bytes_per_sample;
	uint32_t byte_rate   = sample_rate * block_align;

	PUT_BYTES(ptr, "RIFF", 4);
	PUT_U32  (ptr, 0);
	PUT_BYTES(ptr, "WAVE", 4);
	PUT_BYTES(ptr, "fmt ", 4);
	PUT_U32  (ptr, 16);
	PUT_U16  (ptr, 1); // PCM
	PUT_U16  (ptr, channels);
	PUT_U32  (ptr, sample_rate);
	PUT_U32  (ptr, byte_rate);
	PUT_U16  (ptr, block_align);
	PUT_U16  (ptr, bits_per_sample);
	PUT_BYTES(ptr, "data", 4);
	PUT_U32  (ptr, 0);

	for (int i = optind; i < argc; ++ i) {
		const char *filename = argv[i];
		size_t outlen = strlen(filename) + 4;
		puts(filename);
		if (namesize < outlen) {
			namebuf  = realloc(namebuf, outlen);
			namesize = outlen;
			if (!namebuf) {
				perror("allocating space for filename");
				goto error;
			}
		}
		strcpy(namebuf, filename);
		strcat(namebuf, ".wav");

		fd_in = open(filename, O_RDONLY);
		if (fd_in < 0) {
			perror(filename);
			goto error;
		}

		if (fstat(fd_in, &st) == -1) {
			perror(filename);
			goto error;
		}

		if (st.st_size > UINT32_MAX - RIFF_WAVE_HEADER_SIZE) {
			fprintf(stderr, "%s: File too large\n", filename);
			goto error;
		}

		fd_out = open(namebuf, O_CREAT | O_WRONLY, st.st_mode);
		if (fd_in < 0) {
			perror(namebuf);
			goto error;
		}

		ptr = buf + 4;
		PUT_U32(ptr, 36 + st.st_size);
		ptr = buf + RIFF_WAVE_HEADER_SIZE - 4;
		PUT_U32(ptr, st.st_size);

		if (write(fd_out, buf, RIFF_WAVE_HEADER_SIZE) != RIFF_WAVE_HEADER_SIZE) {
			perror(namebuf);
			goto error;
		}

		if (st.st_size > 0 && sendfile(fd_out, fd_in, NULL, st.st_size) != st.st_size) {
			perror(namebuf);
			goto error;
		}

		close(fd_out);
		close(fd_in);
	}

	goto end;

error:
	status = 1;

end:

	if (fd_in >= 0) {
		close(fd_in);
	}

	if (fd_out >= 0) {
		close(fd_out);
	}

	if (namebuf) {
		free(namebuf);
	}

	return status;
}
