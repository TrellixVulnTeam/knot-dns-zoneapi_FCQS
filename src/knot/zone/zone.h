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
/*!
 * \file zone.h
 *
 * \brief Zone structure and API for manipulating it.
 *
 * \addtogroup libknot
 * @{
 */

#pragma once

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "common/evsched.h"
#include "common/ref.h"
#include "knot/conf/conf.h"
#include "knot/server/journal.h"
#include "knot/updates/acl.h"
#include "knot/zone/events.h"
#include "knot/zone/contents.h"
#include "libknot/dname.h"

struct process_query_param;

/*!
 * \brief Zone flags.
 */
typedef enum zone_flag_t {
	ZONE_FORCE_AXFR   = 1 << 0, /* Force AXFR, zone master may not be IXFR-capable. */
} zone_flag_t;

/*!
 * \brief Structure for holding DNS zone.
 */
typedef struct zone_t {

	//! \todo Move ACLs into configuration.
	//! \todo Remove refcounting + flags.

	ref_t ref;     /*!< Reference counting. */
	knot_dname_t *name;

	zone_contents_t *contents;
	time_t zonefile_mtime;
	uint32_t zonefile_serial;

	zone_flag_t flags;

	/*! \brief Shortcut to zone config entry. */
	conf_zone_t *conf;

	/*! \brief DDNS queue and lock. */
	pthread_mutex_t ddns_lock;
	list_t ddns_queue;

	/*! \brief Access control lists. */
	acl_t *xfr_out;    /*!< ACL for outgoing transfers.*/
	acl_t *notify_in;  /*!< ACL for incoming notifications.*/
	acl_t *update_in;  /*!< ACL for incoming updates.*/

	/*! \brief Zone events. */
	zone_events_t events;

	/*! \brief XFR-IN scheduler. */
	struct {
		uint32_t bootstrap_retry; /*!< AXFR/IN bootstrap retry. */
		unsigned state;
	} xfr_in;

	struct {
		uint32_t refresh_at; /*!< Next DNSSEC resign event. */
		bool next_force;     /*!< Drop existing signatures. */
	} dnssec;

} zone_t;

/*----------------------------------------------------------------------------*/

/*!
 * \brief Creates new zone with emtpy zone content.
 *
 * \param conf  Zone configuration.
 *
 * \return The initialized zone structure or NULL if an error occured.
 */
zone_t *zone_new(conf_zone_t *conf);

/*!
 * \brief Deallocates the zone structure.
 *
 * \note The function also deallocates all bound structures (config, contents, etc.).
 *
 * \param zone Zone to be freed.
 */
void zone_free(zone_t **zone_ptr);

/*! \note Zone change API, subject to change. */
changeset_t *zone_change_prepare(changesets_t *chset);
int zone_change_commit(zone_contents_t *contents, changesets_t *chset);
int zone_change_store(zone_t *zone, changesets_t *chset);
/*! \note @mvavrusa Moved from zones.c, this needs a common API. */
int zone_change_apply_and_store(changesets_t *chs,
                                zone_t *zone,
                                const char *msgpref,
                                mm_ctx_t *rr_mm);
/*!
 * \brief Atomically switch the content of the zone.
 */
zone_contents_t *zone_switch_contents(zone_t *zone,
					   zone_contents_t *new_contents);

/*! \brief Return zone master remote. */
const conf_iface_t *zone_master(const zone_t *zone);

/*! \brief Rotate list of master remotes for current zone. */
void zone_master_rotate(const zone_t *zone);

/*! \brief Synchronize zone file with journal. */
int zone_flush_journal(zone_t *zone);

/*! \brief Enqueue UPDATE request for processing. */
int zone_update_enqueue(zone_t *zone, knot_pkt_t *pkt, struct process_query_param *param);

/*! \brief Dequeue UPDATE request. */
struct request_data *zone_update_dequeue(zone_t *zone);

/*! @} */
