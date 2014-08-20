/*  Copyright (C) 2014 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>

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

#include <tap/basic.h>

#include "knot/zone/adjust.h"
#include "knot/zone/zonefile.h"
#include "knot/updates/apply.h"

static const char *zone_str =
"test. 3600 IN SOA a. b. 1 1 1 1 1\n"
"b.test. IN TXT \"test\"\n"
"e.test. IN TXT \"test\"\n"
"g.test. IN TXT \"test\"\n";

static const char *add1 =
"test. 3600 IN SOA a. b. 2 1 1 1 1\n"
"c.test. IN TXT \"test2\"\n";

static const char *del1 =
"g.test. IN TXT \"test\"\n";

struct adjust_params {
	zcreator_t *zc;
	changeset_t *ch;
};

/*! \brief Creates RR from parser input, passes it to handling function. */
static void scanner_process(zs_scanner_t *scanner)
{
	struct adjust_params *params = scanner->data;

	knot_rrset_t rr;
	knot_rrset_init(&rr, scanner->r_owner, scanner->r_type, scanner->r_class);
	int ret = knot_rrset_add_rdata(&rr, scanner->r_data, scanner->r_data_length,
	                               scanner->r_ttl, NULL);
	assert(ret == KNOT_EOK);
	if (rr.type == KNOT_RRTYPE_SOA) {
		knot_rrset_free(&params->ch->soa_to, NULL);
		params->ch->soa_to = knot_rrset_copy(&rr, NULL);
		assert(params->ch->soa_to);
	}
	zcreator_step(params->zc, &rr);
	knot_rdataset_clear(&rr.rrs, NULL);
}

static bool test_prev_for_tree(zone_tree_t *t)
{
	if (t == NULL) {
		return true;
	}
	
	hattrie_iter_t *itt = hattrie_iter_begin(t, true);
	assert(itt);
	
	zone_node_t *first = (zone_node_t *)(*hattrie_iter_val(itt));
	zone_node_t *prev = NULL;
	zone_node_t *curr = NULL;
	while(!hattrie_iter_finished(itt)) {
		prev = curr;
		curr = (zone_node_t *)(*hattrie_iter_val(itt));
		if (prev) {
			if (curr->prev != prev) {
				hattrie_iter_free(itt);
				return false;
			}
		}
		hattrie_iter_next(itt);
	}
	
	hattrie_iter_free(itt);
	return first->prev == curr;
}

static bool test_prev(zone_contents_t *zone)
{
	return test_prev_for_tree(zone->nodes) && test_prev_for_tree(zone->nsec3_nodes);
}

static void add_and_update(zone_contents_t *zone, changeset_t *ch,
                           zs_scanner_t *sc, const char *str)
{
	// Parse record
	int ret = zs_scanner_parse(sc, str, str + strlen(str), true);
	assert(ret == 0);
	knot_rrset_free(&ch->soa_from, NULL);
	ch->soa_from = node_create_rrset(zone->apex, KNOT_RRTYPE_SOA);
	assert(ch->soa_to && ch->soa_from);
	ret = apply_changeset_directly(zone, ch);
	assert(ret == KNOT_EOK);
}

int main(int argc, char *argv[])
{
	plan(1);
	
	// Fill zone
	knot_dname_t *owner = knot_dname_from_str("test.");
	assert(owner);
	zone_contents_t *zone = zone_contents_new(owner);
	assert(zone);
	zcreator_t zc = {.z = zone, .master = true, .ret = KNOT_EOK };
	changeset_t ch;
	changeset_init(&ch, owner);
	struct adjust_params params = {.zc = &zc, .ch = &ch };
	zs_scanner_t *sc = zs_scanner_create("test.", KNOT_CLASS_IN, 3600, scanner_process,
	                                     NULL, &params);
	assert(sc);
	int ret = zs_scanner_parse(sc, zone_str, zone_str + strlen(zone_str), true);
	assert(ret == 0);
	// Adjust data
	zone_contents_adjust_full(zone, NULL, NULL);
	assert(test_prev(zone));
	
	// Init zone update structure
	zone_update_t up;
	zone_update_init(&up, zone, &ch);
	zc.z = ch.add;
	
	// Add a record
	add_and_update(zone, &ch, sc, add1);
	
	ret = zone_adjust(&up);
	ok(ret == KNOT_EOK && test_prev(zone), "zone adjust: addition");
	
	return 0;
}
