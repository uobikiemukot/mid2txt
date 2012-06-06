#include <iostream> /* cerr, cin, cout... */
#include <fstream> /* ifstream, ofstream */
#include <map> /* map */
#include <vector> /* vector */
#include <string> /* string */
#include <cstdlib> /* exit */

using namespace std;

enum midi_state {
	/* state */
	HEADER = 0,
	TRACK,
	DELTA,
	MIDI,
	SYSEX,
	META,
	UNKNOWN,
};

enum byte_flag {
	/* delta */
	DELTA_BITS_PER_BYTE = 7,
	DELTA_CONTINUE_FLAG = 0x80,
	/* cmd */
	MIDI_BYTE = 0xEF, /* maximum byte of MIDI Ivent */
	SYSEX_BYTE_F0 = 0xF0,
	SYSEX_BYTE_F7 = 0xF7,
	META_BYTE = 0xFF,
	/* midi ivent */
	NOTE_OFF = 0x80,
	NOTE_ON = 0x90,
	POLY_PRESS = 0xA0,
	CTRL_CHANGE = 0xB0,
	PROG_CHANGE = 0xC0,
	CH_PRESS = 0xD0,
	PITCH_BEND = 0xE0,
	/* status byte */
	STATUS_BYTE_FLAG = 0x80,
	STATUS_RESET = 0x00, /* for RUNNING STATUS */
	/* meta ivent */
	TRACK_END = 0x2F,
	/* note mask */
	CHANNEL_MASK = 0x0F,
};

enum error {
	HEADER_ERROR = 1,
	TRACK_ERROR,
	DELTA_ERROR,
	MIDI_ERROR,
	META_ERROR,
	EOF_ERROR,
	CHECK_ERROR,
	UNKNOWN_ERROR,
};

enum {
	/* misc */
	BITS_PER_BYTE = 8,
	DEBUG = 1,
};

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned short u16;
typedef struct note note;
typedef struct midi_header midi_header;
typedef struct midi_track midi_track;
typedef map<u16, note> note_map;

struct midi_header {
	string str; /* MThd */
	u32 size; /* = 6 */
	u16 format;
	u16 track_num;
	u16 division;
};

struct midi_track {
	string str; /* MTrk */
	u32 size;
};

struct note {
	u8 channel;
	u8 pitch;
	u8 velocity;
	u32 duration;
};

u8 prev_status = STATUS_RESET; /* for running status */

/* other functions must use get_n* function when getting new data */
string get_nstr(ifstream &ifs, int length)
{
	int i;
	string str;

	for (i = 0; i < length; i++) {
		if (ifs.eof()) {
			cerr << "unexpected EOF:" << i << endl;
			exit(EOF_ERROR);
		}
		str += ifs.get();
	}

	return str;
}

u32 get_nbyte(ifstream &ifs, int length)
{
	int i;
	u32 ret = 0;

	for (i = 1; i <= length; i++) {
		if (ifs.eof()) {
			cerr << "unexpected EOF:" << i << endl;
			exit(EOF_ERROR);
		}
		ret += ifs.get() << ((length - i) * BITS_PER_BYTE);
	}

	return ret;
}

u8 peek_byte(ifstream &ifs)
{
	u8 c;

	c = ifs.peek();

	if (ifs.eof()) {
		cerr << "unexpected EOF:" << endl;
		exit(EOF_ERROR);
	}

	return c;
}

void check_header(ifstream &ifs, midi_header &header)
{
	header.str = get_nstr(ifs, 4);
	if (header.str != "MThd") {
		cerr << "header string is not 'MThd'" << endl;
		exit(HEADER_ERROR);
	}

	header.size = get_nbyte(ifs, 4);
	header.format = get_nbyte(ifs, 2);
	header.track_num = get_nbyte(ifs, 2);
	header.division = get_nbyte(ifs, 2);

	if (header.format < 0 || header.format > 2) {
		cerr << "header format is not valid(format < 0 or format > 2)" << endl;
		exit(HEADER_ERROR);
	}

	if (DEBUG) {
		cerr << "header info:"  << endl;
		cerr << "\tstring: "    << header.str       << endl;
		cerr << "\tsize: "      << header.size      << endl;
		cerr << "\tformat: "    << header.format    << endl;
		cerr << "\ttrack_num: " << header.track_num << endl;
		cerr << "\tdivision: "  << header.division  << endl;
	}
}

