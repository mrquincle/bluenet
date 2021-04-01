#include <third/cuckoo/CuckooFilter.h>
#include <util/cs_RandomGenerator.h>

#include <util/cs_Crc16.h>

#include <cstring>

/* ------------------------------------------------------------------------- */
/* ---------------------------- Hashing methods ---------------------------- */
/* ------------------------------------------------------------------------- */

cuckoo_fingerprint_t CuckooFilter::hash(cuckoo_key_t key, size_t keyLengthInBytes) {
	return static_cast<cuckoo_fingerprint_t>(
			crc16(static_cast<const uint8_t*>(key), keyLengthInBytes, nullptr));
}

/* ------------------------------------------------------------------------- */

cuckoo_extended_fingerprint_t CuckooFilter::getExtendedFingerprint(
		cuckoo_fingerprint_t finger, cuckoo_index_t bucketIndex) {

	return cuckoo_extended_fingerprint_t{
			.fingerprint = finger,
			.bucketA     = static_cast<cuckoo_index_t>(bucketIndex),
			.bucketB     = static_cast<cuckoo_index_t>((bucketIndex ^ finger) % bucketCount())};
}

cuckoo_extended_fingerprint_t CuckooFilter::getExtendedFingerprint(
		cuckoo_key_t key, size_t keyLengthInBytes) {

	cuckoo_fingerprint_t finger        = hash(key, keyLengthInBytes);
	cuckoo_fingerprint_t hashedFinger = hash(&finger, sizeof(finger));

	return cuckoo_extended_fingerprint_t{
			.fingerprint = finger,
			.bucketA     = static_cast<cuckoo_index_t>(hashedFinger % bucketCount()),
			.bucketB     = static_cast<cuckoo_index_t>((hashedFinger ^ finger) % bucketCount())};
}

/* ------------------------------------------------------------------------- */

cuckoo_fingerprint_t CuckooFilter::filterhash() {
	return hash(&data, size());
}

/* ------------------------------------------------------------------------- */
/* ---------------------------- Filter methods ----------------------------- */
/* ------------------------------------------------------------------------- */

bool CuckooFilter::addFingerprintToBucket(
		cuckoo_fingerprint_t fingerprint, cuckoo_index_t bucketIndex) {
	for (size_t ii = 0; ii < data.nests_per_bucket; ++ii) {
		cuckoo_fingerprint_t& fingerprintInArray = lookupFingerprint(bucketIndex, ii);
		if (0 == fingerprintInArray) {
			fingerprintInArray = fingerprint;
			return true;
		}
	}

	return false;
}

/* ------------------------------------------------------------------------- */

bool CuckooFilter::removeFingerprintFromBucket(
		cuckoo_fingerprint_t fingerprint, cuckoo_index_t bucketIndex) {
	for (cuckoo_index_t ii = 0; ii < data.nests_per_bucket; ++ii) {
		cuckoo_fingerprint_t& candidateFingerprintForRemovalInArray =
				lookupFingerprint(bucketIndex, ii);

		if (candidateFingerprintForRemovalInArray == fingerprint) {
			candidateFingerprintForRemovalInArray = 0;

			// to keep the bucket front loaded, move the last non-zero
			// fingerprint behind ii into the slot.
			for (cuckoo_index_t jj = data.nests_per_bucket - 1; jj > ii; --jj) {
				cuckoo_fingerprint_t& lastFingerprintOfBucket =
						lookupFingerprint(bucketIndex, jj);

				if (lastFingerprintOfBucket != 0) {
					candidateFingerprintForRemovalInArray = lastFingerprintOfBucket;
				}
			}

			return true;
		}
	}

	return false;
}

/* ------------------------------------------------------------------------- */

bool CuckooFilter::move(cuckoo_extended_fingerprint_t entryToInsert) {
	// seeding with a hash for this filter guarantees exact same sequence of
	// random integers used for moving fingerprints in the filter on every crownstone.
	uint16_t seed = filterhash();
	RandomGenerator rand(seed);

	for (size_t attemptsLeft = MAX_KICK_ATTEMPTS; attemptsLeft > 0; --attemptsLeft) {
		// try to add to bucket A
		if (addFingerprintToBucket(entryToInsert.fingerprint, entryToInsert.bucketA)) {
			return true;
		}

		// try to add to bucket B
		if (addFingerprintToBucket(entryToInsert.fingerprint, entryToInsert.bucketB)) {
			return true;
		}

		// no success, time to kick a fingerprint from one of our buckets

		// determine which bucket to kick from
		cuckoo_index_t kickedItemBucket =
				(rand() % 2) ? entryToInsert.bucketA : entryToInsert.bucketB;

		// and which fingerprint index
		cuckoo_index_t kickedItemIndex = rand() % data.nests_per_bucket;

		// swap entry to insert and the randomly chosen (kicked) item
		cuckoo_fingerprint_t& kicked_item_fingerprint_ref =
				lookupFingerprint(kickedItemBucket, kickedItemIndex);
		cuckoo_fingerprint_t kickedItemFingerprintValue = kicked_item_fingerprint_ref;
		kicked_item_fingerprint_ref                        = entryToInsert.fingerprint;
		entryToInsert = getExtendedFingerprint(kickedItemFingerprintValue, kickedItemBucket);

		// next iteration will try to re-insert the footprint previously at (h,i).
	}

	// failed to re-place the last entry into the buffer after max attempts.
	data.victim = entryToInsert;

	return false;
}

/* ------------------------------------------------------------------------- */

void CuckooFilter::init(cuckoo_index_t bucketCount, cuckoo_index_t nestsPerBucket) {
	// find ceil(log2(bucketCount)):
	data.bucket_count_log2 = 0;
	if (bucketCount > 0) {
		bucketCount--;
		while (bucketCount > 0) {
			bucketCount >>= 1;
			data.bucket_count_log2 += 1;
		}
	}

	data.nests_per_bucket = nestsPerBucket;
	clear();
}

/* ------------------------------------------------------------------------- */

bool CuckooFilter::contains(cuckoo_extended_fingerprint_t efp) {
	// loops are split to improve cache hit rate.

	// search bucketA
	for (size_t ii = 0; ii < data.nests_per_bucket; ++ii) {
		if (efp.fingerprint == lookupFingerprint(efp.bucketA, ii)) {
			return true;
		}
	}

	// search bucketB
	for (size_t ii = 0; ii < data.nests_per_bucket; ++ii) {
		if (efp.fingerprint == lookupFingerprint(efp.bucketB, ii)) {
			return true;
		}
	}

	return false;
}

/* ------------------------------------------------------------------------- */

bool CuckooFilter::add(cuckoo_extended_fingerprint_t efp) {
	if (contains(efp)) {
		return true;
	}

	if (data.victim.fingerprint != 0) {
		return false;
	}

	return move(efp);
}

/* ------------------------------------------------------------------------- */

void CuckooFilter::clear() {
	std::memset(data.bucket_array, 0x00, bufferSize());
	data.victim = cuckoo_extended_fingerprint_t{};
}

bool CuckooFilter::remove(cuckoo_extended_fingerprint_t efp) {
	if (removeFingerprintFromBucket(efp.fingerprint, efp.bucketA)
		|| removeFingerprintFromBucket(efp.fingerprint, efp.bucketB)) {
		// short ciruits nicely:
		//    tries bucketA,
		//    on fail try B,
		//    if either succes, fix data.victim.

		if (data.victim.fingerprint != 0) {
			if (add(data.victim)) {
				data.victim = cuckoo_extended_fingerprint_t{};
			}
		}

		return true;
	}

	return false;
}
