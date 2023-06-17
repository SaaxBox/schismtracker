/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define NEED_TIME
#include "headers.h"

#include "event.h"

#include "util.h"

#include "midi.h"
#include "song.h"

#include "sdlmain.h"

#include "dmoz.h"

#include <ctype.h>
#include <assert.h>

static int _connected = 0;
/* midi_mutex is locked by the main thread,
midi_port_mutex is for the port thread(s),
and midi_record_mutex is by the event/sound thread
*/
static SDL_mutex *midi_mutex = NULL;
static SDL_mutex *midi_port_mutex = NULL;
static SDL_mutex *midi_record_mutex = NULL;
static SDL_mutex *midi_play_mutex = NULL;
static SDL_cond *midi_play_cond = NULL;

static struct midi_provider *port_providers = NULL;

/* configurable midi stuff */
int midi_flags = MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCHBEND;

int midi_pitch_depth = 12;
int midi_amplification = 100;
int midi_c5note = 60;

#define CFG_GET_MI(v,d) midi_ ## v = d

static void _cfg_load_midi_part_locked(struct midi_port *q)
{
	struct cfg_section *c;
	const char *sn;
	char *ptr, *ss, *sp;
	char buf[256];
	int j;

	ss = q->name;
	if (!ss) return;
	while (isspace(*ss)) ss++;
	if (!*ss) return;

	sp = q->provider ? q->provider->name : NULL;
	if (sp) {
		while (isspace(*sp)) sp++;
		if (!*sp) sp = NULL;
	}
}


void load_midi(void)
{
	midi_config_t *md, *mc;
	char buf[17], buf2[33];
	int i;

	CFG_GET_MI(flags, MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCHBEND);
	CFG_GET_MI(pitch_depth, 12);
	CFG_GET_MI(amplification, 100);
	CFG_GET_MI(c5note, 60);

	song_lock_audio();
	md = &default_midi_config;
	mc = &current_song->midi_config;
	memcpy(mc, md, sizeof(midi_config_t));

	song_unlock_audio();
}

void midi_engine_poll_ports(void)
{
	struct midi_provider *n;

	if (!midi_mutex) return;

	SDL_mutexP(midi_mutex);
	for (n = port_providers; n; n = n->next) {
		if (n->poll) n->poll(n);
	}
	SDL_mutexV(midi_mutex);
}

int midi_engine_start(void)
{
	if (_connected)
		return 1;

	midi_mutex        = SDL_CreateMutex();
	midi_record_mutex = SDL_CreateMutex();
	midi_play_mutex   = SDL_CreateMutex();
	midi_port_mutex   = SDL_CreateMutex();
	midi_play_cond    = SDL_CreateCond();

	if (!(midi_mutex && midi_record_mutex && midi_play_mutex && midi_port_mutex && midi_play_cond)) {
		if (midi_mutex)        SDL_DestroyMutex(midi_mutex);
		if (midi_record_mutex) SDL_DestroyMutex(midi_record_mutex);
		if (midi_play_mutex)   SDL_DestroyMutex(midi_play_mutex);
		if (midi_port_mutex)   SDL_DestroyMutex(midi_port_mutex);
		if (midi_play_cond)    SDL_DestroyCond(midi_play_cond);
		midi_mutex = midi_record_mutex = midi_play_mutex = midi_port_mutex = NULL;
		midi_play_cond = NULL;
		return 0;
	}

//	_midi_engine_connect();
	_connected = 1;
	return 1;
}

void midi_engine_reset(void)
{
	if (!_connected) return;
	midi_engine_stop();
//	_midi_engine_connect();
}

void midi_engine_stop(void)
{
	struct midi_provider *n, *p;
	struct midi_port *q;

	if (!_connected) return;
	if (!midi_mutex) return;

	SDL_mutexP(midi_mutex);
	for (n = port_providers; n;) {
		p = n->next;

		q = NULL;
		while (midi_port_foreach(p, &q)) {
			midi_port_unregister(q->num);
		}

		if (n->thread) {
			n->cancelled = 1;
			SDL_WaitThread(n->thread, NULL);
		}
		free(n->name);
		free(n);
		n = p;
	}
	_connected = 0;
	SDL_mutexV(midi_mutex);
}


/* PORT system */
static struct midi_port **port_top = NULL;
static int port_count = 0;
static int port_alloc = 0;

struct midi_port *midi_engine_port(int n, const char **name)
{
	struct midi_port *pv = NULL;

	if (!midi_port_mutex) return NULL;
	SDL_mutexP(midi_port_mutex);
	if (n >= 0 && n < port_count) {
		pv = port_top[n];
		if (name) *name = pv->name;
	}
	SDL_mutexV(midi_port_mutex);
	return pv;
}

