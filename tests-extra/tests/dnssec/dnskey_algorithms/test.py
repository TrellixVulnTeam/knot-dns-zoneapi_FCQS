#!/usr/bin/env python3
"""
Validate ZSK and KSK constrains checks.
"""

import tarfile
import os.path

import dnstest.zonefile
from dnstest.test import Test

TEST_CASES = {
    "rsa":                  True,
    "rsa_ecdsa":            True,
    "rsa_now_ecdsa_future": True,
    "rsa_ecdsa_roll":       True,
    "stss_ksk":             True,
    "stss_zsk":             True,
    "stss_two_ksk":         True,
    "stss_rsa256_rsa512":   True,
    "rsa_split_ecdsa_stss": True,

    "rsa_future_all":       False,
    "rsa_future_publish":   False,
    "rsa_future_active":    False,
    "rsa_inactive_zsk":     False,
    "rsa_no_zsk":           False,
}

t = Test()

knot = t.server("knot")
knot.dnssec_enable = True

# setup keys

keys_archive = os.path.join(t.data_dir, "keys.tgz")
with tarfile.open(keys_archive, "r:*") as tar:
    def is_within_directory(directory, target):
        
        abs_directory = os.path.abspath(directory)
        abs_target = os.path.abspath(target)
    
        prefix = os.path.commonprefix([abs_directory, abs_target])
        
        return prefix == abs_directory
    
    def safe_extract(tar, path=".", members=None, *, numeric_owner=False):
    
        for member in tar.getmembers():
            member_path = os.path.join(path, member.name)
            if not is_within_directory(path, member_path):
                raise Exception("Attempted Path Traversal in Tar File")
    
        tar.extractall(path, members, numeric_owner=numeric_owner) 
        
    
    safe_extract(tar, knot.keydir)

# setup zones

zones = []
for zone_name in TEST_CASES:
    zone = dnstest.zonefile.ZoneFile(t.zones_dir)
    zone.set_name(zone_name)
    zone.gen_file(dnssec=False, nsec3=False, records=5)
    zones.append(zone)

t.link(zones, knot)

t.start()

for zone in [zone for zone in zones if TEST_CASES[zone.name.rstrip(".")]]:
    knot.zone_wait(zone)

for zone, valid in TEST_CASES.items():
    expected_rcode = "NOERROR" if valid else "SERVFAIL"
    knot.dig(zone, "SOA").check(rcode=expected_rcode)

t.end()
