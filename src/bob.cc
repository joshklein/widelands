/*
 * Copyright (C) 2002-2004, 2006 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include "animation.h"
#include "bob.h"
#include "critter_bob.h"
#include "game.h"
#include "map.h"
#include "mapviewpixelconstants.h"
#include "player.h"
#include "profile.h"
#include "rendertarget.h"
#include "transport.h"
#include "tribe.h"
#include "wexception.h"


/*
==============================================================================

Bob IMPLEMENTATION

==============================================================================
*/

/*
===============
Bob_Descr::Bob_Descr
Bob_Descr::~Bob_Descr
===============
*/
Bob_Descr::Bob_Descr(const char *name, Tribe_Descr* owner_tribe)
{
	snprintf(m_name, sizeof(m_name), "%s", name);
   m_default_encodedata.clear();
   m_owner_tribe=owner_tribe;
}

Bob_Descr::~Bob_Descr(void)
{
}


/*
===============
Bob_Descr::parse

Parse additional information from the config file
===============
*/
void Bob_Descr::parse(const char *directory, Profile *prof, const EncodeData *encdata)
{
	char picname[256];
   char buf[256];

   Section* global = prof->get_safe_section("idle");

   // Global options
	snprintf(buf, sizeof(buf), "%s_00.png", m_name);
	snprintf(picname, sizeof(picname), "%s/%s", directory, global->get_string("picture", buf));
   m_picture = picname;

   m_default_encodedata.parse(global);

	snprintf(picname, sizeof(picname), "%s_??.png", m_name);
	add_animation("idle", g_anim.get(directory, prof->get_safe_section("idle"), picname, encdata));

	// Parse attributes
   const char* string;
	global= prof->get_safe_section("global");
   while(global->get_next_string("attrib", &string)) {
		uint attrib = get_attribute_id(string);

		if (attrib < Map_Object::HIGHEST_FIXED_ATTRIBUTE)
		{
         throw wexception("Bad attribute '%s'", string);
		}

		add_attribute(attrib);
	}
}


/*
===============
Bob_Descr::create

Create a bob of this type
===============
*/
Bob *Bob_Descr::create(Editor_Game_Base *gg, Player *owner, Coords coords)
{
   Bob *bob = create_object();
   bob->set_owner(owner);
   bob->set_position(gg, coords);
   bob->init(gg);

   return bob;
}


/*
==============================

IMPLEMENTATION

==============================
*/

/*
===============
Bob::Bob

Zero-initialize a map object
===============
*/
Bob::Bob(Bob_Descr* descr)
	: Map_Object(descr)
{
	m_owner = 0;
	m_position.x = m_position.y = 0; // not linked anywhere
	m_position.field = 0;
	m_linknext = 0;
	m_linkpprev = 0;

	m_actid = 0; // this isn't strictly necessary, but it shuts up valgrind and "feels" cleaner

	m_anim = 0;
	m_animstart = 0;

	m_walking = IDLE;
	m_walkstart = m_walkend = 0;

	m_stack_dirty = false;
	m_sched_init_task = false;
}


/*
===============
Bob::~Bob()

Cleanup an object. Removes map links
===============
*/
Bob::~Bob()
{
	if (m_position.field) {
		molog("Map_Object::~Map_Object: m_pos.field != 0, cleanup() not called!\n");
		*(int *)0 = 0;
	}
}


/*
===============
Bob::get_type
===============
*/
int Bob::get_type() const throw () {return BOB;}


