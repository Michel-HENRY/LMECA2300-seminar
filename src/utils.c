#include "utils.h"

List* List_new() {
	List *list = (List*)malloc(sizeof(List));
	list->n = 0;
	list->head = list->tail = NULL;
	return list;
}

void List_append(List *l, void *v) {
	ListNode* node = (ListNode*)malloc(sizeof(ListNode));
	node->v = v;
	node->next = NULL;

	if (l->n == 0)
		l->head = l->tail = node;
	else {
		l->tail->next = node;
		l->tail = node;
	}
	l->n++;
}

void List_free(List* list, destructor des) {
	ListNode* node1 = list->head;
	ListNode* node2 = NULL;
	for (int i = 0; i < list->n; i++) {
		if (des != NULL) des(node1->v);
		node2 = node1;
		node1 = node1->next;
		free(node2);
	}
	free(list);
}

xy* xy_new(double x, double y) {
	xy* pt = (xy*)malloc(sizeof(xy));
	pt->x = x, pt->y = y;
	return pt;
}

void xy_reset(xy *p) {
	p->x = 0;
	p->y = 0;
}

Residual* residual_new (){
  Residual* res = (Residual*)malloc(sizeof(Residual));
  res->mass_eq = 0.0;
  res->momentum_x_eq = 0.0;
  res->momentum_y_eq = 0.0;

}
double rand_interval(double a, double b) {
	return (rand() / (double)RAND_MAX)*(b - a) + a;
}

double squared(double x) {
	return x*x;
}

double norm(xy *v) {
	return hypot(v->x, v->y);
}