int midi_engine_port_count(void)
{
	int pc;
	if (!midi_port_mutex) return 0;
	SDL_mutexP(midi_port_mutex);
	pc = port_count;
	SDL_mutexV(midi_port_mutex);
	return pc;
}

/* midi engines register a provider (one each!) */
struct midi_provider *midi_provider_register(const char *name,
		struct midi_driver *driver)
{
	struct midi_provider *n;

	if (!midi_mutex) return NULL;

	n = mem_calloc(1, sizeof(struct midi_provider));
	n->name = str_dup(name);
	n->poll = driver->poll;
	n->enable = driver->enable;
	n->disable = driver->disable;
	if (driver->flags & MIDI_PORT_CAN_SCHEDULE) {
		n->send_later = driver->send;
		n->drain = driver->drain;
	} else {
		n->send_now = driver->send;
	}

	SDL_mutexP(midi_mutex);
	n->next = port_providers;
	port_providers = n;

	if (driver->thread) {
		// FIXME this cast is stupid
		n->thread = SDL_CreateThread((int (*)(void*))driver->thread, NULL, n);
	}

	SDL_mutexV(midi_mutex);

	return n;
}

/* midi engines list ports this way */
int midi_port_register(struct midi_provider *pv, int inout, const char *name,
void *userdata, int free_userdata)
{
	struct midi_port *p, **pt;
	int i;

	if (!midi_port_mutex) return -1;

	p = mem_alloc(sizeof(struct midi_port));
	p->io = 0;
	p->iocap = inout;
	p->name = str_dup(name);
	p->enable = pv->enable;
	p->disable = pv->disable;
	p->send_later = pv->send_later;
	p->send_now = pv->send_now;
	p->drain = pv->drain;

	p->free_userdata = free_userdata;
	p->userdata = userdata;
	p->provider = pv;

	for (i = 0; i < port_alloc; i++) {
		if (port_top[i] == NULL) {
			port_top[i] = p;
			p->num = i;
			port_count++;
			_cfg_load_midi_part_locked(p);
			return i;
		}
	}

	SDL_mutexP(midi_port_mutex);
	port_alloc += 4;
	pt = realloc(port_top, sizeof(struct midi_port *) * port_alloc);
	if (!pt) {
		free(p->name);
		free(p);
		SDL_mutexV(midi_port_mutex);
		return -1;
	}
	pt[port_count] = p;
	for (i = port_count+1; i < port_alloc; i++) pt[i] = NULL;
	port_top = pt;
	p->num = port_count;
	port_count++;

	/* finally, and just before unlocking, load any configuration for it... */
	_cfg_load_midi_part_locked(p);

	SDL_mutexV(midi_port_mutex);
	return p->num;
}

int midi_port_foreach(struct midi_provider *p, struct midi_port **cursor)
{
	int i;
	if (!midi_port_mutex) return 0;

	SDL_mutexP(midi_port_mutex);
	do {
		if (!*cursor) {
			i = 0;
		} else {
			i = ((*cursor)->num) + 1;
			while (i < port_alloc && !port_top[i]) i++;
		}
		if (i >= port_alloc) {
			*cursor = NULL;
			SDL_mutexV(midi_port_mutex);
			return 0;
		}
		*cursor = port_top[i];
	} while (p && (*cursor)->provider != p);
	SDL_mutexV(midi_port_mutex);
	return 1;
}

static int _midi_send_unlocked(const unsigned char *data, unsigned int len, unsigned int delay,
			int from)
{
	struct midi_port *ptr = NULL;
	int need_timer = 0;
#if 0
	unsigned int i;
printf("MIDI: ");
	for (i = 0; i < len; i++) {
		printf("%02x ", data[i]);
	}
puts("");
fflush(stdout);
#endif
	if (from == 0) {
		/* from == 0 means from immediate; everyone plays */
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_now)
					ptr->send_now(ptr, data, len, 0);
				else if (ptr->send_later)
					ptr->send_later(ptr, data, len, 0);
			}
		}
	} else if (from == 1) {
		/* from == 1 means from buffer-flush; only "now" plays */
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_now)
					ptr->send_now(ptr, data, len, 0);
			}
		}
	} else {
		/* from == 2 means from buffer-write; only "later" plays */
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_later)
					ptr->send_later(ptr, data, len, delay);
				else if (ptr->send_now)
					need_timer = 1;
			}
		}
	}
	return need_timer;
}

void midi_send_now(const unsigned char *seq, unsigned int len)
{
	if (!midi_record_mutex) return;

	SDL_mutexP(midi_record_mutex);
	_midi_send_unlocked(seq, len, 0, 0);
	SDL_mutexV(midi_record_mutex);
}

