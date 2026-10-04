/* Sorting & Searching - victim / resource management (RESQ) */
#include <string.h>
#include "resq_c.h"

/* Insertion sort: triage order (most critical first, then youngest). */
void sort_by_severity_desc(VictimRecord *a, int n) {
    int i, j;
    for (i = 1; i < n; i++) {
        VictimRecord key = a[i];
        j = i - 1;
        while (j >= 0 && (a[j].severity < key.severity ||
                         (a[j].severity == key.severity && a[j].age > key.age))) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

/* Quick sort by id - prepares the array for binary search. */
static void qs(VictimRecord *a, int lo, int hi) {
    int i = lo, j = hi, pivot = a[(lo + hi) / 2].id;
    while (i <= j) {
        while (a[i].id < pivot) i++;
        while (a[j].id > pivot) j--;
        if (i <= j) { VictimRecord t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
    }
    if (lo < j) qs(a, lo, j);
    if (i < hi) qs(a, i, hi);
}

void sort_by_id_asc(VictimRecord *a, int n) { if (n > 1) qs(a, 0, n - 1); }

int binary_search_by_id(const VictimRecord *a, int n, int id) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid].id == id) return mid;
        if (a[mid].id <  id) lo = mid + 1;
        else                 hi = mid - 1;
    }
    return -1;
}

int linear_search_by_name(const VictimRecord *a, int n, const char *name) {
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(a[i].name, name) == 0) return i;
    return -1;
}
