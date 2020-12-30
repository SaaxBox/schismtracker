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

/* This is getting almost as disturbing as the pattern editor. */

#include "headers.h"
#include "it.h"
#include "page.h"
#include "song.h"
#include "dmoz.h"
#include "video.h"

#include <sys/stat.h>

/* --------------------------------------------------------------------- */
/* just one global variable... */

int instrument_list_subpage = PAGE_INSTRUMENT_LIST_GENERAL;

/* --------------------------------------------------------------------- */
/* ... but tons o' ugly statics */

static struct widget widgets_general[18];
static struct widget widgets_volume[17];
static struct widget widgets_panning[19];
static struct widget widgets_pitch[20];

/* rastops for envelope */
static struct vgamem_overlay env_overlay = {
	32, 18, 65, 25,
	NULL, 0, 0, 0
};

/* toggled when pressing "," on the note table's sample field
 * more of a boolean than a bit mask  -delt.
 */
static int note_sample_mask = 1;

static struct widget *get_page_widgets(void)
{
	switch (instrument_list_subpage) {
	case PAGE_INSTRUMENT_LIST_GENERAL:
		return widgets_general;
	case PAGE_INSTRUMENT_LIST_VOLUME:
		return widgets_volume;
	case PAGE_INSTRUMENT_LIST_PANNING:
		return widgets_panning;
	case PAGE_INSTRUMENT_LIST_PITCH:
		return widgets_pitch;
	};
	return widgets_general;
}

static const int subpage_switches_group[5] = { 1, 2, 3, 4, -1 };
static const int nna_group[5] = { 6, 7, 8, 9, -1 };
static const int dct_group[5] = { 10, 11, 12, 13, -1 };
static const int dca_group[4] = { 14, 15, 16, -1 };

static const char *const pitch_envelope_states[] = { "Off", "On Pitch", "On Filter", NULL };

static int top_instrument = 1;
static int current_instrument = 1;
static int _altswap_lastvis = 99; // for alt-down instrument-swapping
static int instrument_cursor_pos = 25;  /* "play" mode */

static int note_trans_top_line = 0;
static int note_trans_sel_line = 0;

static int note_trans_cursor_pos = 0;

/* shared by all the numentries on a page
 * (0 = volume, 1 = panning, 2 = pitch) */
static int numentry_cursor_pos[3] = { 0 };

static int current_node_vol = 0;
static int current_node_pan = 0;
static int current_node_pitch = 0;

static int envelope_edit_mode = 0;
static int envelope_mouse_edit = 0;
static int envelope_tick_limit = 0;

static void set_subpage(int page);

/* playback */
static int last_note = 61;              /* C-5 */

/* strange saved envelopes */
static song_envelope_t saved_env[10];
static unsigned int flags[10] = {0};

/* --------------------------------------------------------------------------------------------------------- */

static void save_envelope(int slot, song_envelope_t *e, unsigned int sec)
{
	song_instrument_t *ins;

	slot = ((unsigned)slot)%10;

	ins = song_get_instrument(current_instrument);
	memcpy(&saved_env[slot], e, sizeof(song_envelope_t));

	switch (sec) {
	case ENV_VOLUME:
		flags[slot] = ins->flags & (ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY);
		break;
	case ENV_PANNING:
		flags[slot] =
			((ins->flags & ENV_PANNING) ? ENV_VOLUME : 0)
		|       ((ins->flags & ENV_PANSUSTAIN) ? ENV_VOLSUSTAIN : 0)
		|       ((ins->flags & ENV_PANLOOP) ? ENV_VOLLOOP : 0)
		|       ((ins->flags & ENV_PANCARRY) ? ENV_VOLCARRY : 0)
		|       (ins->flags & ENV_SETPANNING);
		break;
	case ENV_PITCH:
		flags[slot] =
			((ins->flags & ENV_PITCH) ? ENV_VOLUME : 0)
		|       ((ins->flags & ENV_PITCHSUSTAIN) ? ENV_VOLSUSTAIN : 0)
		|       ((ins->flags & ENV_PITCHLOOP) ? ENV_VOLLOOP : 0)
		|       ((ins->flags & ENV_PITCHCARRY) ? ENV_VOLCARRY : 0)
		|       (ins->flags & ENV_FILTER);
		break;
	};
}
static void restore_envelope(int slot, song_envelope_t *e, unsigned int sec)
{
	song_instrument_t *ins;

	song_lock_audio();

	slot = ((unsigned)slot)%10;

	ins = song_get_instrument(current_instrument);
	memcpy(e, &saved_env[slot], sizeof(song_envelope_t));

	switch (sec) {
	case ENV_VOLUME:
		ins->flags &= ~(ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY);
		ins->flags |= (flags[slot] & (ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY));
		break;

	case ENV_PANNING:
		ins->flags &= ~(ENV_PANNING|ENV_PANSUSTAIN|ENV_PANLOOP|ENV_PANCARRY|ENV_SETPANNING);
		if (flags[slot] & ENV_VOLUME) ins->flags |= ENV_PANNING;
		if (flags[slot] & ENV_VOLSUSTAIN) ins->flags |= ENV_PANSUSTAIN;
		if (flags[slot] & ENV_VOLLOOP) ins->flags |= ENV_PANLOOP;
		if (flags[slot] & ENV_VOLCARRY) ins->flags |= ENV_PANCARRY;
		ins->flags |= (flags[slot] & ENV_SETPANNING);
		break;

	case ENV_PITCH:
		ins->flags &= ~(ENV_PITCH|ENV_PITCHSUSTAIN|ENV_PITCHLOOP|ENV_PITCHCARRY|ENV_FILTER);
		if (flags[slot] & ENV_VOLUME) ins->flags |= ENV_PITCH;
		if (flags[slot] & ENV_VOLSUSTAIN) ins->flags |= ENV_PITCHSUSTAIN;
		if (flags[slot] & ENV_VOLLOOP) ins->flags |= ENV_PITCHLOOP;
		if (flags[slot] & ENV_VOLCARRY) ins->flags |= ENV_PITCHCARRY;
		ins->flags |= (flags[slot] & ENV_FILTER);
		break;

	};

	song_unlock_audio();

	status.flags |= SONG_NEEDS_SAVE;
}