/*
Bobs and tasks
--------------

Bobs have a call-stack of "tasks". The top-most task is the one that is
currently being executed. As soon as the task is finished (either successfully
or with an error signal), it is popped from the top of the stack, and the task
that is now on top will be asked to update() itself.

Upon initialization, an object has no task at all. A CMD_ACT will be scheduled
automatically. When it is executed, init_auto_task() is called to automatically
select a fallback task.
However, the creator of the bob can choose to push a specific task immediately
after creating the bob. This will override the fallback behaviour.
init_auto_task() is also called when the final task is popped from the stack.

All state information that a task uses must be stored in the State structure
returned by get_state(). Note that every task on the task stack has its own
state structure, i.e. push_task() does not destroy the current task's state.

In order to start a new sub-task, you have to call push_task(), and then fill
the State structure returned by get_state() with any parameters that the task
may need.
A task is ended by pop_task(). Note, however, that you should only call
pop_task() from a task's update() or signal() function.
If you want to interrupt the current task for some reason, you should call
send_signal().

To implement a new task, you need to create a new Task object, and at least an
update() function. The signal() and mask() functions are optional (can be 0).
The update() function is called once after the task is pushed, and whenever a
previously scheduled CMD_ACT occured. It is also called when a sub-task is pops
itself.
update() must call either schedule_act() or skip_act() if you really don't
want a CMD_ACT to occur. Note that if you call skip_act(), your task MUST
implement signal().
Alternatively, update() can call pop_task() to end the current task.

signal() is usually called by send_signal().
signal() is also called when a sub-task returns while a signal is still set.
Note that after signal() called, update() is also called.
signal() must call schedule_act(), or pop_task(), or do nothing at all.
If signal() is not implemented, it is equivalent to a signal() function that
does nothing at all.

Whenever send_signal() is called, the mask() function of all tasks are called,
starting with the highest-level task.
The mask() function cannot schedule CMD_ACT, but it can use set_signal() to
modify or even clear the signal before it reaches the normal signal handling
functions.
*/

/*
===============
Bob::init

Make sure you call this from derived classes!

Initialize the object
===============
*/
void Bob::init(Editor_Game_Base* gg)
{
   Map_Object::init(gg);

   m_sched_init_task = true;

	Game * const game = dynamic_cast<Game * const>(gg);
	if (game) schedule_act(game, 1);
	else {// In editor: play idle task forever
      set_animation(gg, get_descr()->get_animation("idle"));
   }
}


/*
===============
Bob::cleanup

Perform independant cleanup as necessary.
===============
*/
void Bob::cleanup(Editor_Game_Base *gg)
{
	while(m_stack.size()) {
		pop_task((Game*)gg);
		m_stack_dirty = false;
	}

   if (m_position.field) {
      m_position.field = 0;
      *m_linkpprev = m_linknext;
      if (m_linknext)
         m_linknext->m_linkpprev = m_linkpprev;

   }
   Map_Object::cleanup(gg);
}


/*
===============
Bob::act

Called by Cmd_Queue when a CMD_ACT event is triggered.
Hand the acting over to the task.

Change to the next task if necessary.
===============
*/
void Bob::act(Game* g, uint data)
{
	if (!m_stack.size()) {
		assert(m_sched_init_task);

		m_sched_init_task = false;
		m_signal = "";

		m_stack_dirty = false;
		init_auto_task(g);
		m_sched_init_task = false;

		if (!m_stack.size())
			throw wexception("MO(%u): init_auto_task() failed to set a task", get_serial());

		assert(m_stack_dirty);

		m_stack_dirty = false;
		do_act(g, false);
		return;
	} else if (m_sched_init_task) {
		assert(m_stack_dirty);

		// This happens when we schedule for an init_auto_task(), but the bob
		// owner explicitly pushes a task
		m_sched_init_task = false;
		m_stack_dirty = false;
	}

	// Eliminate spurious calls of act().
	// These calls are to be expected and perfectly normal, e.g. when a carrier's
	// idle task is interrupted by the request to pick up a ware from a flag.
	if (data != m_actid)
		return;

	do_act(g, false);
}


