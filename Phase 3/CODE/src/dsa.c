/*
 * dsa.c  --  RESQ Phase III: Data Structures and Algorithms
 *
 * Team member : Akshat Dobhal (ML-2)
 * Responsibility: Linked list, FIFO queues (urgent + routine), critical
 *                 max-heap, operation-history stack, sorting algorithms
 *                 (insertion sort, quick sort) and search algorithms
 *                 (linear search, binary search).
 *
 * Data structures implemented:
 *   - ResqLinkedList : singly-linked list with head + tail pointers (O(1) append)
 *   - ResqPriorityQueue : binary max-heap keyed on (priority_score DESC, seq ASC)
 *   - ResqQueue (urgent / routine) : circular FIFO for YELLOW and GREEN victims
 *   - ResqStack : LIFO history of resolved operations
 *
 * Algorithms:
 *   - resq_sort_insertion : stable O(N^2) insertion sort on victim arrays
 *   - resq_sort_quick     : O(N log N) average quick sort on victim arrays
 *   - resq_search_linear  : O(N) linear search by victim ID
 *   - resq_search_binary  : O(log N) binary search by victim ID on sorted array
 *
 * Both sorting algorithms order records by priority score (descending) and
 * break ties using victim ID (ascending, preserving arrival order within
 * equal-score groups).
 */

#include "resq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/*  Internal helpers                                                    */
/* ================================================================== */

/*
 * free_list_nodes -- walk the linked list and free every heap-allocated node.
 * Called by resq_store_clear() so nodes are not leaked when the store is reset.
 */