/* --------------------------------------------------------------------------------------------------------- */

static void instrument_list_draw_list(void);

/* --------------------------------------------------------------------------------------------------------- */
static int _last_vis_inst(void)
{
	int i, j, n;

	n = 99;
	j = 0;
	/* 65 is first visible sample on last page */
	for (i = 65; i < MAX_INSTRUMENTS; i++) {
		if (!csf_instrument_is_empty(current_song->instruments[i])) {
			j = i;
		}
	}
	while ((j + 34) > n) 
		n += 34;
	return MIN(n, MAX_INSTRUMENTS - 1);
}
/* the actual list */

static void instrument_list_reposition(void)
{
	if (current_instrument < top_instrument) {
		top_instrument = current_instrument;
		if (top_instrument < 1) {
			top_instrument = 1;
		}
	} else if (current_instrument > top_instrument + 34) {
		top_instrument = current_instrument - 34;
	}
}

int instrument_get_current(void)
{
	return current_instrument;
}

void instrument_set(int n)
{
	int new_ins = n;
	song_instrument_t *ins;

	if (page_is_instrument_list(status.current_page)) {
		new_ins = CLAMP(n, 1, _last_vis_inst());
	} else {
		new_ins = CLAMP(n, 0, _last_vis_inst());
	}

	if (current_instrument == new_ins)
		return;

	envelope_edit_mode = 0;
	current_instrument = new_ins;
	instrument_list_reposition();

	ins = song_get_instrument(current_instrument);

	current_node_vol = ins->vol_env.nodes ? CLAMP(current_node_vol, 0, ins->vol_env.nodes - 1) : 0;
	current_node_pan = ins->pan_env.nodes ? CLAMP(current_node_vol, 0, ins->pan_env.nodes - 1) : 0;
	current_node_pitch = ins->pitch_env.nodes ? CLAMP(current_node_vol, 0, ins->pan_env.nodes - 1) : 0;

	status.flags |= NEED_UPDATE;
}

void instrument_synchronize_to_sample(void)
{
	song_instrument_t *ins;
	int sample = sample_get_current();
	int n, pos;

	/* 1. if the instrument with the same number as the current sample
	 * has the sample in its sample_map, change to that instrument. */
	ins = song_get_instrument(sample);
	for (pos = 0; pos < 120; pos++) {
		if ((int)(ins->sample_map[pos]) == sample) {
			instrument_set(sample);
			return;
		}
	}

	/* 2. look through the instrument list for the first instrument
	 * that uses the selected sample. */
	for (n = 1; n < 100; n++) {
		if (n == sample)
			continue;
		ins = song_get_instrument(n);
		for (pos = 0; pos < 120; pos++) {
			if ((int)(ins->sample_map[pos]) == sample) {
				instrument_set(n);
				return;
			}
		}
	}

	/* 3. if no instruments are using the sample, just change to the
	 * same-numbered instrument. */
	instrument_set(sample);
}

/* --------------------------------------------------------------------- */

static int instrument_list_add_char(int c)
{
	song_instrument_t *ins;

	if (c < 32)
		return 0;
	ins = song_get_instrument(current_instrument);
	text_add_char(ins->name, c, &instrument_cursor_pos, 25);
	if (instrument_cursor_pos == 25)
		instrument_cursor_pos--;

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
	return 1;
}

static void instrument_list_delete_char(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	text_delete_char(ins->name, &instrument_cursor_pos, 25);

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

static void instrument_list_delete_next_char(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	text_delete_next_char(ins->name, &instrument_cursor_pos, 25);

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

static void clear_instrument_text(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	memset(ins->filename, 0, 14);
	memset(ins->name, 0, 26);
	if (instrument_cursor_pos != 25)
		instrument_cursor_pos = 0;

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

static void do_swap_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_swap_instruments(current_instrument, n);
	}
}

static void do_exchange_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_exchange_instruments(current_instrument, n);
	}
}

static void do_copy_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_copy_instrument(current_instrument, n);
	}
}

static void do_replace_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_replace_instrument(current_instrument, n);
	}
}

/* --------------------------------------------------------------------- */

static void instrument_list_draw_list(void)
{
	int pos, n;
	song_instrument_t *ins;
	int selected = (ACTIVE_PAGE.selected_widget == 0);
	int is_current;
	int ss, cl = 0, cr = 0;
	int is_playing[MAX_INSTRUMENTS];
	char buf[4];

	ss = -1;

	song_get_playing_instruments(is_playing);

	for (pos = 0, n = top_instrument; pos < 35; pos++, n++) {
		ins = song_get_instrument(n);
		is_current = (n == current_instrument);

		if (ins->played)
			draw_char(is_playing[n] > 1 ? 183 : 173, 1, 13 + pos, is_playing[n] ? 3 : 1, 2);

		draw_text(num99tostr(n, buf), 2, 13 + pos, 0, 2);
		if (instrument_cursor_pos < 25) {
			/* it's in edit mode */
			if (is_current) {
				draw_text_len(ins->name, 25, 5, 13 + pos, 6, 14);
				if (selected) {
					draw_char(ins->name[instrument_cursor_pos],
						  5 + instrument_cursor_pos,
						  13 + pos, 0, 3);
				}
			} else {
				draw_text_len(ins->name, 25, 5, 13 + pos, 6, 0);
			}
		} else {
			draw_text_len(ins->name, 25, 5, 13 + pos,
				      ((is_current && selected) ? 0 : 6),
				      (is_current ? (selected ? 3 : 14) : 0));
		}
		if (ss == n) {
			draw_text_len(ins->name + cl, (cr-cl)+1,
					5 + cl, 13 + pos,
					(is_current ? 3 : 11), 8);
		}
	}
}