/*
===============
Bob::do_act

Handle the actual calls to update() and signal() as appropriate.

signalhandling is true when do_act() is called by send_signal(), and false
otherwise.
===============
*/
void Bob::do_act(Game* g, bool signalhandling)
{
	for(;;) {
		uint origactid;

		origactid = m_actid;

		if (m_stack_dirty)
			throw wexception("MO(%u): stack dirty before update[%s]", get_serial(),
									get_state() ? get_state()->task->name : "(nil)");

		// Run the task if we're not coming from signalhandling
		if (!signalhandling) {
			Task* task = get_state()->task;

			(this->*task->update)(g, get_state());

			if (!m_stack_dirty)
			{
				if (origactid == m_actid)
					throw wexception("MO(%u): update[%s] failed to act", get_serial(), task->name);

				break; // we did our work, now get out of here
			}
			else if (origactid != m_actid)
				throw wexception("MO(%u): [%s] changes both stack and act", get_serial(),
									task->name);
		}

		do
		{
			m_stack_dirty = false;

			// Get a new task as soon as possible
			// Sometimes, the owner of a bob will want to force it to stop all
			// tasks and start a new task afterwards, which is why we don't
			// call init_auto_task() immediately.
			if (!m_stack.size()) {
				molog("schedule reget auto task\n");

				set_signal("");
				schedule_act(g, 1);
				m_sched_init_task = true;
				return;
			}

			// Prepare the new task
			if (m_signal.size())
			{
				Task* task = get_state()->task;

				if (task->signal)
					(this->*task->signal)(g, get_state());

				// If the initial signal handler doesn't mess with the stack, get out of here
				if (!m_stack_dirty && signalhandling)
					return;
			}
		} while(m_stack_dirty);

		signalhandling = false; // next pass will be a normal, non-signal handling pass
	}
}


/*
===============
Bob::schedule_destroy

Kill self ASAP.
===============
*/
void Bob::schedule_destroy(Game* g)
{
	Map_Object::schedule_destroy(g);
	m_actid++;
}


/*
===============
Bob::schedule_act

Schedule a new act for the current task. All other pending acts are cancelled.
===============
*/
void Bob::schedule_act(Game* g, uint tdelta)
{
	Map_Object::schedule_act(g, tdelta, ++m_actid);
}


/*
===============
Bob::skip_act

Explicitly state that we don't want to act.
===============
*/
void Bob::skip_act(Game *)
{
	if (!get_state()->task->signal)
		throw wexception("MO(%u): %s calls skip_act(), but has no signal() function",
					get_serial(), get_state()->task->name);

	m_actid++;
}


/*
===============
Bob::force_skip_act

Explicitly state that we don't want to act, even if we cannot be awoken by a
signal. Use with great care.
===============
*/
void Bob::force_skip_act(Game *)
{
	m_stack_dirty = false;
	m_actid++;
}


/*
===============
Bob::push_task

Push a new task onto the stack.

push_task() itself does not call any functions of the task, so the caller can
fill the state information with parameters for the task.
===============
*/
void Bob::push_task(Game *, Task * task)
{
   State* state;

   if (m_stack_dirty && m_stack.size())
      throw wexception("MO(%u): push_task(%s): stack already dirty", get_serial(), task->name);

   m_stack.push_back(State());

   state = get_state();
   state->task = task;
   state->ivar1 = 0;
   state->ivar2 = 0;
   state->diranims = 0;
   state->path = 0;
   state->transfer = 0;
   state->route = 0;
   state->program = 0;

   m_stack_dirty = true;
}


/*
===============
Bob::pop_task

Remove the current task from the stack.

pop_task() itself does not call any parent task functions, but it sets a flag
to make it happen.
===============
   */
void Bob::pop_task(Game *)
{
   State* state = get_state();

   if (m_stack_dirty)
      throw wexception("MO(%u): pop_task(%s): stack already dirty", get_serial(), state->task->name);

   if (state->path)
		delete state->path;
	if (state->route)
		delete state->route;
	if (state->transfer)
		state->transfer->has_failed();

   m_stack.pop_back();


	m_stack_dirty = true;
}


/*
===============
Bob::get_state

Get the bottom-most (usually the only) state of this task from the stack.
Return 0 if this task is not running at all.
===============
*/
Bob::State* Bob::get_state(Task* task)
{
	std::vector<State>::iterator it = m_stack.end();

	while(it != m_stack.begin())
	{
		--it;

		if (it->task == task)
			return &*it;
	}

	return 0;
}


