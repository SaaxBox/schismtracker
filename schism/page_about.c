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

#include "headers.h"
#include "it.h"
#include "page.h"
#include "video.h"
#include "song.h"
#include "version.h"

/* Eventual TODO: draw the pattern data in the Schism logo in a different color than the words */
#include "auto/logoit.h"
#include "auto/logoschism.h"

#define LOGO_WIDTH 292
#define LOGO_PITCH 292
#define LOGO_HEIGHT 50


static int _fixup_ignore_globals(struct key_event *k)
{
	if (k->mouse && k->y > 20) return 0;
	switch (k->sym) {
	case SDLK_LEFT:
	case SDLK_RIGHT:
	case SDLK_DOWN:
	case SDLK_UP:
	case SDLK_TAB:
	case SDLK_RETURN:
	case SDLK_ESCAPE:
		/* use default handler */
		return 0;
	case SDLK_F2: case SDLK_F5: case SDLK_F9: case SDLK_F10:
		// Ctrl + these keys does not lead to a new screen
		if (k->mod & KMOD_CTRL)
			break;
		// Fall through.
	case SDLK_F1: case SDLK_F3: case SDLK_F4:
	case SDLK_F11: case SDLK_F12:
		// Ignore Alt and so on.
		if (k->mod & (KMOD_ALT | KMOD_SHIFT))
			break;
//		dialog_destroy();
		return 0;
	default:
		break;
	}
	/* this way, we can't pull up help here */
	return 1;
}

static void _draw_full(void)
{
	draw_fill_chars(0,0,79,49,0);
}

void about_load_page(struct page *page)
{
	page->title = "";
	page->total_widgets = 0;
	page->widgets = NULL;
	page->pre_handle_key = _fixup_ignore_globals;
	page->help_index = HELP_COPYRIGHT;
	page->draw_full = _draw_full;
	page->set_page = show_about;
}

void show_about(void)
{
//	static int didit = 0;
//	struct dialog *d;
//	unsigned char *p;
//	int x, y;
//
//	fake_driver = (rand() & 3) ? 0 : 1;
//
//	if (!didit) {
//		vgamem_ovl_alloc(&logo_image);
//		it_logo = xpmdata(_logo_it_xpm);
//		schism_logo = xpmdata(_logo_schism_xpm);
//		didit=1;
//	}
//
//	if (status.flags & CLASSIC_MODE) {
//		p = it_logo ? it_logo->pixels : NULL;
//	} else {
//		p = schism_logo ? schism_logo->pixels : NULL;
//	}
//
//	/* this is currently pretty gross */
//	vgamem_ovl_clear(&logo_image, 2);
//	if (p) {
//		int c = (status.flags & CLASSIC_MODE) ? 11 : 0;
//		for (y = 0; y < LOGO_HEIGHT; y++) {
//			for (x = 0; x < LOGO_WIDTH; x++) {
//				if (p[x]) {
//					vgamem_ovl_drawpixel(&logo_image, x+2, y+6, c);
//				}
//			}
//			vgamem_ovl_drawpixel(&logo_image, x, y+6, 2);
//			vgamem_ovl_drawpixel(&logo_image, x+1, y+6, 2);
//			p += LOGO_PITCH;
//		}
//	}
//
//	create_button(widgets_about + 0,
//			33,32,
//			12,
//			0,0,0,0,0,
//			dialog_yes_NULL, "Continue", 3);
//	d = dialog_create_custom(11,16,
//			58, 19,
//			widgets_about, 1, 0,
//			about_draw_const, NULL);
//	d->action_yes = about_close;
//	d->action_no = about_close;
//	d->action_cancel = about_close;
//
//	/* okay, in just a moment, we're going to the module page.
//	 * if your modules dir is large enough, this causes an annoying pause.
//	 * to defeat this, we start scanning *NOW*. this makes startup "feel"
//	 * faster.
//	 */
//	status.flags |= DIR_MODULES_CHANGED;
//	pages[PAGE_LOAD_MODULE].set_page();
}