/*----------------------------------------------------------------------------------*/

/* okay, our local queue is a little confusing,
 * but it works on the premise that the people who need it (oss)
 * also have systems with high-speed context switches.
 *
 * this sucks for freebsd/386 users as they've got a high-latency
 * audio system (oss) and slow context switches (x86). someone should
 * port alsa to freebsd. maybe someone else will solve this problem,
 * or maybe the computers that are slow enough to matter simply won't
 * be bothered.
 *
 * midi, that is, real midi, is 31250bps. that's 3125 bits per msec,
 * or 391 bytes per msec. i use a fixed buffer here because access needs
 * to be fast, and attempting to handle more will simply help people
 * using software/only setups.
 *
 * really, software-only midi, without kernel assistance sucks.
 */
struct qent {
	int used;
	unsigned char b[391];
};
static struct qent *qq = NULL;
static int midims, ms10s, qlen;

void midi_queue_alloc(int my_audio_buffer_samples, int sample_size, int samples_per_second)
{
	int buffer_size = (my_audio_buffer_samples * sample_size);

	if (qq) {
		free(qq);
		qq = NULL;
	}

	/* how long is the audio buffer in 10 msec?
	 * well, (sample_size*samples_per_second)/80 is the number of bytes per msec
	 */
	midims = sample_size * samples_per_second;
	if ((midims % 80) != 0) midims += (80 - (midims % 80));
	ms10s = midims / 80;
	midims /= 8;

	if (ms10s > buffer_size) {
		/* okay, there's not even 10msec of audio data; midi queueing will be impossible */
		qlen = 0;
		return;
	}

	if ((buffer_size % ms10s) != 0) {
		buffer_size += (ms10s - (buffer_size % ms10s));
	}
	qlen = buffer_size / ms10s;
	/* now qlen is the number of msec in digital output buffer */

	qq = mem_calloc(qlen, sizeof(struct qent));
}

static SDL_Thread *midi_queue_thread = NULL;

static int _midi_queue_run(UNUSED void *xtop)
{
	int i;

//#ifdef WIN32
//	__win32_pick_usleep();
//	SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
//	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
//	/*SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);*/
//#endif

	SDL_mutexP(midi_play_mutex);
	for (;;) {
		SDL_CondWait(midi_play_cond, midi_play_mutex);

		for (i = 0; i < qlen; i++) {
			SDL_mutexP(midi_record_mutex);
			_midi_send_unlocked(qq[i].b, qq[i].used, 0, 1);
			SDL_mutexV(midi_record_mutex);
//			SLEEP_FUNC(10000); /* 10msec */
			qq[i].used = 0;
		}
	}

	return 0; /* never happens */
}

int midi_need_flush(void)
{
	struct midi_port *ptr;
	int need_explicit_flush = 0;
	int i;

	if (!midi_record_mutex || !midi_play_mutex) return 0;

	ptr = NULL;

	while (midi_port_foreach(NULL, &ptr)) {
		if ((ptr->io & MIDI_OUTPUT)) {
			if (!ptr->drain && ptr->send_now)
				need_explicit_flush = 1;
		}
	}
	if (!need_explicit_flush) return 0;

	for (i = 0; i < qlen; i++) {
		if (qq[i].used) return 1;
	}

	return 0;
}

void midi_send_flush(void)
{
	struct midi_port *ptr = NULL;
	int need_explicit_flush = 0;

	if (!midi_record_mutex || !midi_play_mutex) return;

	while (midi_port_foreach(NULL, &ptr)) {
		if ((ptr->io & MIDI_OUTPUT)) {
			if (ptr->drain) ptr->drain(ptr);
			else if (ptr->send_now) need_explicit_flush=1;
		}
	}

	/* no need for midi sync huzzah; driver does it for us... */
	if (!need_explicit_flush) return;

	if (!midi_queue_thread) {
		midi_queue_thread = SDL_CreateThread(_midi_queue_run, NULL, NULL);
	}

	SDL_mutexP(midi_play_mutex);
	SDL_CondSignal(midi_play_cond);
	SDL_mutexV(midi_play_mutex);
}

void midi_send_buffer(const unsigned char *data, unsigned int len, unsigned int pos)
{
	if (!midi_record_mutex) return;

	SDL_mutexP(midi_record_mutex);

	/* pos is still in miliseconds */
	if (midims != 0 && _midi_send_unlocked(data, len, pos/midims, 2)) {
		/* grr, we need a timer */

		/* calculate pos in buffer */
		pos /= ms10s;
		assert(((unsigned)pos) < ((unsigned)qlen));

		if ((len + qq[pos].used) > sizeof(qq[pos].b)) {
			len = sizeof(qq[pos].b) - qq[pos].used;
			/* okay, we're going to lose data here */
		}
		memcpy(qq[pos].b+qq[pos].used, data, len);
		qq[pos].used += len;
	}

	SDL_mutexV(midi_record_mutex);
}

