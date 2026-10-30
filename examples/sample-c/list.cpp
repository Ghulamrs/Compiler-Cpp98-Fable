/* list.cpp - linked list, qsort comparators and binary search of the multi-file C sample */
#include "cprog.h"
#include <stdlib.h>
#include <string.h>

Node* list_push(Node* head, int value)
{
    Node* n = (Node*)malloc(sizeof(Node));
    if (!n) return head;
    n->value = value;
    n->next = head;
    return n;
}

Node* list_reverse(Node* head)
{
    Node* prev = NULL;
    while (head) {
        Node* next = head->next;
        head->next = prev;
        prev = head;
        head = next;
    }
    return prev;
}

int list_sum(const Node* head)
{
    return head ? head->value + list_sum(head->next) : 0;
}

void list_free(Node* head)
{
    while (head) {
        Node* next = head->next;
        free(head);
        head = next;
    }
}

int compare_ints(const void* a, const void* b)
{
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);
}

int compare_strings(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

int binary_search(const int* v, int n, int key)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (v[mid] == key) return mid;
        if (v[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}