static void free_list_nodes(ResqLinkedList *list)
{
    ResqVictimNode *current;
    ResqVictimNode *next_node;

    if (!list) {
        return;
    }

    current = list->head;
    while (current != NULL) {
        next_node = current->next;
        free(current);
        current = next_node;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

/*
 * find_node -- linear search through the linked list for a given victim ID.
 * Returns a pointer to the node, or NULL when not found.
 */
static ResqVictimNode *find_node(ResqLinkedList *list, int victim_id)
{
    ResqVictimNode *node;

    if (!list) {
        return NULL;
    }

    for (node = list->head; node != NULL; node = node->next) {
        if (node->value.id == victim_id) {
            return node;
        }
    }

    return NULL;
}

/*
 * heap_compare -- returns a positive number when victim 'a' should be served
 * before victim 'b', zero when they are equivalent in priority, and a negative
 * number when 'b' should come first.
 *
 * Primary key   : priority_score DESCENDING (higher score = higher urgency)
 * Secondary key : seq            ASCENDING  (lower seq = arrived earlier)
 *
 * The secondary key implements the report requirement that "earlier calls come
 * first when scores match", fixing the FIFO tie-breaking defect.
 */
static int heap_compare(const ResqVictim *a, const ResqVictim *b)
{
    int score_a = resq_priority_score(a);
    int score_b = resq_priority_score(b);

    if (score_a != score_b) {
        return score_a - score_b;   /* positive => a wins */
    }
    /* Tie: earlier arrival (lower seq) wins */
    return b->seq - a->seq;         /* positive when b arrived later => a wins */
}

/*
 * heap_sift_up -- restore the max-heap invariant after inserting at index i.
 */
static void heap_sift_up(ResqPriorityQueue *heap, int i)
{
    ResqVictim temp;
    int parent;

    while (i > 0) {
        parent = (i - 1) / 2;
        if (heap_compare(&heap->items[parent], &heap->items[i]) >= 0) {
            break;  /* parent already has equal or higher priority */
        }
        /* Swap parent and child */
        temp               = heap->items[parent];
        heap->items[parent] = heap->items[i];
        heap->items[i]      = temp;
        i = parent;
    }
}

/*
 * heap_sift_down -- restore the max-heap invariant after removing the root.
 */
static void heap_sift_down(ResqPriorityQueue *heap, int i)
{
    ResqVictim temp;
    int left, right, best;

    while (1) {
        left  = i * 2 + 1;
        right = i * 2 + 2;
        best  = i;

        if (left < heap->size &&
            heap_compare(&heap->items[left], &heap->items[best]) > 0) {
            best = left;
        }
        if (right < heap->size &&
            heap_compare(&heap->items[right], &heap->items[best]) > 0) {
            best = right;
        }
        if (best == i) {
            break;  /* heap invariant satisfied */
        }

        temp            = heap->items[i];
        heap->items[i]  = heap->items[best];
        heap->items[best] = temp;
        i = best;
    }
}

/*
 * heap_push -- insert a victim into the max-heap.
 * Returns 1 on success, 0 when the heap is full or arguments are invalid.
 */
static int heap_push(ResqPriorityQueue *heap, const ResqVictim *victim)
{
    if (!heap || !victim || heap->size >= RESQ_MAX_VICTIMS) {
        return 0;
    }

    heap->items[heap->size] = *victim;
    heap_sift_up(heap, heap->size);
    heap->size++;
    return 1;
}

/*
 * heap_pop -- remove and return the highest-priority victim from the max-heap.
 * Returns 1 on success, 0 when the heap is empty or arguments are invalid.
 */
static int heap_pop(ResqPriorityQueue *heap, ResqVictim *out)
{
    if (!heap || !out || heap->size == 0) {
        return 0;
    }

    *out = heap->items[0];                          /* copy the root (best) */
    heap->size--;
    if (heap->size > 0) {
        heap->items[0] = heap->items[heap->size];   /* move last to root */
        heap_sift_down(heap, 0);
    }
    return 1;
}

/*
 * heap_peek -- copy the root element without removing it.
 * Returns 1 on success, 0 when empty.
 */
static int heap_peek(const ResqPriorityQueue *heap, ResqVictim *out)
{
    if (!heap || !out || heap->size == 0) {
        return 0;
    }
    *out = heap->items[0];
    return 1;
}

/* ================================================================== */
/*  Store lifecycle                                                     */
/* ================================================================== */

/*
 * resq_store_init -- zero-initialise the data store.
 * Must be called once before any other store operation.
 */
void resq_store_init(ResqDataStore *store)
{
    if (!store) {
        return;
    }
    memset(store, 0, sizeof(*store));
    store->history.top = -1;
    store->next_id     = 1;
    store->next_seq    = 0;
}

/*
 * resq_store_clear -- free all linked-list nodes and reset all queues.
 * Also resets next_id so a fresh scenario starts from victim #1.
 * Safe to call on an empty store.
 */
void resq_store_clear(ResqDataStore *store)
{
    if (!store) {
        return;
    }

    free_list_nodes(&store->records);

    store->critical.size     = 0;
    store->urgent.front      = 0;
    store->urgent.rear       = 0;
    store->urgent.count      = 0;
    store->routine.front     = 0;
    store->routine.rear      = 0;
    store->routine.count     = 0;
    store->history.top       = -1;
    store->next_id           = 1;
    store->next_seq          = 0;
}

/* ================================================================== */
/*  Input validation                                                    */
/* ================================================================== */

/*
 * resq_valid_text -- check that 'text' is a non-empty, non-NULL C string
 * that fits within max_len characters (not including the null terminator).
 * Also rejects strings containing bare newlines or carriage returns.
 * Returns 1 if valid, 0 otherwise.
 */
int resq_valid_text(const char *text, int max_len)
{
    size_t length;
    size_t i;

    if (!text || max_len <= 1) {
        return 0;
    }

    length = strlen(text);
    if (length == 0 || length >= (size_t)max_len) {
        return 0;
    }

    for (i = 0; i < length; ++i) {
        if (text[i] == '\n' || text[i] == '\r') {
            return 0;
        }
    }

    return 1;
}

/* ================================================================== */
/*  Priority scoring                                                    */
/* ================================================================== */

/*
 * resq_priority_score -- compute the numeric triage priority for a victim.
 *
 * Formula:
 *   base_score = RED:300  YELLOW:200  GREEN:100
 *   people_score = min(affected * 2, 80)   (capped to avoid dominating)
 *   waiting_bonus = 10 if status == 0 (waiting)
 *   total = base_score + people_score + waiting_bonus
 *
 * Higher score means the victim needs attention sooner.
 * Returns -1 for a NULL pointer.
 */
int resq_priority_score(const ResqVictim *victim)
{
    int base_score;
    int people_score;
    int waiting_bonus;

    if (!victim) {
        return -1;
    }

    switch (victim->severity) {
        case RESQ_RED:    base_score = 300; break;
        case RESQ_YELLOW: base_score = 200; break;
        default:          base_score = 100; break; /* RESQ_GREEN */
    }

    people_score = victim->affected * 2;
    if (people_score > 80) {
        people_score = 80;
    }

    waiting_bonus = (victim->status == 0) ? 10 : 0;

    return base_score + people_score + waiting_bonus;
}

/* ================================================================== */
/*  Linked-list operations (master victim records)                     */
/* ================================================================== */

/*
 * resq_store_add -- append a new victim record to the master linked list.
 *
 * The list uses a tail pointer so each append is O(1).  The function
 * rejects duplicates (same id already in list) and enforces the
 * RESQ_MAX_VICTIMS ceiling.
 *
 * Returns 1 on success, 0 on failure (full, duplicate, or bad arguments).
 */
int resq_store_add(ResqDataStore *store, const ResqVictim *victim)
{
    ResqVictimNode *new_node;

    if (!store || !victim) {
        return 0;
    }
    if (store->records.size >= RESQ_MAX_VICTIMS) {
        return 0;
    }
    if (find_node(&store->records, victim->id) != NULL) {
        return 0; /* duplicate */
    }

    new_node = (ResqVictimNode *)malloc(sizeof(ResqVictimNode));
    if (!new_node) {
        return 0; /* allocation failure */
    }

    new_node->value = *victim;
    new_node->next  = NULL;

    if (store->records.head == NULL) {
        store->records.head = new_node;
        store->records.tail = new_node;
    } else {
        store->records.tail->next = new_node;
        store->records.tail       = new_node;
    }

    store->records.size++;

    /* Keep next_id above any manually-assigned id */
    if (victim->id >= store->next_id) {
        store->next_id = victim->id + 1;
    }

    return 1;
}

/*
 * resq_store_find -- return a pointer to the live ResqVictim inside the
 * linked list for the given victim_id, or NULL if not found.
 * Writing through this pointer updates the master record in-place.
 */
ResqVictim *resq_store_find(ResqDataStore *store, int victim_id)
{
    ResqVictimNode *node = find_node(store ? &store->records : NULL, victim_id);
    return node ? &node->value : NULL;
}

/*
 * resq_store_update_status -- update the status field of a victim record.
 * Validates that new_status is in [0, 4].
 * Returns 1 on success, 0 on failure.
 */
int resq_store_update_status(ResqDataStore *store, int victim_id, int new_status)
{
    ResqVictim *victim = resq_store_find(store, victim_id);

    if (!victim || new_status < 0 || new_status > 4) {
        return 0;
    }
    victim->status = new_status;
    return 1;
}

/* ================================================================== */
/*  Dispatch queue operations                                           */
/* ================================================================== */

/*
 * resq_store_enqueue -- place a victim into the correct dispatch queue:
 *   RED    -> critical max-heap (highest priority first)
 *   YELLOW -> urgent circular FIFO (served before GREEN, in arrival order)
 *   GREEN  -> routine circular FIFO (served last, in arrival order)
 *
 * This three-tier routing ensures that YELLOW (urgent) victims are always
 * dispatched before GREEN (non-urgent) victims regardless of arrival time,
 * which the original single-FIFO design did not guarantee.
 *
 * Returns 1 on success, 0 when the target queue is full.
 */
int resq_store_enqueue(ResqDataStore *store, const ResqVictim *victim)
{
    if (!store || !victim) {
        return 0;
    }

    if (victim->severity == RESQ_RED) {
        return heap_push(&store->critical, victim);
    }

    if (victim->severity == RESQ_YELLOW) {
        if (store->urgent.count >= RESQ_MAX_VICTIMS) {
            return 0;
        }
        store->urgent.items[store->urgent.rear] = *victim;
        store->urgent.rear = (store->urgent.rear + 1) % RESQ_MAX_VICTIMS;
        store->urgent.count++;
        return 1;
    }

    /* RESQ_GREEN */
    if (store->routine.count >= RESQ_MAX_VICTIMS) {
        return 0;
    }
    store->routine.items[store->routine.rear] = *victim;
    store->routine.rear = (store->routine.rear + 1) % RESQ_MAX_VICTIMS;
    store->routine.count++;
    return 1;
}

/*
 * resq_store_peek_tier -- non-destructive read of the head of ONE specific tier.
 *
 * tier 0 = critical heap (RED), tier 1 = urgent FIFO (YELLOW),
 * tier 2 = routine FIFO (GREEN).
 *
 * Returns 1 if the tier is non-empty and 'out' was filled, 0 otherwise.
 * Invalid tier values return 0.
 */
int resq_store_peek_tier(const ResqDataStore *store, int tier, ResqVictim *out)
{
    if (!store || !out) {
        return 0;
    }

    switch (tier) {
        case 0: /* critical heap */
            return heap_peek(&store->critical, out);

        case 1: /* urgent FIFO */
            if (store->urgent.count > 0) {
                *out = store->urgent.items[store->urgent.front];
                return 1;
            }
            return 0;

        case 2: /* routine FIFO */
            if (store->routine.count > 0) {
                *out = store->routine.items[store->routine.front];
                return 1;
            }
            return 0;

        default:
            return 0;
    }
}

/*
 * resq_store_pop_tier -- destructively remove and return the head of ONE
 * specific tier.  Called only after a successful dispatch confirmation.
 *
 * tier 0 = critical heap (RED), tier 1 = urgent FIFO (YELLOW),
 * tier 2 = routine FIFO (GREEN).
 *
 * Returns 1 on success, 0 when the tier is empty or arguments are invalid.
 */
int resq_store_pop_tier(ResqDataStore *store, int tier, ResqVictim *out)
{
    if (!store || !out) {
        return 0;
    }

    switch (tier) {
        case 0: /* critical heap */
            return heap_pop(&store->critical, out);

        case 1: /* urgent FIFO */
            if (store->urgent.count > 0) {
                *out = store->urgent.items[store->urgent.front];
                store->urgent.front = (store->urgent.front + 1) % RESQ_MAX_VICTIMS;
                store->urgent.count--;
                return 1;
            }
            return 0;

        case 2: /* routine FIFO */
            if (store->routine.count > 0) {
                *out = store->routine.items[store->routine.front];
                store->routine.front = (store->routine.front + 1) % RESQ_MAX_VICTIMS;
                store->routine.count--;
                return 1;
            }
            return 0;

        default:
            return 0;
    }
}

/*
 * resq_store_peek_next -- copy the next victim that would be dispatched
 * WITHOUT removing it from the queue.
 *
 * Priority order: critical heap first, then urgent FIFO, then routine FIFO.
 *
 * This is used by the controller before dispatch so that it can check
 * availability and reachability and decide to skip or dispatch WITHOUT
 * permanently dropping the victim when the check fails.
 *
 * Returns 1 if a victim was found, 0 if all queues are empty.
 */
int resq_store_peek_next(const ResqDataStore *store, ResqVictim *out)
{
    if (!store || !out) {
        return 0;
    }

    /* Check critical heap first */
    if (heap_peek(&store->critical, out)) {
        return 1;
    }

    /* Then urgent FIFO */
    if (store->urgent.count > 0) {
        *out = store->urgent.items[store->urgent.front];
        return 1;
    }

    /* Finally routine FIFO */
    if (store->routine.count > 0) {
        *out = store->routine.items[store->routine.front];
        return 1;
    }

    return 0; /* all queues empty */
}

/*
 * resq_store_pop_next -- remove and return the highest-priority pending victim.
 * Call this ONLY after a successful dispatch, not as a speculative check.
 *
 * Priority order: critical heap, urgent FIFO, routine FIFO.
 * Returns 1 on success, 0 when all queues are empty.
 */
int resq_store_pop_next(ResqDataStore *store, ResqVictim *out)
{
    if (!store || !out) {
        return 0;
    }

    if (heap_pop(&store->critical, out)) {
        return 1;
    }

    if (store->urgent.count > 0) {
        *out = store->urgent.items[store->urgent.front];
        store->urgent.front = (store->urgent.front + 1) % RESQ_MAX_VICTIMS;
        store->urgent.count--;
        return 1;
    }

    if (store->routine.count > 0) {
        *out = store->routine.items[store->routine.front];
        store->routine.front = (store->routine.front + 1) % RESQ_MAX_VICTIMS;
        store->routine.count--;
        return 1;
    }

    return 0;
}

/* ================================================================== */
/*  History stack                                                       */
/* ================================================================== */

/*
 * resq_history_push -- push a resolved victim onto the LIFO history stack.
 * Returns 1 on success, 0 when the stack is full.
 */
int resq_history_push(ResqDataStore *store, const ResqVictim *victim)
{
    if (!store || !victim) {
        return 0;
    }
    if (store->history.top + 1 >= RESQ_MAX_HISTORY) {
        return 0; /* stack full */
    }
    store->history.top++;
    store->history.items[store->history.top] = *victim;
    return 1;
}

/*
 * resq_history_peek_all -- copy all history entries into out_array in
 * newest-first order WITHOUT modifying the stack.
 *
 * This replaces the previous resq_history_pop / re-push pattern that
 * permanently inverted the stack on every call to printHistory().
 *
 * out_count is set to the number of entries copied.
 * Returns 1 if at least one entry was copied, 0 otherwise.
 */
int resq_history_peek_all(const ResqDataStore *store,
                          ResqVictim *out_array, int *out_count)
{
    int i;
    int entry_count;

    if (!store || !out_array || !out_count) {
        return 0;
    }

    entry_count = store->history.top + 1; /* number of valid entries */
    *out_count  = 0;

    if (entry_count <= 0) {
        return 0;
    }

    /* Copy from top downward (newest first) */
    for (i = store->history.top; i >= 0; --i) {
        out_array[*out_count] = store->history.items[i];
        (*out_count)++;
    }

    return 1;
}

/* ================================================================== */
/*  Utility: copy linked list to flat array                            */
/* ================================================================== */

/*
 * resq_list_to_array -- copy all victim records from the linked list into a
 * flat array for sorting and searching.
 *
 * Returns the number of records copied (capped at max_count).
 */
int resq_list_to_array(const ResqDataStore *store,
                       ResqVictim *out_arr, int max_count)
{
    const ResqVictimNode *node;
    int count = 0;

    if (!store || !out_arr || max_count <= 0) {
        return 0;
    }

    for (node = store->records.head; node != NULL && count < max_count;
         node = node->next) {
        out_arr[count] = node->value;
        count++;
    }

    return count;
}

/* ================================================================== */
/*  Sorting algorithms                                                  */
/* ================================================================== */

/*
 * victim_sort_gt -- return non-zero if victim 'a' should appear before 'b'
 * in a priority-descending sort (highest score first; lower id as tiebreak).
 */
static int victim_sort_gt(const ResqVictim *a, const ResqVictim *b)
{
    int sa = resq_priority_score(a);
    int sb = resq_priority_score(b);
    if (sa != sb) {
        return sa > sb;   /* higher score first */
    }
    return a->id < b->id; /* earlier arrival (lower id) first */
}

/*
 * resq_sort_insertion -- stable insertion sort on a flat ResqVictim array.
 *
 * Complexity: O(N^2) comparisons and shifts in the worst case, but
 * O(N) for nearly-sorted data.  Chosen here because the victim list is
 * typically small (< 200) and often nearly ordered by arrival.
 *
 * Order: priority score descending; victim ID ascending as tie-break.
 */
void resq_sort_insertion(ResqVictim *arr, int n)
{
    int        i, j;
    ResqVictim key;

    if (!arr || n <= 1) {
        return;
    }

    for (i = 1; i < n; ++i) {
        key = arr[i];
        j   = i - 1;

        /* Shift elements that are "lower priority" than key to the right */
        while (j >= 0 && victim_sort_gt(&key, &arr[j])) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/*
 * quick_partition -- rearrange arr[low..high] around a pivot chosen as the
 * median of arr[low], arr[mid] and arr[high].  Returns the final pivot index.
 *
 * Uses median-of-three to avoid worst-case O(N^2) on already-sorted input.
 */
static int quick_partition(ResqVictim *arr, int low, int high)
{
    int        mid;
    ResqVictim temp;
    ResqVictim pivot;
    int        store_index;
    int        i;

    mid = low + (high - low) / 2;

    /* Place the median of three at arr[high] as the pivot */
    if (victim_sort_gt(&arr[mid], &arr[low])) {
        temp = arr[mid]; arr[mid] = arr[low]; arr[low] = temp;
    }
    if (victim_sort_gt(&arr[high], &arr[low])) {
        temp = arr[high]; arr[high] = arr[low]; arr[low] = temp;
    }
    if (victim_sort_gt(&arr[mid], &arr[high])) {
        temp = arr[mid]; arr[mid] = arr[high]; arr[high] = temp;
    }
    pivot = arr[high];

    store_index = low;
    for (i = low; i < high; ++i) {
        if (victim_sort_gt(&arr[i], &pivot)) {
            temp = arr[i]; arr[i] = arr[store_index]; arr[store_index] = temp;
            store_index++;
        }
    }

    temp = arr[store_index]; arr[store_index] = arr[high]; arr[high] = temp;
    return store_index;
}

/*
 * resq_sort_quick -- in-place quick sort on a flat ResqVictim array.
 *
 * Complexity: O(N log N) average, O(N^2) worst case (mitigated by
 * median-of-three pivot selection).
 *
 * Call as:  resq_sort_quick(arr, 0, count - 1);
 * Order: priority score descending; victim ID ascending as tie-break.
 */
void resq_sort_quick(ResqVictim *arr, int low, int high)
{
    int pivot_index;

    if (!arr || low >= high) {
        return;
    }

    pivot_index = quick_partition(arr, low, high);
    resq_sort_quick(arr, low,            pivot_index - 1);
    resq_sort_quick(arr, pivot_index + 1, high);
}

/* ================================================================== */
/*  Search algorithms                                                   */
/* ================================================================== */

/*
 * resq_search_linear -- search an unsorted ResqVictim array for victim_id.
 *
 * Complexity: O(N).
 * Returns the array index where arr[index].id == victim_id, or -1 if not found.
 */
int resq_search_linear(const ResqVictim *arr, int n, int victim_id)
{
    int i;

    if (!arr || n <= 0) {
        return -1;
    }

    for (i = 0; i < n; ++i) {
        if (arr[i].id == victim_id) {
            return i;
        }
    }

    return -1; /* not found */
}

/*
 * resq_search_binary -- binary search on a ResqVictim array sorted by id ASC.
 *
 * Complexity: O(log N).
 * Returns the array index where arr[index].id == victim_id, or -1 if not found.
 *
 * IMPORTANT: the caller must ensure the array is sorted by id in ascending
 * order before calling this function; use resq_sort_insertion or a custom
 * sort keyed on id if needed.
 */
int resq_search_binary(const ResqVictim *arr, int n, int victim_id)
{
    int low  = 0;
    int high = n - 1;
    int mid;

    if (!arr || n <= 0) {
        return -1;
    }

    while (low <= high) {
        mid = low + (high - low) / 2;   /* avoids integer overflow */

        if (arr[mid].id == victim_id) {
            return mid;
        } else if (arr[mid].id < victim_id) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return -1; /* not found */
}

/* ================================================================== */
/*  Printing utilities                                                  */
/* ================================================================== */

/*
 * resq_print_severity -- print the string label for a severity level.
 */
void resq_print_severity(ResqSeverity severity)
{
    const char *label;

    switch (severity) {
        case RESQ_RED:    label = "RED";    break;
        case RESQ_YELLOW: label = "YELLOW"; break;
        default:          label = "GREEN";  break;
    }
    printf("%s", label);
}

/*
 * resq_print_status -- print the string label for a victim status code.
 */
void resq_print_status(int status)
{
    static const char *labels[] = {
        "Waiting", "Assigned", "In Progress", "Resolved", "Cancelled"
    };

    if (status >= 0 && status <= 4) {
        printf("%s", labels[status]);
    } else {
        printf("Unknown");
    }
}

/*
 * resq_print_victim -- print a single victim record on one line.
 * Format: #id  name  severity  affected=N  at LOCATION  status=STATUS  score=N
 */
void resq_print_victim(const ResqVictim *victim)
{
    const char *severity_label;

    if (!victim) {
        return;
    }

    switch (victim->severity) {
        case RESQ_RED:    severity_label = "RED";    break;
        case RESQ_YELLOW: severity_label = "YELLOW"; break;
        default:          severity_label = "GREEN";  break;
    }

    printf("#%-3d %-20s %-6s affected=%-3d at %-24s status=",
           victim->id,
           victim->name,
           severity_label,
           victim->affected,
           victim->location);

    resq_print_status(victim->status);
    printf("  score=%d\n", resq_priority_score(victim));
}
