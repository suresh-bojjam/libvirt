#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "internal.h"
#include "hash.h"
#include "hashdata.h"
#include "testutils.h"


#define testError(...)                                          \
    do {                                                        \
        fprintf(stderr, __VA_ARGS__);                           \
        /* Pad to line up with test name ... in virTestRun */   \
        fprintf(stderr, "%74s", "... ");                        \
    } while (0)


static virHashTablePtr
testHashInit(int size)
{
    virHashTablePtr hash;
    int i;

    if (!(hash = virHashCreate(size, NULL)))
        return NULL;

    /* entires are added in reverse order so that they will be linked in
     * collision list in the same order as in the uuids array
     */
    for (i = ARRAY_CARDINALITY(uuids) - 1; i >= 0; i--) {
        int oldsize = virHashTableSize(hash);
        if (virHashAddEntry(hash, uuids[i], (void *) uuids[i]) < 0) {
            virHashFree(hash);
            return NULL;
        }

        if (virHashTableSize(hash) != oldsize && virTestGetDebug()) {
            fprintf(stderr, "\nhash grown from %d to %d",
                    oldsize, virHashTableSize(hash));
        }
    }

    for (i = 0; i < ARRAY_CARDINALITY(uuids); i++) {
        if (!virHashLookup(hash, uuids[i])) {
            if (virTestGetVerbose()) {
                fprintf(stderr, "\nentry \"%s\" could not be found\n",
                        uuids[i]);
            }
            virHashFree(hash);
            return NULL;
        }
    }

    if (size && size != virHashTableSize(hash) && virTestGetDebug())
        fprintf(stderr, "\n");

    return hash;
}


static int
testHashCheckCount(virHashTablePtr hash, int count)
{
    if (virHashSize(hash) != count) {
        testError("\nhash contains %d instead of %d elements\n",
                  virHashSize(hash), count);
        return -1;
    }

    return 0;
}


struct testInfo {
    void *data;
    int count;
};