static int instrument_list_handle_key_on_list(struct key_event * k)
{
	int new_ins = current_instrument;

	if (k->state == KEY_PRESS && k->mouse != MOUSE_NONE && k->y >= 13 && k->y <= 47 && k->x >= 5 && k->x <= 30) {
		if (k->mouse == MOUSE_CLICK) {
			new_ins = (k->y - 13) + top_instrument;
			if (instrument_cursor_pos < 25)
				instrument_cursor_pos = MIN(k->x - 5, 24);
			status.flags |= NEED_UPDATE;
		} else if (k->mouse == MOUSE_DBLCLICK) {
			/* this doesn't seem to work, but I think it'd be
			more useful if double click switched to edit mode */
			if (instrument_cursor_pos < 25) {
				instrument_cursor_pos = 25;
				get_page_widgets()->accept_text = 0;
			} else {
				set_page(PAGE_LOAD_INSTRUMENT);
			}
			status.flags |= NEED_UPDATE;
			return 1;

		} else if (k->mouse == MOUSE_SCROLL_UP) {
			top_instrument -= MOUSE_SCROLL_LINES;
			if (top_instrument < 1) top_instrument = 1;
			status.flags |= NEED_UPDATE;
			return 1;
		} else if (k->mouse == MOUSE_SCROLL_DOWN) {
			top_instrument += MOUSE_SCROLL_LINES;
			if (top_instrument > (_last_vis_inst()-34)) top_instrument = _last_vis_inst()-34;
			status.flags |= NEED_UPDATE;
			return 1;
		}
	} else {
		switch (k->sym) {
		case SDLK_UP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_ALT) {
				if (current_instrument > 1) {
					new_ins = current_instrument - 1;
					song_swap_instruments(current_instrument, new_ins);
				}
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				new_ins--;
			}
			break;
		case SDLK_DOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_ALT) {
				// restrict position to the "old" value of _last_vis_inst()
				// (this is entirely for aesthetic reasons)
				if (status.last_keysym != SDLK_DOWN && !k->is_repeat)
					_altswap_lastvis = _last_vis_inst();
				if (current_instrument < _altswap_lastvis) {
					new_ins = current_instrument + 1;
					song_swap_instruments(current_instrument, new_ins);
				}
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				new_ins++;
			}
			break;
		case SDLK_PAGEUP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_CTRL)
				new_ins = 1;
			else
				new_ins -= 16;
			break;
		case SDLK_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_CTRL)
				new_ins = _last_vis_inst();
			else
				new_ins += 16;
			break;
		case SDLK_HOME:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos < 25) {
				instrument_cursor_pos = 0;
				get_page_widgets()->accept_text = 1;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SDLK_END:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos < 24) {
				instrument_cursor_pos = 24;
				get_page_widgets()->accept_text = 1;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SDLK_LEFT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos < 25 && instrument_cursor_pos > 0) {
				instrument_cursor_pos--;
				get_page_widgets()->accept_text = 1;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SDLK_RIGHT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos == 25) {
				get_page_widgets()->accept_text = 0;
				change_focus_to(1);
			} else if (instrument_cursor_pos < 24) {
				get_page_widgets()->accept_text = 1;
				instrument_cursor_pos++;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SDLK_RETURN:
			if (k->state == KEY_PRESS)
				return 0;
			if (instrument_cursor_pos < 25) {
				instrument_cursor_pos = 25;
				get_page_widgets()->accept_text = 0;
				status.flags |= NEED_UPDATE;
			} else {
				get_page_widgets()->accept_text = 1;
				set_page(PAGE_LOAD_INSTRUMENT);
			}
			return 1;
		case SDLK_ESCAPE:
			if ((k->mod & KMOD_SHIFT) || instrument_cursor_pos < 25) {
				if (k->state == KEY_RELEASE)
					return 1;
				instrument_cursor_pos = 25;
				get_page_widgets()->accept_text = 0;
				status.flags |= NEED_UPDATE;
				return 1;
			}
			return 0;
		case SDLK_BACKSPACE:
			if (k->state == KEY_RELEASE)
				return 0;
			if (instrument_cursor_pos == 25)
				return 0;
			if ((k->mod & (KMOD_CTRL | KMOD_ALT)) == 0)
				instrument_list_delete_char();
			else if (k->mod & KMOD_CTRL)
				instrument_list_add_char(127);
			return 1;
		case SDLK_INSERT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_ALT) {
				song_insert_instrument_slot(current_instrument);
				status.flags |= NEED_UPDATE;
				return 1;
			}
			return 0;
		case SDLK_DELETE:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_ALT) {
				song_remove_instrument_slot(current_instrument);
				status.flags |= NEED_UPDATE;
				return 1;
			} else if ((k->mod & KMOD_CTRL) == 0) {
				if (instrument_cursor_pos == 25)
					return 0;
				instrument_list_delete_next_char();
				return 1;
			}
			return 0;
		default:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_ALT) {
				if (k->sym == SDLK_c) {
					clear_instrument_text();
					return 1;
				}
			} else if ((k->mod & KMOD_CTRL) == 0) {
				if (!k->unicode) return 0;
				if (instrument_cursor_pos < 25) {
					return instrument_list_add_char(k->unicode);
				} else if (k->sym == SDLK_SPACE) {
					instrument_cursor_pos = 0;
					get_page_widgets()->accept_text = 0;
					status.flags |= NEED_UPDATE;
					memused_songchanged();
					return 1;
				}
			}
			return 0;
		};
	}

	new_ins = CLAMP(new_ins, 1, _last_vis_inst());
	if (new_ins != current_instrument) {
		instrument_set(new_ins);
		status.flags |= NEED_UPDATE;
		memused_songchanged();
	}

	return 1;
}

/* --------------------------------------------------------------------- */
/* note translation table */

static void note_trans_reposition(void)
{
	if (note_trans_sel_line < note_trans_top_line) {
		note_trans_top_line = note_trans_sel_line;
	} else if (note_trans_sel_line > note_trans_top_line + 31) {
		note_trans_top_line = note_trans_sel_line - 31;
	}
}

