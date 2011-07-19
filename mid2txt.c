/*
	mid2txt converter:
	author: haru

	09/04/05-09/04/07 ver 0.1
	09/09/29-09/10/01 ver 0.2 rest note at start of mid
	09/10/15 ver 0.25 error massage / usage fix
	11/07/19 code refine / !remove ver 0.2 rest note fix

	usage: mid2txt <input file>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

/* define */
enum {
	DEBUG = 1,
	BUFSIZE = 8192,
	LISTNUM = 16, /* max midi ch? */
};

typedef enum {HEADER = 0, TRACK, DELTA, MIDI, SYSEX, META} state;

/* struct */
struct hdinfo {
	unsigned int datalen;
	unsigned char format;
	unsigned char tracknum;
	unsigned int division;
};

struct note_header { /* doubly-linked list */
	unsigned char data[5]; /* ch, note number and velocity / st flag / gt flag */
	unsigned int st; /* step time */
	unsigned int gt; /* gate time */
	struct note_header *next;
	struct note_header *prev;
};

/* function */
void getheader();
unsigned int gettrack();
unsigned int getdelta();
void getmidi();
void getsysex();
state getmeta();

state checkmsg();
void on_note(unsigned char status);
void off_note(unsigned char status);

void init();
void destroy();

/* global variable */
struct hdinfo hd;
unsigned char *ptr_buf; /* pointer to reading buffer */
struct note_header list_head[LISTNUM]; /* list header and pointer to note list */
unsigned char prev_status[2]; /* store status byte / runnning status flag */

int main(int argc, char *argv[], char *env[])
{
	/* variable */
	int fd; /* file discripter */
	int n = 0; /* number of read bytes */
	char buf[BUFSIZE]; /* buffer */
	int state = HEADER; /* reading status */
	int i = 0;
	unsigned int tracklen, deltatime;

	/* check argument */
	if (argc < 2) {
		fprintf(stderr, "fewer arguments.\n");
		fprintf(stderr, "usage: mid2txt <midi file>\n");
		exit(EXIT_FAILURE);
	}
	else {
		/* open file */
		if ((fd = open(argv[1], O_RDONLY)) < 0) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		else {
			/* initialize note list */
			init();

			/* init previous status(for running status) */
			prev_status[0] = 0;
			prev_status[1] = 0;

			/* read file */
			while ((n = read(fd, buf, BUFSIZE)) > 0) {
				ptr_buf = &buf[0];
				const unsigned char * const endp = buf + n;

				/* read per 1 byte */
				while (ptr_buf < endp) {
					switch (state) {
					case HEADER:
						/* get datalen, format, tracknum, division */
						getheader();
						state = TRACK;
						break;
					case TRACK:
						/* destroy note list */
						destroy();

						/* get track datalen */
						tracklen = gettrack();
						state = DELTA;
						break;
					case DELTA:
						/* calc deltatime */
						deltatime = getdelta();
						/* check next message type(midi or sysex or meta) */
						state = checkmsg();
						break;
					case MIDI:
						getmidi();
						state = DELTA;
						break;
					case SYSEX:
						getsysex();
						state = DELTA;
						break;
					case META:
						state = getmeta();
						break;
					default:
						fprintf(stderr, "unknown state! exit...\n");
						exit(EXIT_FAILURE);
					}
				}
			}
			/* destroy note list */
			destroy();

			/* close file */
			if (close(fd) < 0) {
				perror("close");
				exit(EXIT_FAILURE);
			}
		}
	}

	return 0;
}

void getheader()
{
	/* skip "MThd" */
	ptr_buf += 4;

	/* get data length */
	ptr_buf += 3;
	hd.datalen = *ptr_buf++;

	/* get smf format */
	ptr_buf += 1;
	hd.format = *ptr_buf++;

	/* get track number */
	ptr_buf += 1;
	hd.tracknum = *ptr_buf++;

	/* get division */
	hd.division = *ptr_buf++;
	hd.division <<= 8;
	hd.division |= *ptr_buf++;

	if (DEBUG) {
		fprintf(stderr, "--- smf header ---\n");
		fprintf(stderr, "datalen:%d\nformat:%d\ntracknum:%d\ndivision:%d\n", \
			hd.datalen, hd.format, hd.tracknum, hd.division);
	}
}

state checkmsg()
{
	if (*ptr_buf & 0x80) { // whether status byte or not
	if (*ptr_buf <= 0xEF) { // this is MIDI Ivent
		if (DEBUG)
			fprintf(stderr, "midi! --> ");
		return MIDI;
	}
	else if (*ptr_buf == 0xFF) { // SMF Meta Ivent
		if (DEBUG)
			fprintf(stderr, "meta! --> ");
		return META;
	}
	else if (*ptr_buf == 0xF0 || *ptr_buf == 0xF7) { // SysEx Message
		if (DEBUG)
			fprintf(stderr, "sysex! -> ");
		return SYSEX;
	}
	else
		fprintf(stderr, "unknown message.\n");
	}
	else {
		if (!(prev_status[0])) // if status == 0
			fprintf(stderr, "cant find 'status byte'.\n");
		else {
			if (DEBUG)
				fprintf(stderr, "assume 'running status' is used.\n");
			prev_status[1] = 1;
			return MIDI;
		}
	}

	return -1;
}