/*
===============
Bob::send_signal

Sets the signal string and calls the current task's signal function, if any.
===============
*/
void Bob::send_signal(Game* g, std::string sig)
{
	assert(sig.size());	// use set_signal() for signal removal

	m_signal = sig;

	for(uint i = 0; i < m_stack.size(); i++) {
		State* state = &m_stack[i];

		if (state->task->mask) {
			(this->*state->task->mask)(g, state);

			if (!m_signal.size())
				return;
		}
	}

	do_act(g, true); // signal handler act
}


/*
===============
Bob::reset_tasks

Force a complete reset of the state stack.
The state stack is emptied completely, and an init auto task is scheduled
as if the Bob has just been created and initialized.
===============
*/
void Bob::reset_tasks(Game* g)
{
	while(m_stack.size()) {
		m_stack_dirty = false;

		pop_task(g);
	}

	set_signal("");
   schedule_act(g, 1);
   m_sched_init_task = true;
	m_stack_dirty = false;
}


/*
===============
Bob::set_signal

Simply set the signal string without calling any functions.
You should use this function to unset a signal, or to set a signal just before
calling pop_task().
===============
*/
void Bob::set_signal(std::string sig)
{
	m_signal = sig;
}


/*
===============
Bob::init_auto_task

Automatically select a task.
===============
*/
void Bob::init_auto_task(Game *) {}


/*
==============================

IDLE task

Wait a time or indefinitely.
Every signal can interrupt this task.  No signals are caught.

==============================
*/

Bob::Task Bob::taskIdle = {
	"idle",

	&Bob::idle_update,
	&Bob::idle_signal,
	0, // mask
};


/*
===============
Bob::start_task_idle

Start an idle phase, using the given animation
If the timeout is a positive value, the idle phase stops after the given
time.

This task always succeeds unless interrupted.
===============
*/
void Bob::start_task_idle(Game* g, uint anim, int timeout)
{
	State* state;

	assert(timeout < 0 || timeout > 0);

	set_animation(g, anim);

	push_task(g, &taskIdle);

	state = get_state();
	state->ivar1 = timeout;
}

void Bob::idle_update(Game* g, State* state)
{
	if (!state->ivar1) {
		pop_task(g);
		return;
	}

	if (state->ivar1 > 0)
		schedule_act(g, state->ivar1);
	else
		skip_act(g);

	state->ivar1 = 0;
}

void Bob::idle_signal(Game * g, State *) {pop_task(g);}


/*
==============================

MOVEPATH task

Move along a predefined path.
ivar1 is the step number.
ivar2 is non-zero if we should force moving onto the final field.
ivar3 is number of steps to take maximally or -1

Sets the following signal(s):
"fail" - cannot move along the given path

==============================
*/

Bob::Task Bob::taskMovepath = {
	"movepath",

	&Bob::movepath_update,
	&Bob::movepath_signal,		// lazy signal handling, only act on one signal
	0,		// mask
};

/*
===============
Bob::start_task_movepath

Start moving to the given destination. persist is the same parameter as
for Map::findpath().

Returns false if no path could be found.

The task finishes once the goal has been reached. It may fail.

only step defines how many steps should be taken, before this returns as a success
===============
*/
bool Bob::start_task_movepath
(Game* g,
 const Coords dest,
 int persist,
 const DirAnimations *anims,
 const bool forceonlast,
 const int only_step)
{
	Path* path = new Path;
	State* state;
	CheckStepDefault cstep_default(get_movecaps());
	CheckStepWalkOn cstep_walkon(get_movecaps(), true);
	CheckStep* cstep;

	if (forceonlast)
		cstep = &cstep_walkon;
	else
		cstep = &cstep_default;

	if (g->get_map()->findpath(m_position, dest, persist, *path, *cstep) < 0) {
		delete path;
		return false;
	}

	push_task(g, &taskMovepath);

	state = get_state();
	state->path = path;
	state->ivar1 = 0;		// step #
	state->ivar2 = forceonlast ? 1 : 0;
   state->ivar3 = only_step;
   state->diranims = anims;

	return true;
}