static void note_trans_draw(void)
{
	int pos, n;
	int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	int bg, sel_bg = (is_selected ? 14 : 0);
	song_instrument_t *ins = song_get_instrument(current_instrument);
	char buf[4];

	for (pos = 0, n = note_trans_top_line; pos < 32; pos++, n++) {
		bg = ((n == note_trans_sel_line) ? sel_bg : 0);

		/* invalid notes are translated to themselves (and yes, this edits the actual instrument) */
		if (ins->note_map[n] < 1 || ins->note_map[n] > 120)
			ins->note_map[n] = n + 1;

		draw_text(get_note_string(n + 1, buf), 32, 16 + pos, 2, bg);
		draw_char(168, 35, 16 + pos, 2, bg);
		draw_text(get_note_string(ins->note_map[n], buf), 36, 16 + pos, 2, bg);
		if (is_selected && n == note_trans_sel_line) {
			if (note_trans_cursor_pos == 0)
				draw_char(buf[0], 36, 16 + pos, 0, 3);
			else if (note_trans_cursor_pos == 1)
				draw_char(buf[2], 38, 16 + pos, 0, 3);
		}
		draw_char(0, 39, 16 + pos, 2, bg);
		if (ins->sample_map[n]) {
			num99tostr(ins->sample_map[n], buf);
		} else {
			buf[0] = buf[1] = 173;
			buf[2] = 0;
		}
		draw_text(buf, 40, 16 + pos, 2, bg);
		if (is_selected && n == note_trans_sel_line) {
			if (note_trans_cursor_pos == 2)
				draw_char(buf[0], 40, 16 + pos, 0, 3);
			else if (note_trans_cursor_pos == 3)
				draw_char(buf[1], 41, 16 + pos, 0, 3);
		}
	}

	/* draw the little mask thingy at the bottom. Could optimize this....  -delt.
	   Sure can! This could share the same track-view functions that the
	   pattern editor ought to be using. -Storlek */
	if (is_selected && !(status.flags & CLASSIC_MODE)) {
		switch (note_trans_cursor_pos) {
		case 0:
			draw_char(171, 36, 48, 3, 2);
			draw_char(171, 37, 48, 3, 2);
			draw_char(169, 38, 48, 3, 2);
			if (note_sample_mask) {
				draw_char(169, 40, 48, 3, 2);
				draw_char(169, 41, 48, 3, 2);
			}
			break;
		case 1:
			draw_char(169, 38, 48, 3, 2);
			if (note_sample_mask) {
				draw_char(170, 40, 48, 3, 2);
				draw_char(170, 41, 48, 3, 2);
			}
			break;
		case 2:
		case 3:
			draw_char(note_sample_mask ? 171 : 169, 40, 48, 3, 2);
			draw_char(note_sample_mask ? 171 : 169, 41, 48, 3, 2);
			break;
		};
	}
}

static void instrument_note_trans_transpose(song_instrument_t *ins, int dir)
{
	int i;
	for (i = 0; i < 120; i++) {
		ins->note_map[i] = CLAMP(ins->note_map[i]+dir, 1, 120);
	}
}

static void instrument_note_trans_insert(song_instrument_t *ins, int pos)
{
	int i;
	for (i = 119; i > pos; i--) {
		ins->note_map[i] = ins->note_map[i-1];
		ins->sample_map[i] = ins->sample_map[i-1];
	}
	if (pos) {
		ins->note_map[pos] = ins->note_map[pos-1]+1;
	} else {
		ins->note_map[0] = 1;
	}
}

static void instrument_note_trans_delete(song_instrument_t *ins, int pos)
{
	int i;
	for (i = pos; i < 120; i++) {
		ins->note_map[i] = ins->note_map[i+1];
		ins->sample_map[i] = ins->sample_map[i+1];
	}
	ins->note_map[119] = ins->note_map[118]+1;
}