unsigned int gettrack()
{
	unsigned int datalen = 0;
	int i = 0;

	/* skip "MTrk" */
	ptr_buf += 4;

	/* get track data length */
	while (i < 4) {
		if (i != 0)
			datalen <<= 8;
		datalen |= *ptr_buf++;
		i++;
	}

	if (DEBUG) {
		fprintf(stderr, "--- smf header ---\n");
		fprintf(stderr, "datalen:%d\n", datalen);
	}

	return datalen;
}

unsigned int getdelta()
{
	unsigned int delta = 0;
	struct note_header *lp, *hd;
	int i;

	while (*ptr_buf & 0x80) { // MSB == 0?
		delta |= (0x7f & *ptr_buf++);
		delta <<= 7;
	}
	delta |= (0x7f & *ptr_buf++);

	if (DEBUG) {
		fprintf(stderr, "--- smf ivent ---\n");
		fprintf(stderr, "delta:%d\n", delta);
	}

	/* remove note from list */
	i = 0;

	while (i < LISTNUM) {
		hd = &list_head[i];
		for (lp = hd->next; lp != hd; lp = lp->next) {
			if ((lp->data[3]) && (lp->data[4])) {
				if (DEBUG)
					fprintf(stderr, "del! st:%d gt:%d (ch:%d note:%d)\n", lp->st, lp->gt, lp->data[0], lp->data[1]);
				fprintf(stdout, "%d %d %d %d %d\n", lp->data[0], lp->data[1], lp->st, lp->gt, lp->data[2]);

				/* delete note from note list 

					hd ... -> lp -> ... -> hd
					hd ... -> (del) -> ... -> hd
				*/
				(lp->prev)->next = lp->next;
				(lp->next)->prev = lp->prev;

				free(lp);
			}
		}
		i++;
	}

	/* add st / gt if there is on_note in list */
	i = 0;

	while (i < LISTNUM) {
		hd = &list_head[i];
		for (lp = hd->next; lp != hd; lp = lp->next) {
			if (!(lp->data[3])) {
				lp->st += delta;
				if (DEBUG)
					fprintf(stderr, "\tadd st! st:%d gt:%d (ch:%d note:%d)\n", lp->st, lp->gt, lp->data[0], lp->data[1]);
			}
			if (!(lp->data[4])) {
				lp->gt += delta;
				if (DEBUG)
					fprintf(stderr, "\tadd gt! st:%d gt:%d (ch:%d note:%d)\n", lp->st, lp->gt, lp->data[0], lp->data[1]);
			}
		}
		i++;
	}

	/* rest note */
	i = 0;

	int count = 0;

	while (i < LISTNUM) {
		hd = &list_head[i];
		if (hd == hd->next)
			count++;
		i++;
	}

	if (count == LISTNUM && delta != 0) {
		fprintf(stderr, "rest delta:%d\n", delta);
		#fprintf(stdout, "0 0 %d 0 0\n", delta);
	}

	return delta;
}

void getmidi()
{
	unsigned char status;

	if (prev_status[1]) { /* runnning status */
		status = prev_status[0];
		prev_status[1] = 0;
	}
	else {
		prev_status[0] = status = *ptr_buf;
		ptr_buf++;
	}

	if (status>>4 == 0x9) { /* note on message */
		if (*(ptr_buf + 1) == 0) {
			if (DEBUG)
				fprintf(stderr, "note off(0x9)\n");
			off_note(status);
			ptr_buf += 2;

		}
		else {
			if (DEBUG)
				fprintf(stderr, "note on\n");
			on_note(status);
			ptr_buf += 2;
		}
	}
	else if (status>>4 == 0x8) { // note on message
		if (DEBUG)
			fprintf(stderr, "note off(0x8)\n");
		off_note(status);
		ptr_buf += 2;
	}
	else if (status>>4 == 0xb) { // control change
		if (DEBUG)
			fprintf(stderr, "control change ch:%x cc:%x val:%x\n", *(ptr_buf - 1) & 0x0f, *ptr_buf, *(ptr_buf + 1));
		ptr_buf += 2;
	}
	else if (status>>4 == 0xc) { // program change
		if (DEBUG)
			fprintf(stderr, "program change ch:%x ptr_bufc:%x\n", *(ptr_buf - 1) & 0x0f, *ptr_buf);
		ptr_buf++;
	}
}