/*
===============
Bob::start_task_movepath

Start moving along the given, precalculated path.
===============
*/
void Bob::start_task_movepath
(Game* g,
 const Path & path,
 const DirAnimations *anims,
 const bool forceonlast,
 const int only_step)
{
	State* state;

	assert(path.get_start() == get_position());

	push_task(g, &taskMovepath);

	state = get_state();
	state->path = new Path(path);
	state->ivar1 = 0;
	state->ivar2 = forceonlast ? 1 : 0;
   state->ivar3 = only_step;
	state->diranims = anims;
}


/*
===============
Bob::start_task_movepath

Move to the given index on the given path. The current position must be on the
given path.

Return true if a task has been started, or false if we already are on the given
path index.
===============
*/
bool Bob::start_task_movepath
(Game* g,
 const Path & origpath,
 const int index,
 const DirAnimations* anims,
 const bool forceonlast,
 const int only_step)
{
	CoordPath path(origpath);
	int curidx = path.get_index(get_position());

	if (curidx == -1)
		throw wexception("MO(%u): start_task_movepath(index): not on path", get_serial());

	if (curidx != index) {
		if (curidx < index) {
			path.truncate(index);
			path.starttrim(curidx);

			start_task_movepath(g, path, anims, forceonlast, only_step);
		} else {
			path.truncate(curidx);
			path.starttrim(index);
			path.reverse();

			start_task_movepath(g, path, anims, forceonlast, only_step);
		}

		return true;
	}

	return false;
}


/*
===============
Bob::movepath_update
===============
*/
void Bob::movepath_update(Game* g, State* state)
{
	if (state->ivar1)
		end_walk(g);

	// We ignore signals when they arrive, but interrupt as soon as possible,
	// i.e. when the next step has finished
	if (get_signal().size()) {
		molog("[movepath]: Interrupted by signal '%s'.\n", get_signal().c_str());
		pop_task(g);
		return;
	}

	if (!state->path || state->ivar1 >= state->path->get_nsteps()) {
		assert(!state->path || m_position == state->path->get_end());
      pop_task(g); // success
		return;
	} else if(state->ivar1==state->ivar3) {
      // We have stepped all steps that we were asked for.
      // This is some kind of success, though we do not are were we wanted
      // to go
      pop_task(g);
      return;
   }

	char dir = state->path->get_step(state->ivar1);
	bool forcemove = false;

	if (state->ivar2 && (state->ivar1+1) == state->path->get_nsteps())
		forcemove = true;

	int tdelta = start_walk(g, (WalkingDir)dir, state->diranims->get_animation(dir), forcemove);
	if (tdelta < 0) {
		molog("[movepath]: Can't walk.\n");
		set_signal("fail"); // failure to reach goal
		pop_task(g);
		return;
	}

	state->ivar1++;

	schedule_act(g, tdelta);
}

/*
 * movepath signal
 */
void Bob::movepath_signal(Game * g, State *) {
   if(get_signal()=="interrupt_now") {
      // User requests an immediate interrupt.
      // Well, he better nows what he's doing
      pop_task(g);
   }
}

/*
==============================

FORCEMOVE task

==============================
*/

Bob::Task Bob::taskForcemove = {
	"forcemove",

	&Bob::forcemove_update,
	0,
	0,
};


/*
===============
Bob::start_task_forcemove

Move into the given direction, without passability checks.
===============
*/
void Bob::start_task_forcemove(Game *g, const int dir, const DirAnimations *anims) {
	State* state;

	push_task(g, &taskForcemove);

	state = get_state();
	state->ivar1 = dir;
	state->diranims = anims;
}

void Bob::forcemove_update(Game* g, State* state)
{
	if (state->diranims)
	{
		int tdelta = start_walk(g, (WalkingDir)state->ivar1,
		                        state->diranims->get_animation(state->ivar1), true);

		state->diranims = 0;
		schedule_act(g, tdelta);
	}
	else
	{
		end_walk(g);
		pop_task(g);
	}
}


