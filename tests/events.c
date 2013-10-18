/*  Copyright (C) 2011 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <tests/tap/basic.h>

#include "common/evqueue.h"
#include "common/evsched.h"

void* term_thr(void *arg)
{
	evsched_t *s = (evsched_t *)arg;

	/* Sleep for 100ms. */
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100 * 1000; // 100ms
	select(0, 0, 0, 0, &tv);

	/* Issue termination event. */
	evsched_schedule_term(s, 0);
	return 0;
}

int main(int argc, char *argv[])
{
	plan(20);

	/*
	 * Event queue tests.
	 */

	// 1. Construct an event queue
	evqueue_t *q = evqueue_new();
	ok(q != 0, "evqueue: new");

	// 2. Send integer through event queue
	int ret = 0;
	uint8_t sent = 0xaf, rcvd = 0;
	ret = evqueue_write(q, &sent, sizeof(uint8_t));
	is_int(sizeof(uint8_t), ret, "evqueue: send byte through");

	// 3. Receive byte from event queue
	ret = evqueue_read(q, &rcvd, sizeof(uint8_t));
	is_int(sizeof(uint8_t), ret, "evqueue: received byte");

	// 4. Received match
	ok(sent == rcvd, "evqueue: received byte match");

	// 5. Sending event
	event_t ev, rev;
	memset(&ev, 0, sizeof(event_t));
	memset(&rev, 0, sizeof(event_t));
	ev.type = 0xfa11;
	ev.data = (void*)0xceed;
	ret = evqueue_add(q, &ev);
	is_int(0, ret, "evqueue: sent event to queue");

	// 6. Poll for new events
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 100 * 1000 * 1000; // 100ms
	ret = evqueue_poll(q, &ts, 0);
	ok(ret > 0, "evqueue: polling queue for events");

	// 7. Compare received event
	ret = evqueue_get(q, &rev);
	/* Compare useful data, as event owner was changed in evqueue_get(). */
	if (ev.type == rev.type && ev.data == rev.data) {
		ret = 0;
	}
	is_int(0, ret, "evqueue: received event matches sent");

	// 8. Invalid parameters
	evqueue_free(0);
	evqueue_poll(0,0,0);
	evqueue_read(0, 0, 0);
	evqueue_write(0, 0, 0);
	evqueue_read(0, 0, 0);
	evqueue_get(0, 0);
	evqueue_add(0, 0);
	ok(1, "evqueue: won't crash with NULL parameters");

	// 9. Free event queue
	evqueue_free(&q);
	ok(1, "evqueue: delete");

	/*
	 * Event scheduler tests.
	 */

	// 1. Construct event scheduler
	event_t *e = 0;
	evsched_t *s = evsched_new();
	ok(s != 0, "evsched: new");

	// 2. Schedule event to happen after N ms
	int msecs = 200;
	struct timeval st, rt;
	gettimeofday(&st, 0);
	e = evsched_schedule_cb(s, 0, (void*)0xcafe, msecs);
	ok(e != 0, "evsched: scheduled empty event after %dms", msecs);

	// 3. Wait for next event
	e = evsched_next(s);
	evsched_event_finished(s);
	gettimeofday(&rt, 0);
	ok(e != 0, "evsched: received valid event");

#ifdef ENABLE_TIMED_TESTS
	// 4. Check receive time
	double passed = (rt.tv_sec - st.tv_sec) * 1000;
	passed += (rt.tv_usec - st.tv_usec) / 1000;
	double margin = msecs * 0.4;
	double lb = msecs - margin, ub = msecs + margin;
	int in_bounds = (passed >= lb) && (passed <= ub);
	ok(in_bounds, "evsched: receive time %.1lfms is in <%.1lf,%.1lf>",
	   passed, lb, ub);
#else
	skip("Timed tests not enabled");
#endif

	// 5. Check data
	ok(e->data == (void*)0xcafe, "evsched: received data is valid");

	// 6. Delete event
	evsched_event_free(s, e);
	ok(1, "evsched: deleted event");

	// 7. Insert and immediately cancel an event
	e = evsched_schedule_cb(s, 0, (void*)0xdead, 1000);
	ret = evsched_cancel(s, e);
	ok(ret >= 0, "evsched: inserted and cancelled an event");
	if (e) {
		evsched_event_free(s, e);
	}

	// 8. Start listener thread and block
	pthread_t t;
	pthread_create(&t, 0, term_thr, s);
	e = evsched_next(s);
	evsched_event_finished(s);
	ok(e != 0, "evsched: received termination event");

	// 9. Termination event is valid
	ok(e->type == EVSCHED_TERM, "evsched: termination event is valid");
	evsched_event_free(s, e);
	pthread_join(t, 0);

	// 10. Invalid parameters
	evsched_delete(0);
	evsched_event_new(0, 0);
	evsched_event_free(0, 0);
	evsched_next(0);
	evsched_schedule(0, 0, 0);
	evsched_schedule_cb(0, 0, 0, 0);
	evsched_schedule_term(0, 0);
	evsched_cancel(0, 0);
	ok(1, "evsched: won't crash with NULL parameters");

	// 11. Delete event scheduler
	evsched_delete(&s);
	ok(1, "evsched: delete");

	return 0;
}