void getsysex()
{
	/* reset running status */
	prev_status[0] = 0;
	prev_status[1] = 0;

	unsigned int datalen = 0;

	if (DEBUG)
		fprintf(stderr, "sysex message(only skip)\n");
	ptr_buf++;

	while (*ptr_buf & 0x80) { /* whether MSB == 0 or not */
		datalen |= (0x7f & *ptr_buf++);
		datalen <<= 7;
	}
	datalen |= (0x7f & *ptr_buf++);

	ptr_buf += datalen;
}

state getmeta()
{
	/* reset running status */
	prev_status[0] = 0;
	prev_status[1] = 0;

	/* skip "0xFF" */
	ptr_buf += 1;

	if (*ptr_buf == 0x2F && *(ptr_buf + 1) == 0x00) {
		if (DEBUG)
			fprintf(stderr, "track end\n");
		ptr_buf += 2;

		return TRACK;
	}
	else {
		unsigned int datalen = 0;

		if (DEBUG)
			fprintf(stderr, "smf meta type:%x len:%x\n", *ptr_buf, *(ptr_buf + 1));
		ptr_buf++;

		while (*ptr_buf & 0x80) { /* whether MSB == 0 or not */
			datalen |= (0x7f & *ptr_buf++);
			datalen <<= 7;
		}
		datalen |= (0x7f & *ptr_buf++);
		ptr_buf += datalen;

		return DELTA;
	}
}

void init()
{
	int i = 0;

	/* init note list */
	while (i < LISTNUM) {
		list_head[i].prev = &list_head[i];
		list_head[i].next = &list_head[i];
		i++;
	}
}

void on_note(unsigned char status)
{
	struct note_header *lp, *hd, *ap;
	int ch = (0x0f & status), i = 0;
	hd = &list_head[ch];

	// set st flag
	while (i < LISTNUM) {
		hd = &list_head[i];
		for (lp = hd->next; lp != hd; lp = lp->next) {
			if (DEBUG)
				fprintf(stderr, "\tset st flag! st:%d gt:%d (ch:%d note:%d)\n", lp->st, lp->gt, lp->data[0], lp->data[1]);
			lp->data[3] = 1;
		}
		i++;
	}

	/* add new note to note list */
	for (lp = hd; ; lp = lp->next) {
		if (lp->next == hd) {
			/*
			   hd ... -> lp -> ap -> hd
			*/
			ap = malloc(sizeof(struct note_header));
			ap->next = hd;
			ap->prev = lp;
			lp->next = ap;

			/* set note info */
			ap->data[0] = ch; /* ch */
			ap->data[1] = *ptr_buf; /* note num */
			ap->data[2] = *(ptr_buf + 1); /* velocity */

			/* init */
			ap->st = 0;
			ap->gt = 0;
			ap->data[3] = 0;
			ap->data[4] = 0;

			if (DEBUG)
				fprintf(stderr, "(ch:%d note:%d vel:%d)\n", ap->data[0], ap->data[1], ap->data[2]);
			break;
		}
	}
}

void off_note(unsigned char status)
{
	struct note_header *lp, *hd;
	int i = 0;
	unsigned char ch = (0x0f & status), note = *ptr_buf, vel = *(ptr_buf + 1);

	if (DEBUG)
		fprintf(stderr, "(ch:%d note:%d vel:%d)\n", ch, note, vel);

	/* set gt flag */
	while (i < LISTNUM) {
		hd = &list_head[i];
		for (lp = hd->next; lp != hd; lp = lp->next) {
			if (ch == lp->data[0] && note == lp->data[1]) {
				if (DEBUG)
					fprintf(stderr, "\tset gt flag! st:%d gt:%d (ch:%d note:%d)\n", lp->st, lp->gt, lp->data[0], lp->data[1]);
				lp->data[4] = 1;
			}
		}
		i++;
	}
}

void destroy()
{
	struct note_header *lp, *hd;
	int i;

	/* delete note from list if note exists */
	i = 0;
	while (i < LISTNUM) {
		hd = &list_head[i];
		for (lp = hd->next; lp != hd; lp = lp->next) {
			if ((lp->data[4])) {
				if (DEBUG) {
					fprintf(stderr, "\tdestroy! st:%d gt:%d (ch:%d note:%d)\n", lp->gt, lp->gt, lp->data[0], lp->data[1]);
					fprintf(stderr, "%d %d %d %d %d\n", lp->data[0], lp->data[1], lp->gt, lp->gt, lp->data[2]);
				}

				/* delete note from note list
					hd ... -> lp -> ... -> hd
					hd ... -> (del) -> ... -> hd
				*/
				(lp->prev)->next = lp->next;
				(lp->next)->prev = lp->prev;

				free(lp);
			}
		}
		i++;
	}

	/* search note remaining in list */
	i = 0;
	while (i < LISTNUM) {
		hd = &list_head[i];
		for (lp = hd->next; lp != hd; lp = lp->next) {
			if (DEBUG)
				fprintf(stderr, "unreleased note --> st:%d gt:%d (ch:%d note:%d)\n", lp->st, lp->gt, lp->data[0], lp->data[1]);
			free(lp);
		}
		i++;
	}
}