/*----------------------------------------------------------------------------------*/

void midi_port_unregister(int num)
{
	struct midi_port *q;
	int i;

	if (!midi_port_mutex) return;

	SDL_mutexP(midi_port_mutex);
	for (i = 0; i < port_alloc; i++) {
		if (port_top[i] && port_top[i]->num == num) {
			q = port_top[i];
			if (q->disable) q->disable(q);
			if (q->free_userdata) free(q->userdata);
			free(q);

			port_top[i] = NULL;
			port_count--;
			break;
		}
	}
	SDL_mutexV(midi_port_mutex);
}

void midi_received_cb(struct midi_port *src, unsigned char *data, unsigned int len)
{
	unsigned char d4[4];
	int cmd;

	if (!len) return;
	if (len < 4) {
		memset(d4, 0, sizeof(d4));
		memcpy(d4, data, len);
		data = d4;
	}

	cmd = ((*data) & 0xF0) >> 4;
	if (cmd == 0x8 || (cmd == 0x9 && data[2] == 0)) {
		midi_event_note(MIDI_NOTEOFF, data[0] & 15, data[1], 0);
	} else if (cmd == 0x9) {
		midi_event_note(MIDI_NOTEON, data[0] & 15, data[1], data[2]);
	} else if (cmd == 0xA) {
		midi_event_note(MIDI_KEYPRESS, data[0] & 15, data[1], data[2]);
	} else if (cmd == 0xB) {
		midi_event_controller(data[0] & 15, data[1], data[2]);
	} else if (cmd == 0xC) {
		midi_event_program(data[0] & 15, data[1]);
	} else if (cmd == 0xD) {
		midi_event_aftertouch(data[0] & 15, data[1]);
	} else if (cmd == 0xE) {
		midi_event_pitchbend(data[0] & 15, data[1]);
	} else if (cmd == 0xF) {
		switch ((*data & 15)) {
		case 0: /* sysex */
			if (len <= 2) return;
			midi_event_sysex(data+1, len-2);
			break;
		case 6: /* tick */
			midi_event_tick();
			break;
		default:
			/* something else */
			midi_event_system((*data & 15), (data[1])
					| (data[2] << 8)
					| (data[3] << 16));
			break;
		}
	}
}

static void midi_push_event(Uint8 code, void *data1, size_t data1_len, int alloc)
{
	SDL_Event e = { .user = { .type = SCHISM_EVENT_MIDI, .code = code, .data1 = data1 } };

	if (data1 && alloc) {
		e.user.data1 = mem_alloc(data1_len);
		memcpy(e.user.data1, data1, data1_len);
	}

	SDL_PushEvent(&e);
}

void midi_event_note(enum midi_note mnstatus, int channel, int note, int velocity)
{
	int st[4] = { mnstatus, channel, note, velocity };

	midi_push_event(SCHISM_EVENT_MIDI_NOTE, st, sizeof(st), 1);
}

void midi_event_controller(int channel, int param, int value)
{
	int st[4] = { value, channel, param };

	midi_push_event(SCHISM_EVENT_MIDI_CONTROLLER, st, sizeof(st), 1);
}

void midi_event_program(int channel, int value)
{
	int st[4] = { value, channel };

	midi_push_event(SCHISM_EVENT_MIDI_PROGRAM, st, sizeof(st), 1);
}

void midi_event_aftertouch(int channel, int value)
{
	int st[4] = { value, channel };

	midi_push_event(SCHISM_EVENT_MIDI_AFTERTOUCH, st, sizeof(st), 1);
}

void midi_event_pitchbend(int channel, int value)
{
	int st[4] = { value, channel };

	midi_push_event(SCHISM_EVENT_MIDI_PITCHBEND, st, sizeof(st), 1);
}

void midi_event_system(int argv, int param)
{
	int st[4] = { argv, param };

	midi_push_event(SCHISM_EVENT_MIDI_SYSTEM, st, sizeof(st), 1);
}

void midi_event_tick(void)
{
	midi_push_event(SCHISM_EVENT_MIDI_TICK, NULL, 0, 0);
}

void midi_event_sysex(const unsigned char *data, unsigned int len)
{
	size_t packet_len = len + sizeof(len);
	void *packet = mem_alloc(packet_len);

	memcpy(packet, &len, sizeof(len));
	memcpy(packet + sizeof(len), data, len);

	midi_push_event(SCHISM_EVENT_MIDI_SYSEX, packet, packet_len, 0);
}