/*
===============
Bob::calc_drawpos

Calculates the actual position to draw on from the base field position.
This function takes walking etc. into account.

pos is the location, in pixels, of the field m_position (height is already
taken into account).
===============
*/
Point Bob::calc_drawpos(const Editor_Game_Base & game, const Point pos) const {
	const Map & map = game.get_map();
	const FCoords end = m_position;
	FCoords start;
	Point spos = pos, epos = pos;

	switch(m_walking) {
	case WALK_NW:
		map.get_brn(end, &start);
		spos.x += TRIANGLE_WIDTH / 2;
		spos.y += TRIANGLE_HEIGHT;
		break;
	case WALK_NE:
		map.get_bln(end, &start);
		spos.x -= TRIANGLE_WIDTH / 2;
		spos.y += TRIANGLE_HEIGHT;
		break;
	case WALK_W:
		map.get_rn(end, &start);
		spos.x += TRIANGLE_WIDTH;
		break;
	case WALK_E:
		map.get_ln(end, &start);
		spos.x -= TRIANGLE_WIDTH;
		break;
	case WALK_SW:
		map.get_trn(end, &start);
		spos.x += TRIANGLE_WIDTH / 2;
		spos.y -= TRIANGLE_HEIGHT;
		break;
	case WALK_SE:
		map.get_tln(end, &start);
		spos.x -= TRIANGLE_WIDTH / 2;
		spos.y -= TRIANGLE_HEIGHT;
		break;

	case IDLE: start.field = 0; break;
	}

	if (start.field) {
		spos.y += end.field->get_height() * HEIGHT_FACTOR;
		spos.y -= start.field->get_height() * HEIGHT_FACTOR;

		float f =
			static_cast<float>(game.get_gametime() - m_walkstart)
			/
			(m_walkend - m_walkstart);
		if (f < 0) f = 0;
		else if (f > 1) f = 1;

		epos.x = static_cast<int>(f * epos.x + (1 - f) * spos.x);
		epos.y = static_cast<int>(f * epos.y + (1 - f) * spos.y);
	}

	return epos;
}


/*
===============
Bob::draw

Draw the map object.
posx/posy is the on-bitmap position of the field we're currently on.

It LERPs between start and end position when we're walking.
Note that the current field is actually the field we're walking to, not
the one we start from.
===============
*/
void Bob::draw
(const Editor_Game_Base & game, RenderTarget & dst, const Point pos) const
{
	if (!m_anim)
		return;

	const Point drawpos = calc_drawpos(game, pos);

	dst.drawanim
		(drawpos.x, drawpos.y,
		 m_anim,
		 game.get_gametime() - m_animstart,
		 get_owner());
}


/*
===============
Bob::set_animation

Set a looping animation, starting now.
===============
*/
void Bob::set_animation(Editor_Game_Base* g, uint anim)
{
	m_anim = anim;
	m_animstart = g->get_gametime();
}

/*
===============
Bob::is_walking

Return true if we're currently walking
===============
*/
bool Bob::is_walking()
{
	return m_walking != IDLE;
}

/*
===============
Bob::end_walk

Call this from your task_act() function that was scheduled after start_walk().
===============
*/
void Bob::end_walk(Game *) {m_walking = IDLE;}


/*
===============
Bob::start_walk

Cause the object to walk, honoring passable/impassable parts of the map using movecaps.
If force is true, the passability check is skipped.

Returns the number of milliseconds after which the walk has ended. You must
call end_walk() after this time, so schedule a task_act().

Returns a negative value when we can't walk into the requested direction.
===============
*/
int Bob::start_walk(Game *g, WalkingDir dir, uint a, bool force)
{
	FCoords newf;

	g->get_map()->get_neighbour(m_position, dir, &newf);

	// Move capability check by ANDing with the field caps
	//
	// The somewhat crazy check involving MOVECAPS_SWIM should allow swimming objects to
	// temporarily land.
	uint movecaps = get_movecaps();

	if (!force) {
		if (!(m_position.field->get_caps() & movecaps & MOVECAPS_SWIM && newf.field->get_caps() & MOVECAPS_WALK) &&
		    !(newf.field->get_caps() & movecaps))
			return -1;
	}

	// Move is go
	int tdelta = g->get_map()->calc_cost(m_position, dir);

	m_walking = dir;
	m_walkstart = g->get_gametime();
	m_walkend = m_walkstart + tdelta;

	set_position(g, newf);
	set_animation(g, a);

	return tdelta; // yep, we were successful
}