static int note_trans_handle_key(struct key_event * k)
{
	int prev_line = note_trans_sel_line;
	int new_line = prev_line;
	int prev_pos = note_trans_cursor_pos;
	int new_pos = prev_pos;
	song_instrument_t *ins = song_get_instrument(current_instrument);
	int c, n;

	if (k->mouse == MOUSE_CLICK && k->mouse_button == MOUSE_BUTTON_MIDDLE) {
		if (k->state == KEY_RELEASE)
			status.flags |= CLIPPY_PASTE_SELECTION;
		return 1;
	} else if (k->mouse == MOUSE_SCROLL_UP || k->mouse == MOUSE_SCROLL_DOWN) {
		if (k->state == KEY_PRESS) {
			note_trans_top_line += (k->mouse == MOUSE_SCROLL_UP) ? -3 : 3;
			note_trans_top_line = CLAMP(note_trans_top_line, 0, 119 - 31);
			status.flags |= NEED_UPDATE;
		}
		return 1;
	} else if (k->mouse != MOUSE_NONE) {
		if (k->x >= 32 && k->x <= 41 && k->y >= 16 && k->y <= 47) {
			new_line = note_trans_top_line + k->y - 16;
			if (new_line == prev_line) {
				switch (k->x - 36) {
				case 2:
					new_pos = 1;
					break;
				case 4:
					new_pos = 2;
					break;
				case 5:
					new_pos = 3;
					break;
				default:
					new_pos = 0;
					break;
				};
			}
		}
	} else if (k->mod & KMOD_ALT) {
		if (k->state == KEY_RELEASE)
			return 0;
		switch (k->sym) {
		case SDLK_UP:
			instrument_note_trans_transpose(ins, 1);
			break;
		case SDLK_DOWN:
			instrument_note_trans_transpose(ins, -1);
			break;
		case SDLK_INSERT:
			instrument_note_trans_insert(ins, note_trans_sel_line);
			break;
		case SDLK_DELETE:
			instrument_note_trans_delete(ins, note_trans_sel_line);
			break;
		case SDLK_n:
			n = note_trans_sel_line - 1; // the line to copy *from*
			if (n < 0 || ins->note_map[n] == NOTE_LAST)
				break;
			ins->note_map[note_trans_sel_line] = ins->note_map[n] + 1;
			ins->sample_map[note_trans_sel_line] = ins->sample_map[n];
			new_line++;
			break;
		case SDLK_p:
			n = note_trans_sel_line + 1; // the line to copy *from*
			if (n > (NOTE_LAST - NOTE_FIRST) || ins->note_map[n] == NOTE_FIRST)
				break;
			ins->note_map[note_trans_sel_line] = ins->note_map[n] - 1;
			ins->sample_map[note_trans_sel_line] = ins->sample_map[n];
			new_line--;
			break;
		case SDLK_a:
			c = sample_get_current();
			for (n = 0; n < (NOTE_LAST - NOTE_FIRST + 1); n++)
				ins->sample_map[n] = c;
			if (k->mod & KMOD_SHIFT) {
				// Copy the name too.
				memcpy(ins->name, current_song->samples[c].name, 32);
			}
			break;
		default:
			return 0;
		}
	} else {
		switch (k->sym) {
		case SDLK_UP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_CTRL)
				sample_set(sample_get_current () - 1);
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (--new_line < 0) {
				change_focus_to(1);
				return 1;
			}
			break;
		case SDLK_DOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_CTRL)
				sample_set(sample_get_current () + 1);
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_line++;
			break;
		case SDLK_PAGEUP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_CTRL) {
				instrument_set(current_instrument - 1);
				return 1;
			}
			new_line -= 16;
			break;
		case SDLK_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & KMOD_CTRL) {
				instrument_set(current_instrument + 1);
				return 1;
			}
			new_line += 16;
			break;
		case SDLK_HOME:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_line = 0;
			break;
		case SDLK_END:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_line = 119;
			break;
		case SDLK_LEFT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_pos--;
			break;
		case SDLK_RIGHT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_pos++;
			break;
		case SDLK_RETURN:
			if (k->state == KEY_PRESS)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			sample_set(ins->sample_map[note_trans_sel_line]);
			get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
			return 1;
		case SDLK_LESS:
		case SDLK_SEMICOLON:
		case SDLK_COLON:
			if (k->state == KEY_RELEASE)
				return 0;
			sample_set(sample_get_current() - 1);
			return 1;
		case SDLK_GREATER:
		case SDLK_QUOTE:
		case SDLK_QUOTEDBL:
			if (k->state == KEY_RELEASE)
				return 0;
			sample_set(sample_get_current() + 1);
			return 1;

		default:
			if (k->state == KEY_RELEASE)
				return 0;
			switch (note_trans_cursor_pos) {
			case 0:        /* note */
				n = kbd_get_note(k);
				if (!NOTE_IS_NOTE(n))
					return 0;
				ins->note_map[note_trans_sel_line] = n;
				if (note_sample_mask || (status.flags & CLASSIC_MODE))
					ins->sample_map[note_trans_sel_line] = sample_get_current();
				new_line++;
				break;
			case 1:        /* octave */
				c = kbd_char_to_hex(k);
				if (c < 0 || c > 9) return 0;
				n = ins->note_map[note_trans_sel_line];
				n = ((n - 1) % 12) + (12 * c) + 1;
				ins->note_map[note_trans_sel_line] = n;
				new_line++;
				break;

				/* Made it possible to enter H to R letters
				on 1st digit for expanded sample slots.  -delt. */

			case 2:        /* instrument, first digit */
			case 3:        /* instrument, second digit */
				if (k->sym == SDLK_SPACE) {
					ins->sample_map[note_trans_sel_line] =
						sample_get_current();
					new_line++;
					break;
				}

				if ((k->sym == SDLK_PERIOD && NO_MODIFIER(k->mod)) || k->sym == SDLK_DELETE) {
					ins->sample_map[note_trans_sel_line] = 0;
					new_line += (k->sym == SDLK_PERIOD) ? 1 : 0;
					break;
				}
				if (k->sym == SDLK_COMMA && NO_MODIFIER(k->mod)) {
					note_sample_mask = note_sample_mask ? 0 : 1;
					break;
				}

				n = ins->sample_map[note_trans_sel_line];
				if (note_trans_cursor_pos == 2) {
					c = kbd_char_to_99(k);
					if (c < 0) return 0;
					n = (c * 10) + (n % 10);
					new_pos++;
				} else {
					c = kbd_char_to_hex(k);
					if (c < 0 || c > 9) return 0;
					n = ((n / 10) * 10) + c;
					new_pos--;
					new_line++;
				}
				n = MIN(n, MAX_SAMPLES - 1);
				ins->sample_map[note_trans_sel_line] = n;
				sample_set(n);
				break;
			}
			break;
		}
	}

	new_line = CLAMP(new_line, 0, 119);
	note_trans_cursor_pos = CLAMP(new_pos, 0, 3);
	if (new_line != prev_line) {
		note_trans_sel_line = new_line;
		note_trans_reposition();
	}

	/* this causes unneeded redraws in some cases... oh well :P */
	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* envelope helper functions */

static void _env_draw_axes(int middle)
{
	int n, y = middle ? 31 : 62;
	for (n = 0; n < 64; n += 2)
		vgamem_ovl_drawpixel(&env_overlay, 3, n, 12);
	for (n = 0; n < 256; n += 2)
		vgamem_ovl_drawpixel(&env_overlay, 1 + n, y, 12);
}

static void _env_draw_node(int x, int y, int on)
{
	int c = (status.flags & CLASSIC_MODE) ? 12 : 5;

	vgamem_ovl_drawpixel(&env_overlay, x - 1, y - 1, c);
	vgamem_ovl_drawpixel(&env_overlay, x - 1, y, c);
	vgamem_ovl_drawpixel(&env_overlay, x - 1, y + 1, c);

	vgamem_ovl_drawpixel(&env_overlay, x, y - 1, c);
	vgamem_ovl_drawpixel(&env_overlay, x, y, c);
	vgamem_ovl_drawpixel(&env_overlay, x, y + 1, c);

	vgamem_ovl_drawpixel(&env_overlay, x + 1, y - 1,c);
	vgamem_ovl_drawpixel(&env_overlay, x + 1, y,c);
	vgamem_ovl_drawpixel(&env_overlay, x + 1, y + 1,c);

	if (on) {
		vgamem_ovl_drawpixel(&env_overlay, x - 3, y - 1,c);
		vgamem_ovl_drawpixel(&env_overlay, x - 3, y,c);
		vgamem_ovl_drawpixel(&env_overlay, x - 3, y + 1,c);

		vgamem_ovl_drawpixel(&env_overlay, x + 3, y - 1,c);
		vgamem_ovl_drawpixel(&env_overlay, x + 3, y,c);
		vgamem_ovl_drawpixel(&env_overlay, x + 3, y + 1,c);
	}
}