void check_track(ifstream &ifs, vector<midi_track> &tracks)
{
	midi_track track;

	track.str = get_nstr(ifs, 4);

	if (track.str != "MTrk") {
		cerr << "track string is not 'MTrk'" << endl;
		exit(TRACK_ERROR);
	}

	track.size = get_nbyte(ifs, 4);

	if (DEBUG) {
		cerr << "track info:" << endl;
		cerr << "\tstring: "  << track.str  << endl;
		cerr << "\tsize: "    << track.size << endl;
	}

	tracks.push_back(track);
}

note get_note(ifstream &ifs)
{
	int i;
	u8 c;
	note n;

	for (i = 0; i < 3; i++) {
		c = get_nbyte(ifs, 1);
		switch(i) {
		case 0:
			n.channel = c & CHANNEL_MASK;
			break;
		case 1:
			n.pitch = c;
			break;
		case 2:
			n.velocity = c;
			break;
		default:
			break;
		}
	}
	n.duration = 0;

	return n;
}

u16 get_key(note &n)
{
	return (n.channel << BITS_PER_BYTE) | n.pitch;
}

void insert_note(ifstream &ifs, note_map &notes)
{
	u16 key;
	note n, *np;
	note_map::iterator itr;

	n = get_note(ifs);
	key = get_key(n);

	if (n.velocity == 0) { /* special note off */
		itr = notes.find(key);
		np = &itr->second;
		if (itr == notes.end()) {
			cerr << "can't find any note for erasing" << endl;
			exit(MIDI_ERROR);
		}
		if (DEBUG) {
			cerr << "\terasing note..." << endl;
			cerr << "\t\tchannel:"  << (int) np->channel  << endl;
			cerr << "\t\tpitch:"    << (int) np->pitch    << endl;
			cerr << "\t\tvelocity:" << (int) np->velocity << endl;
			cerr << "\t\tduration:" << np->duration       << endl;
		}
		notes.erase(itr);
	}
	else {
		if (DEBUG) {
			cerr << "\tinserting note..." << endl;
			cerr << "\t\tchannel:"  << (int) n.channel  << endl;
			cerr << "\t\tpitch:"    << (int) n.pitch    << endl;
			cerr << "\t\tvelocity:" << (int) n.velocity << endl;
			cerr << "\t\tduration:" << n.duration       << endl;
		}
		notes.insert(pair<u16, note>(key, n));
	}
}

void erase_note(ifstream &ifs, note_map &notes)
{
	u16 key;
	note n, *np;
	note_map::iterator itr;

	n = get_note(ifs);
	key = get_key(n);

	itr = notes.find(key);

	if (itr == notes.end()) {
		cerr << "can't find any note for erasing" << endl;
		if (DEBUG) {
			cerr << "\tnote info:" << endl;
			cerr << "\t\tchannel:"  << (int) n.channel  << endl;
			cerr << "\t\tpitch:"    << (int) n.pitch    << endl;
			cerr << "\t\tvelocity:" << (int) n.velocity << endl;
			cerr << "\t\tduration:" << n.duration       << endl;
		}
		exit(MIDI_ERROR);
	}
	else {
		np = &itr->second;
		if (DEBUG) {
			cerr << "\terasing note..." << endl;
			cerr << "\t\tchannel:"  << (int) np->channel  << endl;
			cerr << "\t\tpitch:"    << (int) np->pitch    << endl;
			cerr << "\t\tvelocity:" << (int) np->velocity << endl;
			cerr << "\t\tduration:" << np->duration       << endl;
		}
		notes.erase(itr);
	}
}

void check_midi(ifstream &ifs, note_map &notes)
{
	u8 c;

	if (DEBUG)
		cerr << "midi ivent" << endl;

	c = peek_byte(ifs);
	prev_status = c;

	switch (c & 0xF0) {
	case NOTE_ON:
		if (DEBUG)
			cerr << "\tnote on" << endl;
		insert_note(ifs, notes);
		break;
	case NOTE_OFF:
		if (DEBUG)
			cerr << "\tnote off" << endl;
		erase_note(ifs, notes);
		break;
	case POLY_PRESS: /* 3 byte midi ivent */
	case CTRL_CHANGE:
	case PITCH_BEND:
		if (DEBUG)
			cerr << "\tsome 3byte ivent" << endl;
		get_nbyte(ifs, 3);
		break;
	case CH_PRESS: /* 2 byte midi ivent */
	case PROG_CHANGE:
		if (DEBUG)
			cerr << "\tsome 2byte ivent" << endl;
		get_nbyte(ifs, 2);
		break;
	default:
		break;
	}
}