/*
===============
Bob::set_position

Moves the Map_Object to the given position.
===============
*/
void Bob::set_position(Editor_Game_Base* g, Coords coords)
{
	if (m_position.field) {
		*m_linkpprev = m_linknext;
		if (m_linknext)
			m_linknext->m_linkpprev = m_linkpprev;
	}

	m_position = g->get_map()->get_fcoords(coords);

	m_linknext = m_position.field->bobs;
	m_linkpprev = &m_position.field->bobs;
	if (m_linknext)
		m_linknext->m_linkpprev = &m_linknext;
	m_position.field->bobs = this;
}
/*
 * Give debug informations
 */
void Bob::log_general_info(Editor_Game_Base* egbase) {
   molog("Owner: %p\n", m_owner);

   molog("Postition: (%i,%i)\n", m_position.x, m_position.y);
   molog("ActID: %i\n", m_actid);

	molog("Animation: %s\n", m_anim ? get_descr()->get_animation_name(m_anim).c_str() : "<none>" );
   molog("AnimStart: %i\n", m_animstart);

	molog("WalkingDir: %i\n", m_walking);
	molog("WalkingStart: %i\n", m_walkstart);
	molog("WalkEnd: %i\n", m_walkend);

	molog("Stack dirty: %i\n", m_stack_dirty);
	molog("Init auto task: %i\n", m_sched_init_task);
	molog("Signale: %s\n", m_signal.c_str());

   molog("Stack size: %i\n", m_stack.size());
   for(uint i=0; i<m_stack.size(); i++) {
      molog("Stack dump %i/%i\n", i+1, m_stack.size());

      molog("* task->name: %s\n", m_stack[i].task->name);

      molog("* ivar1: %i\n", m_stack[i].ivar1);
      molog("* ivar2: %i\n", m_stack[i].ivar2);
      molog("* ivar3: %i\n", m_stack[i].ivar3);

      molog("* object pointer: %p\n", m_stack[i].objvar1.get(egbase));
		molog("* svar1: %s\n", m_stack[i].svar1.c_str());

		molog("* coords: (%i,%i)\n", m_stack[i].coords.x, m_stack[i].coords.y);
		molog("* diranims: %p\n",  m_stack[i].diranims);
		molog("* path: %p\n",  m_stack[i].path);
      if(m_stack[i].path) {
         Path* p=m_stack[i].path;
         molog("** Path length: %i\n", p->get_nsteps());
         molog("** Start: (%i,%i)\n", p->get_start().x, p->get_start().y);
         molog("** End: (%i,%i)\n", p->get_end().x, p->get_end().y);
         for(int i=0; i<p->get_nsteps(); i++)
            molog("** Step %i/%i: %i\n", i+1, p->get_nsteps(), p->get_step(i));
      }
		molog("* transfer: %p\n",  m_stack[i].transfer);
		molog("* route: %p\n",  m_stack[i].route);

      molog("* program: %p\n",  m_stack[i].route);
   }
}

/*
==============================================================================

Bob_Descr factory

==============================================================================
*/

/*
===============
Bob_Descr::create_from_dir(const char *directory) [static]

Master factory to read a bob from the given directory and create the
appropriate description class.
===============
*/
Bob_Descr *Bob_Descr::create_from_dir(const char *name, const char *directory, Profile *prof, Tribe_Descr* tribe)
{
	Bob_Descr *bob = 0;

	try
	{
		Section *s = prof->get_safe_section("global");
		const char *type = s->get_safe_string("type");

		if (!strcasecmp(type, "critter")) {
			bob = new Critter_Bob_Descr(name, tribe);
		} else
			throw wexception("Unsupported bob type '%s'", type);

		bob->parse(directory, prof, 0);
	}
	catch(std::exception &e) {
		if (bob)
			delete bob;
		throw wexception("Error reading bob %s: %s", directory, e.what());
	}
	catch(...) {
		if (bob)
			delete bob;
		throw;
	}

	return bob;
}