static void _env_draw_loop(int xs, int xe, int sustain)
{
	int y = 0;
	int c = (status.flags & CLASSIC_MODE) ? 12 : 3;

	if (sustain) {
		while (y < 62) {
			/* unrolled once */
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
		}
	} else {
		while (y < 62) {
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
		}
	}
}

static void _env_draw(const song_envelope_t *env, int middle, int current_node,
			int env_on, int loop_on, int sustain_on, int env_num)
{
	song_voice_t *channel;
	unsigned int *channel_list;
	char buf[16];
	unsigned int envpos[3];
	int x, y, n, m, c;
	int last_x = 0, last_y = 0;
	int max_ticks = 50;

	while (env->ticks[env->nodes - 1] >= max_ticks)
		max_ticks *= 2;

	vgamem_ovl_clear(&env_overlay, 0);

	/* draw the axis lines */
	_env_draw_axes(middle);

	for (n = 0; n < env->nodes; n++) {
		x = 4 + env->ticks[n] * 256 / max_ticks;

		/* 65 values are being crammed into 62 pixels => have to lose three pixels somewhere.
		 * This is where IT compromises -- I don't quite get how the lines are drawn, though,
		 * because it changes for each value... (apart from drawing 63 and 64 the same way) */
		y = env->values[n];
		if (y > 63) y--;
		if (y > 42) y--;
		if (y > 21) y--;
		y = 62 - y;

		_env_draw_node(x, y, n == current_node);

		if (last_x)
			vgamem_ovl_drawline(&env_overlay,
				last_x, last_y, x, y, 12);

		last_x = x;
		last_y = y;
	}

	if (sustain_on)
		_env_draw_loop(4 + env->ticks[env->sustain_start] * 256 / max_ticks,
			       4 + env->ticks[env->sustain_end] * 256 / max_ticks, 1);
	if (loop_on)
		_env_draw_loop(4 + env->ticks[env->loop_start] * 256 / max_ticks,
			       4 + env->ticks[env->loop_end] * 256 / max_ticks, 0);

	if (env_on) {
		max_ticks = env->ticks[env->nodes-1];
		m = max_ticks ? song_get_mix_state(&channel_list) : 0;
		while (m--) {
			channel = song_get_mix_channel(channel_list[m]);
			if (channel->ptr_instrument != song_get_instrument(current_instrument))
				continue;

			envpos[0] = channel->vol_env_position;
			envpos[1] = channel->pan_env_position;
			envpos[2] = channel->pitch_env_position;

			x = 4 + (envpos[env_num] * (last_x-4) / max_ticks);
			if (x > last_x)
				x = last_x;
			c =  (status.flags & CLASSIC_MODE)
				? 12
				: ((channel->flags & (CHN_KEYOFF | CHN_NOTEFADE)) ? 8 : 6);
			for (y = 0; y < 62; y++)
				vgamem_ovl_drawpixel(&env_overlay, x, y, c);
		}
	}

	draw_fill_chars(65, 18, 76, 25, 0);
	vgamem_ovl_apply(&env_overlay);

	sprintf(buf, "Node %d/%d", current_node, env->nodes);
	draw_text(buf, 66, 19, 2, 0);
	sprintf(buf, "Tick %d", env->ticks[current_node]);
	draw_text(buf, 66, 21, 2, 0);
	sprintf(buf, "Value %d", (int)(env->values[current_node] - (middle ? 32 : 0)));
	draw_text(buf, 66, 23, 2, 0);
}

/* return: the new current node */
static int _env_node_add(song_envelope_t *env, int current_node, int override_tick, int override_value)
{
	int newtick, newvalue;

	status.flags |= SONG_NEEDS_SAVE;

	if (env->nodes > 24 || current_node == env->nodes - 1)
		return current_node;

	newtick = (env->ticks[current_node] + env->ticks[current_node + 1]) / 2;
	newvalue = (env->values[current_node] + env->values[current_node + 1]) / 2;
	if (override_tick > -1 && override_value > -1) {
		newtick = override_tick;
		newvalue = override_value;
	} else if (newtick == env->ticks[current_node] || newtick == env->ticks[current_node + 1]) {
		printf("Not enough room!\n");
		return current_node;
	}

	env->nodes++;
	memmove(env->ticks + current_node + 1, env->ticks + current_node,
		(env->nodes - current_node - 1) * sizeof(env->ticks[0]));
	memmove(env->values + current_node + 1, env->values + current_node,
		(env->nodes - current_node - 1) * sizeof(env->values[0]));
	env->ticks[current_node + 1] = newtick;
	env->values[current_node + 1] = newvalue;
	if (env->loop_end > current_node) env->loop_end++;
	if (env->loop_start > current_node) env->loop_start++;
	if (env->sustain_end > current_node) env->sustain_end++;
	if (env->sustain_start > current_node) env->sustain_start++;

	return current_node;
}

/* --------------------------------------------------------------------- */
/* load_page functions */

