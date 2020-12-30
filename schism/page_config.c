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

#include "it.h"
#include "song.h"
#include "page.h"

#include "sdlmain.h"

#include "snd_gm.h"

#include "disko.h"

/* --------------------------------------------------------------------- */

#define SAVED_AT_EXIT "System configuration will be saved at exit"

/* --------------------------------------------------------------------- */
void config_load_page(struct page *page)
{
//	page->title = "System Configuration (Ctrl-F1)";
//	page->draw_const = config_draw_const;
//	page->set_page = config_set_page;
//	page->total_widgets = 15;
//	page->widgets = widgets_config;
//	page->help_index = HELP_GLOBAL;
//
//	create_thumbbar(widgets_config+0,
//			18, 15, 17,
//			0,1,1,
//			change_mixer_limits, 4, 256);
//	create_numentry(widgets_config+1,
//			18, 16, 7,
//			0,2,2,
//			change_mixer_limits,
//			4000, 192000,
//			&sample_rate_cursor);
//	create_menutoggle(widgets_config+2,
//			18, 17,
//			1,3,2,2,3,
//			change_mixer_limits,
//			bit_rates);
//	create_menutoggle(widgets_config+3,
//			18, 18,
//			2,4,3,3,4,
//			change_mixer_limits,
//			output_channels);
//    ////
//	create_menutoggle(widgets_config+4,
//			18, 20,
//			3,5,4,4,5,
//			change_ui_settings,
//			vis_styles);
//	create_toggle(widgets_config+5,
//			18, 21,
//			4,6,5,5,6,
//			change_ui_settings);
//	create_menutoggle(widgets_config+6,
//			18, 22,
//			5,7,6,6,7,
//			change_ui_settings,
//			sharp_flat);
//	create_menutoggle(widgets_config+7,
//			18, 23,
//			6,8,7,7,8,
//			change_ui_settings,
//			time_displays);
//    ////
//	create_menutoggle(widgets_config+8,
//			18, 25,
//			7,11,8,8,11,
//			change_ui_settings,
//			midi_modes);
//    ////
//	create_togglebutton(widgets_config+9,
//			44, 30, 5,
//			8,9,11,10,10,
//			change_video_settings,
//			"Yes",
//			2, video_fs_group);
//	create_togglebutton(widgets_config+10,
//			54, 30, 5,
//			10,10,9,10,0,
//			change_video_settings,
//			"No",
//			2, video_fs_group);
//    ////
//	create_togglebutton(widgets_config+11,
//			6, 30, 26,
//			8,12,11,9,12,
//			change_video_settings,
//			"SDL Video Surface",
//			2, video_group);
//
//	create_togglebutton(widgets_config+12,
//			6, 33, 26,
//			11,13,12,9,13,
//			change_video_settings,
//			"YUV Video Overlay",
//			2, video_group);
//
//	create_togglebutton(widgets_config+13,
//			6, 36, 26,
//			12,14,13,9,14,
//			change_video_settings,
//			"OpenGL Graphic Context",
//			2, video_group);
//
//	create_togglebutton(widgets_config+14,
//			6, 39, 26,
//			13,14,14,9,9,
//			change_video_settings,
//			"DirectDraw Surface",
//			2, video_group);
//#ifndef WIN32
//	/* patch ddraw out */
//	video_group[3] = -1;
//	widgets_config[14].d.togglebutton.state = 0;
//	widgets_config[13].next.down = 13;
//	widgets_config[13].next.tab = 9;
//	page->total_widgets--;
//#endif

}