static int
testHashGrow(const void *data)
{
    const struct testInfo *info = data;
    virHashTablePtr hash;
    int ret = -1;

    if (!(hash = testHashInit(info->count)))
        return -1;

    if (testHashCheckCount(hash, ARRAY_CARDINALITY(uuids)) < 0)
        goto cleanup;

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


static int
testHashUpdate(const void *data ATTRIBUTE_UNUSED)
{
    int count = ARRAY_CARDINALITY(uuids) + ARRAY_CARDINALITY(uuids_new);
    virHashTablePtr hash;
    int i;
    int ret = -1;

    if (!(hash = testHashInit(0)))
        return -1;

    for (i = 0; i < ARRAY_CARDINALITY(uuids_subset); i++) {
        if (virHashUpdateEntry(hash, uuids_subset[i], (void *) 1) < 0) {
            if (virTestGetVerbose()) {
                fprintf(stderr, "\nentry \"%s\" could not be updated\n",
                        uuids_subset[i]);
            }
            goto cleanup;
        }
    }

    for (i = 0; i < ARRAY_CARDINALITY(uuids_new); i++) {
        if (virHashUpdateEntry(hash, uuids_new[i], (void *) 1) < 0) {
            if (virTestGetVerbose()) {
                fprintf(stderr, "\nnew entry \"%s\" could not be updated\n",
                        uuids_new[i]);
            }
            goto cleanup;
        }
    }

    if (testHashCheckCount(hash, count) < 0)
        goto cleanup;

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


static int
testHashRemove(const void *data ATTRIBUTE_UNUSED)
{
    int count = ARRAY_CARDINALITY(uuids) - ARRAY_CARDINALITY(uuids_subset);
    virHashTablePtr hash;
    int i;
    int ret = -1;

    if (!(hash = testHashInit(0)))
        return -1;

    for (i = 0; i < ARRAY_CARDINALITY(uuids_subset); i++) {
        if (virHashRemoveEntry(hash, uuids_subset[i]) < 0) {
            if (virTestGetVerbose()) {
                fprintf(stderr, "\nentry \"%s\" could not be removed\n",
                        uuids_subset[i]);
            }
            goto cleanup;
        }
    }

    if (testHashCheckCount(hash, count) < 0)
        goto cleanup;

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


const int testHashCountRemoveForEachSome =
    ARRAY_CARDINALITY(uuids) - ARRAY_CARDINALITY(uuids_subset);

static void
testHashRemoveForEachSome(void *payload ATTRIBUTE_UNUSED,
                          const void *name,
                          void *data)
{
    virHashTablePtr hash = data;
    int i;

    for (i = 0; i < ARRAY_CARDINALITY(uuids_subset); i++) {
        if (STREQ(uuids_subset[i], name)) {
            if (virHashRemoveEntry(hash, name) < 0 && virTestGetVerbose()) {
                fprintf(stderr, "\nentry \"%s\" could not be removed",
                        uuids_subset[i]);
            }
            break;
        }
    }
}


const int testHashCountRemoveForEachAll = 0;

static void
testHashRemoveForEachAll(void *payload ATTRIBUTE_UNUSED,
                         const void *name,
                         void *data)
{
    virHashTablePtr hash = data;

    virHashRemoveEntry(hash, name);
}


const int testHashCountRemoveForEachForbidden = ARRAY_CARDINALITY(uuids);

static void
testHashRemoveForEachForbidden(void *payload ATTRIBUTE_UNUSED,
                               const void *name,
                               void *data)
{
    virHashTablePtr hash = data;
    int i;

    for (i = 0; i < ARRAY_CARDINALITY(uuids_subset); i++) {
        if (STREQ(uuids_subset[i], name)) {
            int next = (i + 1) % ARRAY_CARDINALITY(uuids_subset);

            if (virHashRemoveEntry(hash, uuids_subset[next]) == 0 &&
                virTestGetVerbose()) {
                fprintf(stderr,
                        "\nentry \"%s\" should not be allowed to be removed",
                        uuids_subset[next]);
            }
            break;
        }
    }
}


static int
testHashRemoveForEach(const void *data)
{
    const struct testInfo *info = data;
    virHashTablePtr hash;
    int count;
    int ret = -1;

    if (!(hash = testHashInit(0)))
        return -1;

    count = virHashForEach(hash, (virHashIterator) info->data, hash);

    if (count != ARRAY_CARDINALITY(uuids)) {
        if (virTestGetVerbose()) {
            testError("\nvirHashForEach didn't go through all entries,"
                      " %d != %zu\n",
                      count, ARRAY_CARDINALITY(uuids));
        }
        goto cleanup;
    }

    if (testHashCheckCount(hash, info->count) < 0)
        goto cleanup;

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


static int
testHashSteal(const void *data ATTRIBUTE_UNUSED)
{
    int count = ARRAY_CARDINALITY(uuids) - ARRAY_CARDINALITY(uuids_subset);
    virHashTablePtr hash;
    int i;
    int ret = -1;

    if (!(hash = testHashInit(0)))
        return -1;

    for (i = 0; i < ARRAY_CARDINALITY(uuids_subset); i++) {
        if (!virHashSteal(hash, uuids_subset[i])) {
            if (virTestGetVerbose()) {
                fprintf(stderr, "\nentry \"%s\" could not be stolen\n",
                        uuids_subset[i]);
            }
            goto cleanup;
        }
    }

    if (testHashCheckCount(hash, count) < 0)
        goto cleanup;

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


static void
testHashIter(void *payload ATTRIBUTE_UNUSED,
             const void *name ATTRIBUTE_UNUSED,
             void *data ATTRIBUTE_UNUSED)
{
    return;
}

static void
testHashForEachIter(void *payload ATTRIBUTE_UNUSED,
                    const void *name ATTRIBUTE_UNUSED,
                    void *data)
{
    virHashTablePtr hash = data;

    if (virHashAddEntry(hash, uuids_new[0], NULL) == 0 &&
        virTestGetVerbose()) {
        fprintf(stderr, "\nadding entries in ForEach should be forbidden");
    }

    if (virHashUpdateEntry(hash, uuids_new[0], NULL) == 0 &&
        virTestGetVerbose()) {
        fprintf(stderr, "\nupdating entries in ForEach should be forbidden");
    }

    if (virHashSteal(hash, uuids_new[0]) != NULL &&
        virTestGetVerbose()) {
        fprintf(stderr, "\nstealing entries in ForEach should be forbidden");
    }

    if (virHashSteal(hash, uuids_new[0]) != NULL &&
        virTestGetVerbose()) {
        fprintf(stderr, "\nstealing entries in ForEach should be forbidden");
    }

    if (virHashForEach(hash, testHashIter, NULL) >= 0 &&
        virTestGetVerbose()) {
        fprintf(stderr, "\niterating through hash in ForEach"
                " should be forbidden");
    }
}

static int
testHashForEach(const void *data ATTRIBUTE_UNUSED)
{
    virHashTablePtr hash;
    int count;
    int ret = -1;

    if (!(hash = testHashInit(0)))
        return -1;

    count = virHashForEach(hash, testHashForEachIter, hash);

    if (count != ARRAY_CARDINALITY(uuids)) {
        if (virTestGetVerbose()) {
            testError("\nvirHashForEach didn't go through all entries,"
                      " %d != %lu\n",
                      count, ARRAY_CARDINALITY(uuids));
        }
        goto cleanup;
    }

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


static int
testHashRemoveSetIter(const void *payload ATTRIBUTE_UNUSED,
                      const void *name,
                      const void *data)
{
    int *count = (int *) data;
    bool rem = false;
    int i;

    for (i = 0; i < ARRAY_CARDINALITY(uuids_subset); i++) {
        if (STREQ(uuids_subset[i], name)) {
            rem = true;
            break;
        }
    }

    if (rem || rand() % 2) {
        (*count)++;
        return 1;
    } else {
        return 0;
    }
}

static int
testHashRemoveSet(const void *data ATTRIBUTE_UNUSED)
{
    virHashTablePtr hash;
    int count = 0;
    int rcount;
    int ret = -1;

    if (!(hash = testHashInit(0)))
        return -1;

    /* seed the generator so that rand() provides reproducible sequence */
    srand(9000);

    rcount = virHashRemoveSet(hash, testHashRemoveSetIter, &count);

    if (count != rcount) {
        if (virTestGetVerbose()) {
            testError("\nvirHashRemoveSet didn't remove expected number of"
                      " entries, %d != %u\n",
                      rcount, count);
        }
        goto cleanup;
    }

    if (testHashCheckCount(hash, ARRAY_CARDINALITY(uuids) - count) < 0)
        goto cleanup;

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


const int testSearchIndex = ARRAY_CARDINALITY(uuids_subset) / 2;

static int
testHashSearchIter(const void *payload ATTRIBUTE_UNUSED,
                   const void *name,
                   const void *data ATTRIBUTE_UNUSED)
{
    return STREQ(uuids_subset[testSearchIndex], name);
}

static int
testHashSearch(const void *data ATTRIBUTE_UNUSED)
{
    virHashTablePtr hash;
    void *entry;
    int ret = -1;

    if (!(hash = testHashInit(0)))
        return -1;

    entry = virHashSearch(hash, testHashSearchIter, NULL);

    if (!entry || STRNEQ(uuids_subset[testSearchIndex], entry)) {
        if (virTestGetVerbose()) {
            testError("\nvirHashSearch didn't find entry '%s'\n",
                      uuids_subset[testSearchIndex]);
        }
        goto cleanup;
    }

    if (testHashCheckCount(hash, ARRAY_CARDINALITY(uuids)) < 0)
        goto cleanup;

    ret = 0;

cleanup:
    virHashFree(hash);
    return ret;
}


static int
mymain(int argc ATTRIBUTE_UNUSED,
       char **argv ATTRIBUTE_UNUSED)
{
    int ret = 0;

#define DO_TEST_FULL(name, cmd, data, count)                        \
    do {                                                            \
        struct testInfo info = { data, count };                     \
        if (virtTestRun(name, 1, testHash ## cmd, &info) < 0)       \
            ret = -1;                                               \
    } while (0)

#define DO_TEST_DATA(name, cmd, data)                               \
    DO_TEST_FULL(name "(" #data ")",                                \
                 cmd,                                               \
                 testHash ## cmd ## data,                           \
                 testHashCount ## cmd ## data)

#define DO_TEST_COUNT(name, cmd, count)                             \
    DO_TEST_FULL(name "(" #count ")", cmd, NULL, count)

#define DO_TEST(name, cmd)                                          \
    DO_TEST_FULL(name, cmd, NULL, -1)

    DO_TEST_COUNT("Grow", Grow, 1);
    DO_TEST_COUNT("Grow", Grow, 10);
    DO_TEST_COUNT("Grow", Grow, 42);
    DO_TEST("Update", Update);
    DO_TEST("Remove", Remove);
    DO_TEST_DATA("Remove in ForEach", RemoveForEach, Some);
    DO_TEST_DATA("Remove in ForEach", RemoveForEach, All);
    DO_TEST_DATA("Remove in ForEach", RemoveForEach, Forbidden);
    DO_TEST("Steal", Steal);
    DO_TEST("Forbidden ops in ForEach", ForEach);
    DO_TEST("RemoveSet", RemoveSet);
    DO_TEST("Search", Search);

    return (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

VIRT_TEST_MAIN(mymain)