void check_sysex(ifstream &ifs)
{
	u8 type, length;

	type = get_nbyte(ifs, 1);
	length = get_nbyte(ifs, 1);

	if (DEBUG) {
		cerr << "sysex ivent" << endl;
		cerr << hex << showbase;
		cerr << "\ttype:"   << (int) type   << endl;
		cerr << "\tlength:" << (int) length << endl;
		cerr.unsetf(ios::hex | ios::showbase);
	}

	get_nbyte(ifs, length);

	prev_status = STATUS_RESET;
}

midi_state check_meta(ifstream &ifs)
{
	int i;
	u8 c, status, type, length;

	for (i = 0; i < 3; i++) {
		c = get_nbyte(ifs, 1);
		switch (i) {
		case 0:
			status = c;
			break;
		case 1:
			type = c;
			break;
		case 2:
			length = c;
			break;
		default:
			break;
		}
	}

	if (DEBUG) {
		cerr << "meta ivent" << endl;
		cerr << hex << showbase;
		cerr << "\tstatus:" << (int) status << endl;
		cerr << "\ttype:"   << (int) type   << endl;
		cerr << "\tlength:" << (int) length << endl;
		cerr.unsetf(ios::hex | ios::showbase);
	}

	/* skip data byte */
	get_nbyte(ifs, length);

	prev_status = STATUS_RESET;

	if (type == TRACK_END)
		return TRACK;
	else
		return DELTA;
}

u32 get_delta(ifstream &ifs)
{
	int count = 0;
	u32 delta = 0;
	u8 byte;

	while (1) {
		delta <<= (count++ * DELTA_BITS_PER_BYTE);
		byte = get_nbyte(ifs, 1);
		delta += byte & ~DELTA_CONTINUE_FLAG;
		if (!(byte & DELTA_CONTINUE_FLAG))
			break;
	}

	if (DEBUG)
		cerr << "delta: " << delta << endl;

	return delta;
}

void update_note(note_map &notes, u32 delta)
{
	note_map::iterator itr = notes.begin();
	note *np;

	if (delta == 0)
		return;

	while (itr != notes.end()) {
		np = &itr->second;
		if (DEBUG) {
			cerr << "\tadding delta: " << delta            << endl;
			cerr << "\t\tchannel: "    << (int) np->channel  << endl;
			cerr << "\t\tpitch: "      << (int) np->pitch    << endl;
			cerr << "\t\tvelocity: "   << (int) np->velocity << endl;
			cerr << "\t\tduration: "   << np->duration       << endl;
		}
		np->duration += delta;
		itr++;
	}
}

u8 running_status(ifstream &ifs)
{
	if (prev_status == STATUS_RESET) {
		cerr << "can't find status byte" << endl;
		exit(CHECK_ERROR);
	}

	if (DEBUG) {
		cerr.setf(ios::hex | ios::showbase);
		cerr << "running status:" << endl;
		cerr << "\tput back: " << (int) prev_status << endl;
		cerr.unsetf(ios::hex | ios::showbase);
	}

	ifs.putback(prev_status);

	return prev_status;
}

midi_state check_ivent(ifstream &ifs)
{
	u8 c;

	c = peek_byte(ifs);

	if (!(c & STATUS_BYTE_FLAG))
		c = running_status(ifs);

	if (c <= MIDI_BYTE)
		return MIDI;
	else if (c == SYSEX_BYTE_F0 || c == SYSEX_BYTE_F7)
		return SYSEX;
	else if (c == META_BYTE)
		return META;
	else
		return UNKNOWN;
}

int main(int argc, char *argv[])
{
	ifstream ifs;
	u8 c;
	int state = HEADER;

	midi_header header;
	vector<midi_track> tracks;
	note_map notes;

	if (argc < 2)
		ifs.open("/dev/stdin", ios::binary);
	else
		ifs.open(argv[1], ios::binary);

	while (ifs.peek() != istream::traits_type::eof()) {
		switch(state) {
		case HEADER:
			check_header(ifs, header);
			state = TRACK;
			break;
		case TRACK:
			check_track(ifs, tracks);
			state = DELTA;
			break;
		case DELTA:
			update_note(notes, get_delta(ifs));
			state = check_ivent(ifs);
			break;
		case MIDI:
			check_midi(ifs, notes);
			state = DELTA;
			break;
		case SYSEX:
			check_sysex(ifs);
			state = DELTA;
			break;
		case META:
			state = check_meta(ifs);
			break;
		default:
			cerr << "unknown state" << endl;
			exit(UNKNOWN_ERROR);
			break;
		}
	}

	if (DEBUG)
		cerr << "reached EOF" << endl;

	ifs.close();
}