void instrument_list_general_load_page(struct page *page)
{
//	_load_page_common(page, widgets_general);
//
//	page->draw_const = instrument_list_general_draw_const;
//	page->predraw_hook = instrument_list_general_predraw_hook;
//	page->total_widgets = 18;
//
//	/* special case stuff */
//	widgets_general[1].d.togglebutton.state = 1;
//	widgets_general[2].next.down = widgets_general[3].next.down = widgets_general[4].next.down = 6;
//
//	/* 5 = note trans table */
//	create_other(widgets_general + 5, 6, note_trans_handle_key, note_trans_draw);
//	widgets_general[5].x = 32;
//	widgets_general[5].y = 16;
//	widgets_general[5].width = 9;
//	widgets_general[5].height = 31;
//
//	/* 6-9 = nna toggles */
//	create_togglebutton(widgets_general + 6, 46, 19, 29, 2, 7, 5, 0, 0,
//			    instrument_list_general_update_values,
//			    "Note Cut", 2, nna_group);
//	create_togglebutton(widgets_general + 7, 46, 22, 29, 6, 8, 5, 0, 0,
//			    instrument_list_general_update_values,
//			    "Continue", 2, nna_group);
//	create_togglebutton(widgets_general + 8, 46, 25, 29, 7, 9, 5, 0, 0,
//			    instrument_list_general_update_values,
//			    "Note Off", 2, nna_group);
//	create_togglebutton(widgets_general + 9, 46, 28, 29, 8, 10, 5, 0, 0,
//			    instrument_list_general_update_values,
//			    "Note Fade", 2, nna_group);
//
//	/* 10-13 = dct toggles */
//	create_togglebutton(widgets_general + 10, 46, 34, 12, 9, 11, 5, 14,
//			    14, instrument_list_general_update_values,
//			    "Disabled", 2, dct_group);
//	create_togglebutton(widgets_general + 11, 46, 37, 12, 10, 12, 5, 15,
//			    15, instrument_list_general_update_values,
//			    "Note", 2, dct_group);
//	create_togglebutton(widgets_general + 12, 46, 40, 12, 11, 13, 5, 16,
//			    16, instrument_list_general_update_values,
//			    "Sample", 2, dct_group);
//	create_togglebutton(widgets_general + 13, 46, 43, 12, 12, 17, 5, 13,
//			    13, instrument_list_general_update_values,
//			    "Instrument", 2, dct_group);
//	/* 14-16 = dca toggles */
//	create_togglebutton(widgets_general + 14, 62, 34, 13, 9, 15, 10, 0,
//			    0, instrument_list_general_update_values,
//			    "Note Cut", 2, dca_group);
//	create_togglebutton(widgets_general + 15, 62, 37, 13, 14, 16, 11, 0,
//			    0, instrument_list_general_update_values,
//			    "Note Off", 2, dca_group);
//	create_togglebutton(widgets_general + 16, 62, 40, 13, 15, 17, 12, 0,
//			    0, instrument_list_general_update_values,
//			    "Note Fade", 2, dca_group);
//	/* 17 = filename */
//	/* impulse tracker has a 17-char-wide box for the filename for
//	 * some reason, though it still limits the actual text to 12
//	 * characters. go figure... */
//	create_textentry(widgets_general + 17, 56, 47, 13, 13, 17, 0, update_filename,
//			 NULL, 12);
}

void instrument_list_volume_load_page(struct page *page)
{
//	_load_page_common(page, widgets_volume);
//
//	page->pre_handle_key = _fixup_mouse_instpage_volume;
//	page->draw_const = instrument_list_volume_draw_const;
//	page->predraw_hook = instrument_list_volume_predraw_hook;
//	page->total_widgets = 17;
//
//	/* 5 = volume envelope */
//	create_other(widgets_volume + 5, 0, volume_envelope_handle_key, volume_envelope_draw);
//	widgets_volume[5].x = 32;
//	widgets_volume[5].y = 18;
//	widgets_volume[5].width = 45;
//	widgets_volume[5].height = 8;
//
//	/* 6-7 = envelope switches */
//	create_toggle(widgets_volume + 6, 54, 28, 5, 7, 0, 0, 0,
//		      instrument_list_volume_update_values);
//	create_toggle(widgets_volume + 7, 54, 29, 6, 8, 0, 0, 0,
//		      instrument_list_volume_update_values);
//
//	/* 8-10 envelope loop settings */
//	create_toggle(widgets_volume + 8, 54, 32, 7, 9, 0, 0, 0,
//		      instrument_list_volume_update_values);
//	create_numentry(widgets_volume + 9, 54, 33, 3, 8, 10, 0,
//			instrument_list_volume_update_values, 0, 1,
//			numentry_cursor_pos + 0);
//	create_numentry(widgets_volume + 10, 54, 34, 3, 9, 11, 0,
//			instrument_list_volume_update_values, 0, 1,
//			numentry_cursor_pos + 0);
//
//	/* 11-13 = susloop settings */
//	create_toggle(widgets_volume + 11, 54, 37, 10, 12, 0, 0, 0,
//		      instrument_list_volume_update_values);
//	create_numentry(widgets_volume + 12, 54, 38, 3, 11, 13, 0,
//			instrument_list_volume_update_values, 0, 1,
//			numentry_cursor_pos + 0);
//	create_numentry(widgets_volume + 13, 54, 39, 3, 12, 14, 0,
//			instrument_list_volume_update_values, 0, 1,
//			numentry_cursor_pos + 0);
//
//	/* 14-16 = volume thumbbars */
//	create_thumbbar(widgets_volume + 14, 54, 42, 17, 13, 15, 0,
//			instrument_list_volume_update_values, 0, 128);
//	create_thumbbar(widgets_volume + 15, 54, 43, 17, 14, 16, 0,
//			instrument_list_volume_update_values, 0, 256);
//	create_thumbbar(widgets_volume + 16, 54, 46, 17, 15, 16, 0,
//			instrument_list_volume_update_values, 0, 100);
}

