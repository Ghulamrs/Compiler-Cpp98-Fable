/* table.cpp - a word-count hash table with chaining of the multi-file C sample */
#include "cprog.h"
#include <stdlib.h>
#include <string.h>

/* strutil.cpp has a static function of the same name: here it names the characters the hash ignores */
static int is_separator(int c)
{
    return c == '_';
}

static unsigned hash_key(const char* key)
{
    unsigned h = 2166136261u;   /* FNV-1a */
    for (; *key; ++key) {
        if (is_separator((unsigned char)*key)) continue;
        h = (h ^ (unsigned char)*key) * 16777619u;
    }
    return h;
}

void table_init(Table* t)
{
    memset(t, 0, sizeof(*t));
}

void table_add(Table* t, const char* key)
{
    unsigned slot = hash_key(key) % CPROG_TABLE_SIZE;
    Bucket* b;
    for (b = t->slots[slot]; b; b = b->next)
        if (strcmp(b->entry.key, key) == 0) {
            ++b->entry.count;
            return;
        }
    b = (Bucket*)malloc(sizeof(Bucket));
    if (!b) return;
    strncpy(b->entry.key, key, sizeof(b->entry.key) - 1);
    b->entry.key[sizeof(b->entry.key) - 1] = '\0';
    b->entry.count = 1;
    b->next = t->slots[slot];
    t->slots[slot] = b;
    ++t->size;
}

int table_get(const Table* t, const char* key)
{
    const Bucket* b;
    for (b = t->slots[hash_key(key) % CPROG_TABLE_SIZE]; b; b = b->next)
        if (strcmp(b->entry.key, key) == 0) return b->entry.count;
    return 0;
}

void table_free(Table* t)
{
    int i;
    for (i = 0; i < CPROG_TABLE_SIZE; ++i) {
        Bucket* b = t->slots[i];
        while (b) {
            Bucket* next = b->next;
            free(b);
            b = next;
        }
        t->slots[i] = NULL;
    }
    t->size = 0;
}
