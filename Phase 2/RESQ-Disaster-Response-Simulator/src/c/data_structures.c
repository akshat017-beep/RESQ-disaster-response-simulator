#include <stdio.h>
#include <stdlib.h>
#include "resq_c.h"

int resq_list_push(ResqLinkedList *list, const ResqVictim *victim) {
    ResqListNode *node;
    ResqListNode *tail;
    if (list == NULL || victim == NULL) return 0;
    node = (ResqListNode *)malloc(sizeof(ResqListNode));
    if (node == NULL) return 0;
    node->value = *victim;
    node->next = NULL;
    if (list->head == NULL) list->head = node;
    else {
        tail = list->head;
        while (tail->next != NULL) tail = tail->next;
        tail->next = node;
    }
    list->size++;
    return 1;
}

int resq_list_remove(ResqLinkedList *list, int id) {
    ResqListNode *current;
    ResqListNode *previous = NULL;
    if (list == NULL) return 0;
    current = list->head;
    while (current != NULL) {
        if (current->value.id == id) {
            if (previous == NULL) list->head = current->next;
            else previous->next = current->next;
            free(current);
            list->size--;
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return 0;
}

void resq_list_print(const ResqLinkedList *list) {
    const ResqListNode *node;
    if (list == NULL || list->head == NULL) {
        printf("Linked-list history is empty.\n");
        return;
    }
    node = list->head;
    while (node != NULL) {
        printf("History ID %d: %s | ", node->value.id, node->value.name);
        resq_print_status(node->value.status);
        printf("\n");
        node = node->next;
    }
}

void resq_list_clear(ResqLinkedList *list) {
    ResqListNode *node;
    if (list == NULL) return;
    node = list->head;
    while (node != NULL) {
        ResqListNode *next = node->next;
        free(node);
        node = next;
    }
    list->head = NULL;
    list->size = 0;
}

int resq_queue_push(ResqQueue *queue, const ResqVictim *victim) {
    if (queue == NULL || victim == NULL || queue->count >= RESQ_MAX_VICTIMS) return 0;
    queue->items[queue->rear] = *victim;
    queue->rear = (queue->rear + 1) % RESQ_MAX_VICTIMS;
    queue->count++;
    return 1;
}

int resq_queue_pop(ResqQueue *queue, ResqVictim *out) {
    if (queue == NULL || out == NULL || queue->count == 0) return 0;
    *out = queue->items[queue->front];
    queue->front = (queue->front + 1) % RESQ_MAX_VICTIMS;
    queue->count--;
    return 1;
}

void resq_queue_print(const ResqQueue *queue) {
    if (queue == NULL || queue->count == 0) {
        printf("FIFO queue is empty.\n");
        return;
    }
    for (int i = 0; i < queue->count; ++i) {
        int index = (queue->front + i) % RESQ_MAX_VICTIMS;
        printf("Position %d: victim %d - %s\n", i + 1, queue->items[index].id, queue->items[index].name);
    }
}

int resq_pq_push(ResqPriorityQueue *queue, const ResqVictim *victim) {
    int index;
    if (queue == NULL || victim == NULL || queue->size >= RESQ_MAX_VICTIMS) return 0;
    index = queue->size++;
    queue->items[index] = *victim;
    while (index > 0) {
        int parent = (index - 1) / 2;
        int child_score = resq_priority_score(&queue->items[index]);
        int parent_score = resq_priority_score(&queue->items[parent]);
        if (child_score <= parent_score) break;
        ResqVictim temp = queue->items[index];
        queue->items[index] = queue->items[parent];
        queue->items[parent] = temp;
        index = parent;
    }
    return 1;
}

int resq_pq_pop(ResqPriorityQueue *queue, ResqVictim *out) {
    int index = 0;
    if (queue == NULL || out == NULL || queue->size == 0) return 0;
    *out = queue->items[0];
    queue->size--;
    if (queue->size == 0) return 1;
    queue->items[0] = queue->items[queue->size];
    while (1) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int largest = index;
        if (left < queue->size &&
            resq_priority_score(&queue->items[left]) > resq_priority_score(&queue->items[largest])) largest = left;
        if (right < queue->size &&
            resq_priority_score(&queue->items[right]) > resq_priority_score(&queue->items[largest])) largest = right;
        if (largest == index) break;
        ResqVictim temp = queue->items[index];
        queue->items[index] = queue->items[largest];
        queue->items[largest] = temp;
        index = largest;
    }
    return 1;
}

void resq_pq_print(const ResqPriorityQueue *queue) {
    if (queue == NULL || queue->size == 0) {
        printf("Priority queue is empty.\n");
        return;
    }
    for (int i = 0; i < queue->size; ++i) {
        printf("Victim %d | score %d | ", queue->items[i].id, resq_priority_score(&queue->items[i]));
        resq_print_severity(queue->items[i].severity);
        printf("\n");
    }
}

int resq_stack_push(ResqStack *stack, const ResqVictim *victim) {
    if (stack == NULL || victim == NULL || stack->top + 1 >= RESQ_MAX_HISTORY) return 0;
    stack->items[++stack->top] = *victim;
    return 1;
}

int resq_stack_pop(ResqStack *stack, ResqVictim *out) {
    if (stack == NULL || out == NULL || stack->top < 0) return 0;
    *out = stack->items[stack->top--];
    return 1;
}

void resq_stack_print(const ResqStack *stack) {
    if (stack == NULL || stack->top < 0) {
        printf("History stack is empty.\n");
        return;
    }
    for (int i = stack->top; i >= 0; --i) {
        printf("Victim %d | %s | ", stack->items[i].id, stack->items[i].name);
        resq_print_status(stack->items[i].status);
        printf("\n");
    }
}