void instrument_list_panning_load_page(struct page *page)
{
//	_load_page_common(page, widgets_panning);
//
//	page->pre_handle_key = _fixup_mouse_instpage_panning;
//	page->draw_const = instrument_list_panning_draw_const;
//	page->predraw_hook = instrument_list_panning_predraw_hook;
//	page->total_widgets = 19;
//
//	/* 5 = panning envelope */
//	create_other(widgets_panning + 5, 0, panning_envelope_handle_key, panning_envelope_draw);
//	widgets_panning[5].x = 32;
//	widgets_panning[5].y = 18;
//	widgets_panning[5].width = 45;
//	widgets_panning[5].height = 8;
//
//	/* 6-7 = envelope switches */
//	create_toggle(widgets_panning + 6, 54, 28, 5, 7, 0, 0, 0,
//		      instrument_list_panning_update_values);
//	create_toggle(widgets_panning + 7, 54, 29, 6, 8, 0, 0, 0,
//		      instrument_list_panning_update_values);
//
//	/* 8-10 envelope loop settings */
//	create_toggle(widgets_panning + 8, 54, 32, 7, 9, 0, 0, 0,
//		      instrument_list_panning_update_values);
//	create_numentry(widgets_panning + 9, 54, 33, 3, 8, 10, 0,
//			instrument_list_panning_update_values, 0, 1,
//			numentry_cursor_pos + 1);
//	create_numentry(widgets_panning + 10, 54, 34, 3, 9, 11, 0,
//			instrument_list_panning_update_values, 0, 1,
//			numentry_cursor_pos + 1);
//
//	/* 11-13 = susloop settings */
//	create_toggle(widgets_panning + 11, 54, 37, 10, 12, 0, 0, 0,
//		      instrument_list_panning_update_values);
//	create_numentry(widgets_panning + 12, 54, 38, 3, 11, 13, 0,
//			instrument_list_panning_update_values, 0, 1,
//			numentry_cursor_pos + 1);
//	create_numentry(widgets_panning + 13, 54, 39, 3, 12, 14, 0,
//			instrument_list_panning_update_values, 0, 1,
//			numentry_cursor_pos + 1);
//
//	/* 14-15 = default panning */
//	create_toggle(widgets_panning + 14, 54, 42, 13, 15, 0, 0, 0,
//		      instrument_list_panning_update_values);
//	create_thumbbar(widgets_panning + 15, 54, 43, 9, 14, 16, 0,
//			instrument_list_panning_update_values, 0, 64);
//
//	/* 16 = pitch-pan center */
//	create_other(widgets_panning + 16, 0, pitch_pan_center_handle_key, pitch_pan_center_draw);
//	widgets_panning[16].next.up = 15;
//	widgets_panning[16].next.down = 17;
//
//	/* 17-18 = other panning stuff */
//	create_thumbbar(widgets_panning + 17, 54, 46, 9, 16, 18, 0,
//			instrument_list_panning_update_values, -32, 32);
//	create_thumbbar(widgets_panning + 18, 54, 47, 9, 17, 18, 0,
//			instrument_list_panning_update_values, 0, 64);
}

void instrument_list_pitch_load_page(struct page *page)
{
//	static int midi_channel_selection_cursor_position = 0;
//
//	_load_page_common(page, widgets_pitch);
//
//	page->pre_handle_key = _fixup_mouse_instpage_pitch;
//	page->draw_const = instrument_list_pitch_draw_const;
//	page->predraw_hook = instrument_list_pitch_predraw_hook;
//	page->total_widgets = 20;
//
//	/* 5 = pitch envelope */
//	create_other(widgets_pitch + 5, 0, pitch_envelope_handle_key, pitch_envelope_draw);
//	widgets_pitch[5].x = 32;
//	widgets_pitch[5].y = 18;
//	widgets_pitch[5].width = 45;
//	widgets_pitch[5].height = 8;
//
//	/* 6-7 = envelope switches */
//	create_menutoggle(widgets_pitch + 6, 54, 28, 5, 7, 0, 0, 0,
//		      instrument_list_pitch_update_values, pitch_envelope_states);
//	create_toggle(widgets_pitch + 7, 54, 29, 6, 8, 0, 0, 0,
//		      instrument_list_pitch_update_values);
//
//	/* 8-10 envelope loop settings */
//	create_toggle(widgets_pitch + 8, 54, 32, 7, 9, 0, 0, 0,
//		      instrument_list_pitch_update_values);
//	create_numentry(widgets_pitch + 9, 54, 33, 3, 8, 10, 0,
//			instrument_list_pitch_update_values, 0, 1,
//			numentry_cursor_pos + 2);
//	create_numentry(widgets_pitch + 10, 54, 34, 3, 9, 11, 0,
//			instrument_list_pitch_update_values, 0, 1,
//			numentry_cursor_pos + 2);
//
//	/* 11-13 = susloop settings */
//	create_toggle(widgets_pitch + 11, 54, 37, 10, 12, 0, 0, 0,
//		      instrument_list_pitch_update_values);
//	create_numentry(widgets_pitch + 12, 54, 38, 3, 11, 13, 0,
//			instrument_list_pitch_update_values, 0, 1,
//			numentry_cursor_pos + 2);
//	create_numentry(widgets_pitch + 13, 54, 39, 3, 12, 14, 0,
//			instrument_list_pitch_update_values, 0, 1,
//			numentry_cursor_pos + 2);
//
//	/* 14-15 = filter cutoff/resonance */
//	create_thumbbar(widgets_pitch + 14, 54, 42, 17, 13, 15, 0,
//			instrument_list_pitch_update_values, -1, 127);
//	create_thumbbar(widgets_pitch + 15, 54, 43, 17, 14, 16, 0,
//			instrument_list_pitch_update_values, -1, 127);
//	widgets_pitch[14].d.thumbbar.text_at_min = "Off";
//	widgets_pitch[15].d.thumbbar.text_at_min = "Off";
//
//	/* 16-19 = midi crap */
//	create_bitset(widgets_pitch + 16, 54, 44, 17, 15, 17, 0,
//			instrument_list_pitch_update_values,
//			17,
//			" 1 2 3 4 5 6 7 8 9P\0""111213141516M\0",
//			".\0.\0.\0.\0.\0.\0.\0.\0.\0p\0.\0.\0.\0.\0.\0.\0m\0",
//			&midi_channel_selection_cursor_position
//			);
//	widgets_pitch[16].d.bitset.activation_keys =
//		"123456789pabcdefm";
//
//	create_thumbbar(widgets_pitch + 17, 54, 45, 17, 16, 18, 0,
//			instrument_list_pitch_update_values, -1, 127);
//	create_thumbbar(widgets_pitch + 18, 54, 46, 17, 17, 19, 0,
//			instrument_list_pitch_update_values, -1, 127);
//	create_thumbbar(widgets_pitch + 19, 54, 47, 17, 18, 19, 0,
//			instrument_list_pitch_update_values, -1, 127);
//	widgets_pitch[17].d.thumbbar.text_at_min = "Off";
//	widgets_pitch[18].d.thumbbar.text_at_min = "Off";
//	widgets_pitch[19].d.thumbbar.text_at_min = "Off";
}

