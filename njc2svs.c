/*
	mid2txt midi to text converter:
	author: haru
	usage: mid2txt <input file>

	11/07/21 rerewrite
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* define */
enum {
	HEADER = 0,
	TRACK,
	DELTA,
	MIDI,
	SYSEX,
	META,
};

enum {
	BUFSIZE = 128,
};

/* struct */
struct song_info {
} song_info;

/* function */
int check_type()
{
}

/* global variable */

int main(int argc, char *argv[])
{
	/* variable */
	FILE *fp;
	int state = HEADER;
	unsigned char buf[BUFSIZE];

	/* check argument and open file */
	if (argc < 2)
		fp = stdin;
	else {
		if ((fp = fopen(argv[1], "r")) == NULL) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}
	}

	/* main loop */
	while (fread(buf, 1, 1, fp) > 0) {
		switch(check_type(buf)) {
			case HEADER:
				break;
			case TRACK:
				break;
			case DELTA:
				break;
			case MIDI:
				break;
			case SYSEX:
				break;
			case META:
				break;
			default:
				fprintf(stderr, "unknown message type\n");
				exit(EXIT_FAILURE);
				break;
		}
	}
}
