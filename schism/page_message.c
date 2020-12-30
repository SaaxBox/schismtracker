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

/* --->> WARNING <<---
 *
 * This is an excellent example of how NOT to write a text editor.
 * IMHO, the best way to add a song message is by writing it in some
 * other program and attaching it to the song with something like
 * ZaStaR's ITTXT utility (hmm, maybe I should rewrite that, too ^_^) so
 * I'm not *really* concerned about the fact that this code completely
 * sucks. Just remember, this ain't Xcode. */

#include "headers.h"

#include "song.h"
//#include "clippy.h"

#include <ctype.h>
#include <assert.h>

/* --------------------------------------------------------------------- */

