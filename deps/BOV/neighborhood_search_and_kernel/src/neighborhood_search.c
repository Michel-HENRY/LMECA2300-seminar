#include "neighborhood_search.h"
#include <pthread.h>
#include <pthread.c>
#include <implement.h>

void neighbours_new(int index, neighborhood* neigh, int i, double d, int is_after, int is_potential)
{
	if (is_after) {
		neighbours* newP = malloc(sizeof(neighbours));
		CHECK_MALLOC(newP);
		newP->index = index;
		newP->distance = d;
		newP->next = neigh[i].potential_list;
		neigh[i].potential_list = newP;
		neigh[i].nPotentialNeighbours++;
	}
	if (!is_potential) {
		neighbours* new = malloc(sizeof(neighbours));
		CHECK_MALLOC(new);
		new->index = index;
		new->distance = d;
		pthread_mutex_lock(&neigh[i].mutex);
		new->next = neigh[i].list;
		neigh[i].list = new;
		neigh[i].nNeighbours++;
		pthread_mutex_unlock(&neigh[i].mutex);
	}
}

void neighbours_delete(neighbours* n) {
	if (n) {
		neighbours* temp = n->next;
		free(n);
		neighbours_delete(temp);
	}
}

neighborhood* neighborhood_new(GLsizei n, neighborhood* previous)
{
	neighborhood* neigh = calloc(n, sizeof(neighborhood));
	CHECK_MALLOC(neigh);
	for (int i = 0; i < n; i++) {
		neigh[i].index = i;
		neigh[i].mutex = PTHREAD_MUTEX_INITIALIZER;
		neigh[i].nNeighbours = 0;
		neigh[i].list = NULL;
		if (previous) {
			neigh[i].nPotentialNeighbours = previous[i].nPotentialNeighbours;
			neigh[i].potential_list = previous[i].potential_list;
		}
		else {
			neigh[i].nPotentialNeighbours = 0;
			neigh[i].potential_list = NULL;
		}
	}
	return neigh;
}

void neighborhood_delete(neighborhood* nh, GLsizei n) {
	for (int i = 0; i < n; i++) {
		neighbours_delete(nh[i].list);
		//neighbours_delete(nh[i].potential_list);
		pthread_mutex_destroy(&(nh[i].mutex));
	}
	free(nh);
}

struct node* node_new(cell* c, int i, int index)
{
	node* new = malloc(sizeof(node));
	CHECK_MALLOC(new);
	new->index = index;
	new->next = c[i].ResidentList;
	c[i].ResidentList = new;
	c[i].nResident++;
	return new;
}

void node_delete(node* n) {
	if (n) {
		neighbours* temp = n->next;
		free(n);
		node_delete(temp);
	}
}

cell* cell_new(GLsizei n)
{
	cell* c = calloc(n, sizeof(cell));
	CHECK_MALLOC(c);
	for (int i = 0; i < n; i++) {
		c[i].nResident = 0;
		c[i].ResidentList = NULL;
	}
	return c;
}

void cell_delete(cell* c, GLsizei n) {
	for (int i = 0; i < n; i++)
		node_delete(c[i].ResidentList);
	free(c);
}

void printNeighborhood(neighborhood* nh, GLfloat data[][8], int size) {
	for (int i = 0; i < size; i++) {
		printf("Resident %i : coordinate: %f %f   number of neighbours %i\n", i + 1, data[i][0], data[i][1], nh[i].nNeighbours);
		int j = 1;
		//for (neighbours* current = nh[i].list; current; current = current->next)
			//printf("   Neighbours %i : %f %f\n", j++, data[current->index][0], data[current->index][1]);
	}
}

void printCell(GLfloat data[][8], cell* c, int size) {
	for (int i = 0; i < size; i++) {
		printf("Cell %i : %i\n", i + 1, c[i].nResident);
		int j = 1;
		for (node* current = c[i].ResidentList; current; current = current->next)
			printf("   Neighbours %i : %f %f\n", j++, data[current->index][0], data[current->index][1]);
	}
}

// for v, a floating point value between 0 and 1, this function fills color with
// the improved jet colormap color corresponding to v
static void colormap(float v, float color[3])
{
	float v1 = 3.5 * (v - 0.7);
	float v2 = 1.25 * v;
	float v3 = fminf(0.5, v) * 2.0;

	color[0] = -v1 * v1 + 1.0f;
	color[1] = 6.0f * v2 * v2 * (1.0f - v2);
	color[2] = 5.5f * v3 * (1.0f - v3) * (1.0f - v3);

	// alternative: classical jet colormap
	// color[0] = 1.5 - 4.0 * fabs(v - 0.75);
	// color[1] = 1.5 - 4.0 * fabs(v - 0.5 );
	// color[2] = 1.5 - 4.0 * fabs(v - 0.25);
}

void protected_inc(protected_int* guard) {
	pthread_mutex_lock(&guard->mutex);
	guard->var++;
	pthread_mutex_unlock(&guard->mutex);
}

int protected_get(protected_int* guard) {
	pthread_mutex_lock(&guard->mutex);
	int value = guard->var;
	pthread_mutex_unlock(&guard->mutex);
	return value;
}

int protected_get_inc(protected_int* guard) {
	pthread_mutex_lock(&guard->mutex);
	int value = guard->var;
	guard->var++;
	pthread_mutex_unlock(&guard->mutex);
	return value;
}

void protected_reset(protected_int* guard) {
	pthread_mutex_lock(&guard->mutex);
	guard->var = 0;
	pthread_mutex_unlock(&guard->mutex);
}

double compute_kh(int nPoints, int RA) {
	double target = 21.0 / nPoints;
	if (nPoints < 21)
		return sqrt(2.0);
	else if (!RA)
		return sqrt(2.0) * target;
	else if (target <= M_PI / 4)
		return sqrt(4 * target / M_PI);
	double tolerance = 0.0000000001;
	double kh_min = 1.0;
	double kh_max = sqrt(2.0);
	while (fabs(kh_max - kh_min) >= tolerance) {
		kh_max = kh_min;
		kh_min = sqrt((target - sin(acos(1.0 / kh_min)) * kh_min) / ((M_PI / 4 - acos(1.0 / kh_min))));
	}
	return kh_min;
}

bov_points_t* find_grid_points(double kh, double xmax) {
	double size = 2 * xmax / kh;
	float(*pts)[2] = malloc((ceil(size) - 1) * 4 * sizeof(pts[0]));
	CHECK_MALLOC(pts);
	for (int i = 1; i < size; i++) {
		pts[(i - 1) * 4][0] = xmax * (2.0 * i / (size)-1.0);
		pts[(i - 1) * 4][1] = xmax * (-1.0);
		pts[(i - 1) * 4 + 1][0] = xmax * (2.0 * i / (size)-1.0);
		pts[(i - 1) * 4 + 1][1] = xmax * (1.0);
		pts[(i - 1) * 4 + 2][0] = xmax * (-1.0);
		pts[(i - 1) * 4 + 2][1] = xmax * (2.0 * i / (size)-1.0);
		pts[(i - 1) * 4 + 3][0] = xmax * (1.0);
		pts[(i - 1) * 4 + 3][1] = xmax * (2.0 * i / (size)-1.0);
	}
	bov_points_t* points = bov_points_new(pts, (ceil(size) - 1) * 4, GL_STATIC_DRAW);
	return points;
}


void* loop_thread(void* args) {
	lta* loop = (lta*)args;
	cell* cellArray = loop->cells;
	protected_int* cellCounter = loop->cellCounter;
	int size = ceil(loop->size);
	int this_thread_number = loop->this_thread_number;
	neighborhood* nh = loop->nh;
	double kh = loop->kh;
	double L = loop->L;
	GLsizei nPoints = loop->nPoints;
	int iterations = loop->iterations;
	int use_verlet = loop->use_verlet;
	int use_cells = loop->use_cells;
	int use_improved_method = loop->use_improved_method;
	int nThreads = loop->nThreads;
	unsigned long frameCount = 0;
	int i, j;
	if (use_cells && !(use_verlet && iterations))
		i = 0;
	else
		i = (int)(this_thread_number * ((double)nPoints) / nThreads);
	if (use_improved_method)
		j = i + 1;
	else
		j = 0;
	int nValid = 0;
	int nInvalid = 0;
	int nVerlet = 0;
	int checking_cell_number = -1;
	int this_cell_number = -1;
	int checked_cells = 0;
	cell checking_cell;
	node checking_node;
	neighbours checking_neighbours;
	cell this_cell;
	node this_node;
	int i_check = i - 1;
	int j_check = j - 1;
	int f_check = 1;
	int are_still_neighbours = 1;
	while (((!(use_cells && !(use_verlet && iterations)) && i < ((int)((this_thread_number + 1) * ((double)nPoints) / nThreads))) || (use_cells && !(use_verlet && iterations) && i < nPoints)) && this_cell_number < size * size) {
		if (i != i_check) {
			nValid = 0;
			nInvalid = 0;
			nVerlet = 0;
			int are_still_neighbours = 1;
			if (use_verlet && iterations) {
				if (nh[i].potential_list) {
					checking_neighbours = nh[i].potential_list[0];
				}
				else {
					are_still_neighbours = 0;
				}
			}
			else if (use_cells) {
				if (i == 0) {
					this_cell_number = protected_get_inc(cellCounter);
					while (!cellArray[this_cell_number].nResident && this_cell_number < size * size)
						this_cell_number = protected_get_inc(cellCounter);
					if (this_cell_number < size * size) {
						this_cell = cellArray[this_cell_number];
						this_node = this_cell.ResidentList[0];
					}
				}
				else {
					if (this_node.next)
						this_node = *this_node.next;
					else {
						this_cell_number = protected_get_inc(cellCounter);
						while (this_cell_number < size * size && !cellArray[this_cell_number].nResident && !cellArray[this_cell_number].ResidentList)
							this_cell_number = protected_get_inc(cellCounter);
						if (this_cell_number < size * size) {
							this_cell = cellArray[this_cell_number];
							this_node = this_cell.ResidentList[0];
						}
					}
				}
				checked_cells = 0;
				if (use_improved_method && this_cell_number < size * size) {
					if (this_node.next) {
						checking_cell_number = this_cell_number;
						checking_cell = cellArray[checking_cell_number];
						checking_node = *this_node.next;
					}
					else {
						checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
						while (checking_cell_number != -1 && !cellArray[checking_cell_number].nResident && !cellArray[checking_cell_number].ResidentList)
							checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
						if (checking_cell_number != -1) {
							checking_cell = cellArray[checking_cell_number];
							checking_node = checking_cell.ResidentList[0];
						}
					}
				}
				else if (this_cell_number < size * size) {
					checking_cell_number = find_next_cell(this_cell_number, checked_cells, size, use_improved_method);
					while (checking_cell_number != -1 && !cellArray[checking_cell_number].nResident && !cellArray[checking_cell_number].ResidentList)
						checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
					if (checking_cell_number != -1) {
						checking_cell = cellArray[checking_cell_number];
						checking_node = checking_cell.ResidentList[0];
					}
				}
			}
			if (use_improved_method && !(use_verlet && iterations)) {
				j_check = i;
				j = i + 1;
			}
		}
		i_check = i;
		if ((!(use_verlet && iterations) && use_cells && checking_cell_number == -1) || (use_verlet && iterations && !are_still_neighbours)) {
			i++;
			j_check = j;
		}
		if (j != j_check) {
			int index_i, index_j;
			if (use_verlet && iterations) {
				index_j = checking_neighbours.index;
				index_i = i;
			}
			else if (use_cells) {
				index_j = checking_node.index;
				index_i = this_node.index;
			}
			else {
				index_j = j;
				index_i = i;
			}
			double distance = sqrt((pow((double)loop->coord[index_j][0] - (double)loop->coord[index_i][0], 2) + pow((double)loop->coord[index_j][1] - (double)loop->coord[index_i][1], 2)));
			if (distance <= kh && index_i != index_j) {
				neighbours_new(index_j, nh, index_i, distance, !iterations, 0);
				if (use_improved_method)
					neighbours_new(index_i, nh, index_j, distance, 0, 0);
			}
			else if (use_verlet && !iterations && distance <= (kh + L) && index_i != index_j) {
				neighbours_new(index_j, nh, index_i, distance, !iterations, 1);
				if (use_improved_method)
					neighbours_new(index_i, nh, index_j, distance, 0, 1);
			}
			if (use_verlet && iterations) {
				if (checking_neighbours.next) {
					checking_neighbours = *(checking_neighbours.next);
				}
				else
					i++;
			}
			else if (use_cells)
				if (checking_node.next)
					checking_node = *(checking_node.next);
				else {
					checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
					while (checking_cell_number != -1 && !cellArray[checking_cell_number].nResident && !cellArray[checking_cell_number].ResidentList)
						checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
					if (checking_cell_number != -1) {
						checking_cell = cellArray[checking_cell_number];
						checking_node = cellArray[checking_cell_number].ResidentList[0];
					}
					else
						i++;
				}
			else
				if (j == nPoints - 1 || (i == nPoints - 1 && (j == nPoints - 2 || use_improved_method))) {
					i++;
				}
		}
		j_check = j;
		if (use_improved_method) {
			j++;
		}
		else if (!use_improved_method)
			j = (frameCount) % (nPoints - 1) + (int)(i <= ((frameCount) % (nPoints - 1)));
		frameCount++;
	}
}

void* loop_thread_with_drawing(void* args) {
	ltad* loop = (ltad*)args;
	cell* cellArray = loop->cells;
	protected_int* cellCounter = loop->cellCounter;
	protected_int* finished_threads = loop->finished_threads;
	int div = loop->div;
	int size = ceil(loop->size);
	int this_thread_number = loop->this_thread_number;
	neighborhood* nh = loop->nh;
	double kh = loop->kh;
	double L = loop->L;
	double scale = loop->scale;
	GLsizei nPoints = loop->nPoints;
	int iterations = loop->iterations;
	int use_verlet = loop->use_verlet;
	int use_cells = loop->use_cells;
	int use_improved_method = loop->use_improved_method;
	int nThreads = loop->nThreads;
	double points_width = 0.02;
	char name[32];
	snprintf(name, sizeof(name), "Neighborhood: thread %i", this_thread_number);
	bov_window_t* window = NULL;
	if (DRAWING) {
		window = bov_window_new(400, 400, name);
		bov_window_set_color(window, (GLfloat[]) { 0.9f, 0.85f, 0.8f, 1.0f });
	}
	float test_point_color[4] = { 0.0, 0.0, 1.0, 1.0 };
	float circle_color[4] = { 0.0, 0.0, 1.0, .2 };
	float verlet_circle_color[4] = { 1.0, 0.0, 1.0, .2 };
	float verlet_color[4] = { 1.0, 0.0, 1.0, 1.0 };
	float valid_color[4] = { 0.0, 1.0, 0.0, 1.0 };
	float invalid_color[4] = { 1.0, 0.0, 0.0, 1.0 };
	float other_color[4] = { 0.0, 1.0, 1.0, 1.0 };
	float finished_color[4] = { 1.0, 1.0, 1.0, 1.0 };
	GLuint* tabValid = NULL;
	GLuint* tabInvalid = NULL;
	GLuint* tabFinished = NULL;
	GLuint* tabVerlet = NULL;
	bov_order_t* valid_order = NULL;
	bov_order_t* invalid_order = NULL;
	bov_order_t* finished_order = NULL;
	bov_order_t* verlet_order = NULL;
	bov_points_t* unchecked_points = NULL;
	bov_points_t* this_point = NULL;
	bov_points_t* circle_point = NULL;
	bov_points_t* verlet_circle_point = NULL;
	bov_points_t* checking_point = NULL;
	bov_points_t* valid_points = NULL;
	bov_points_t* invalid_points = NULL;
	bov_points_t* finished_points = NULL;
	bov_points_t* verlet_points = NULL;
	bov_points_t* gridPoint = NULL;
	bov_points_t* pointset = NULL;
	if (DRAWING) {
		tabValid = calloc(nPoints, sizeof(GLuint));
		CHECK_MALLOC(tabValid);
		tabInvalid = calloc(nPoints, sizeof(GLuint));
		CHECK_MALLOC(tabInvalid);
		if (use_improved_method) {
			tabFinished = calloc(nPoints, sizeof(GLuint));
			CHECK_MALLOC(tabFinished);
			finished_order = bov_order_new(tabFinished, nPoints + 1, GL_DYNAMIC_DRAW);
		}
		valid_order = bov_order_new(tabValid, nPoints + 1, GL_DYNAMIC_DRAW);
		invalid_order = bov_order_new(tabInvalid, nPoints + 1, GL_DYNAMIC_DRAW);
		unchecked_points = bov_particles_new(loop->data, nPoints, GL_DYNAMIC_DRAW);
		this_point = bov_points_new(loop->coord, 1, GL_DYNAMIC_DRAW);
		if (use_verlet) {
			tabVerlet = calloc(nPoints, sizeof(GLuint));
			CHECK_MALLOC(tabVerlet);
			verlet_circle_point = bov_points_new(loop->coord, 1, GL_DYNAMIC_DRAW);
			verlet_order = bov_order_new(tabVerlet, nPoints + 1, GL_DYNAMIC_DRAW);
			verlet_points = bov_points_new(loop->coord, nPoints, GL_STATIC_DRAW);
		}
		circle_point = bov_points_new(loop->coord, 1, GL_DYNAMIC_DRAW);
		checking_point = bov_points_new(loop->coord, 0, GL_DYNAMIC_DRAW);
		valid_points = bov_points_new(loop->coord, nPoints, GL_STATIC_DRAW);
		invalid_points = bov_points_new(loop->coord, nPoints, GL_STATIC_DRAW);
		if (use_improved_method)
			finished_points = bov_points_new(loop->coord, nPoints, GL_STATIC_DRAW);
		pointset = bov_points_new((float[4][2]) {
			{-100, 100},
			{ 100, 100 },
			{ 100,-100 },
			{ -100,-100 }
		}, 4, GL_STATIC_DRAW);
		bov_points_param_t lineParams = {
			.fillColor = {1.0, 1.0, 1.0, 1.0},
			.scale = {1.0, 1.0},
			.width = 0.005
		};
		if (use_cells) {
			gridPoint = find_grid_points(kh + L, 100);
			bov_points_set_param(gridPoint, lineParams);
			bov_points_scale(gridPoint, (GLfloat[2]) { scale, scale });
			bov_points_set_pos(gridPoint, (GLfloat[2]) { 0.0, -0.1 });
		}
		if (use_verlet) {
			bov_points_set_color(verlet_circle_point, verlet_circle_color);
			bov_points_set_width(verlet_circle_point, (kh + L) * scale);
			bov_points_scale(verlet_circle_point, (GLfloat[2]) { scale, scale });
			bov_points_set_pos(verlet_circle_point, (GLfloat[2]) { 0.0, -0.1 });
			bov_points_set_color(verlet_points, verlet_color);
			bov_points_set_width(verlet_points, points_width);
			bov_points_set_outline_width(verlet_points, 0.0025);
			bov_points_scale(verlet_points, (GLfloat[2]) { scale, scale });
			bov_points_set_pos(verlet_points, (GLfloat[2]) { 0.0, -0.1 });
		}
		bov_points_set_param(pointset, lineParams);
		bov_points_scale(pointset, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(pointset, (GLfloat[2]) { 0.0, -0.1 });
		bov_points_set_color(this_point, test_point_color);
		bov_points_set_color(checking_point, other_color);
		bov_points_set_color(circle_point, circle_color);
		bov_points_set_color(valid_points, valid_color);
		bov_points_set_color(invalid_points, invalid_color);
		if (use_improved_method)
			bov_points_set_color(finished_points, finished_color);
		bov_points_set_outline_width(circle_point, 0.);
		bov_points_set_width(circle_point, kh * scale);
		bov_points_set_width(this_point, points_width);
		bov_points_set_width(checking_point, points_width);
		bov_points_set_width(unchecked_points, points_width);
		bov_points_set_width(valid_points, points_width);
		bov_points_set_width(invalid_points, points_width);
		if (use_improved_method)
			bov_points_set_width(finished_points, points_width);
		bov_points_set_outline_width(this_point, 0.0025);
		bov_points_set_outline_width(checking_point, 0.0025);
		bov_points_set_outline_width(unchecked_points, 0.0025);
		bov_points_set_outline_width(valid_points, 0.0025);
		bov_points_set_outline_width(invalid_points, 0.0025);
		if (use_improved_method)
			bov_points_set_outline_width(finished_points, 0.0025);
		bov_points_scale(this_point, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(this_point, (GLfloat[2]) { 0.0, -0.1 });
		bov_points_scale(checking_point, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(checking_point, (GLfloat[2]) { 0.0, -0.1 });
		bov_points_scale(unchecked_points, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(unchecked_points, (GLfloat[2]) { 0.0, -0.1 });
		bov_points_scale(valid_points, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(valid_points, (GLfloat[2]) { 0.0, -0.1 });
		bov_points_scale(invalid_points, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(invalid_points, (GLfloat[2]) { 0.0, -0.1 });
		if (use_improved_method) {
			bov_points_scale(finished_points, (GLfloat[2]) { scale, scale });
			bov_points_set_pos(finished_points, (GLfloat[2]) { 0.0, -0.1 });
		}
		bov_points_scale(circle_point, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(circle_point, (GLfloat[2]) { 0.0, -0.1 });
	}
	unsigned long frameCount = 0;
	int i, j;
	if (use_cells && !(use_verlet && iterations))
		i = 0;
	else
		i = (int)(this_thread_number * ((double)nPoints) / nThreads);
	if (use_improved_method)
		j = i + 1;
	else
		j = 0;
	int nValid = 0;
	int nInvalid = 0;
	int nVerlet = 0;
	int checking_cell_number = -1;
	int this_cell_number = -1;
	int checked_cells = 0;
	cell checking_cell;
	node checking_node;
	neighbours checking_neighbours;
	cell this_cell;
	node this_node;
	int i_check = i - 1;
	int j_check = j - 1;
	int f_check = 1;
	int are_still_neighbours = 1;
	if (!DRAWING)
		div = 1;
	while ((!DRAWING || !bov_window_should_close(window)) && ((!(use_cells && !(use_verlet && iterations)) && i < ((int)((this_thread_number + 1) * ((double)nPoints) / nThreads))) || (use_cells && !(use_verlet && iterations))) && this_cell_number < size * size) {
		if (i != i_check) {
			nValid = 0;
			nInvalid = 0;
			nVerlet = 0;
			are_still_neighbours = 1;
			if (use_verlet && iterations) {
				if (nh[i].potential_list) {
					checking_neighbours = nh[i].potential_list[0];
				}
				else {
					are_still_neighbours = 0;
				}
				if (DRAWING) {
					bov_points_update(this_point, loop->coord[i], 1);
					bov_points_update(circle_point, loop->coord[i], 1);
					if (use_verlet)
						bov_points_update(verlet_circle_point, loop->coord[i], 1);
				}
			}
			else if (use_cells) {
				if (i == 0) {
					this_cell_number = protected_get_inc(cellCounter);
					while (!cellArray[this_cell_number].nResident && this_cell_number < size * size)
						this_cell_number = protected_get_inc(cellCounter);
					if (this_cell_number < size * size) {
						this_cell = cellArray[this_cell_number];
						this_node = this_cell.ResidentList[0];
					}
				}
				else {
					if (this_node.next)
						this_node = *this_node.next;
					else {
						this_cell_number = protected_get_inc(cellCounter);
						while (this_cell_number < size * size && !cellArray[this_cell_number].nResident && !cellArray[this_cell_number].ResidentList)
							this_cell_number = protected_get_inc(cellCounter);
						if (this_cell_number < size * size) {
							this_cell = cellArray[this_cell_number];
							this_node = this_cell.ResidentList[0];
						}
					}
				}
				checked_cells = 0;
				if (use_improved_method && this_cell_number < size * size) {
					if (this_node.next) {
						checking_cell_number = this_cell_number;
						checking_cell = cellArray[checking_cell_number];
						checking_node = *this_node.next;
					}
					else {
						checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
						while (checking_cell_number != -1 && !cellArray[checking_cell_number].nResident && !cellArray[checking_cell_number].ResidentList)
							checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
						if (checking_cell_number != -1) {
							checking_cell = cellArray[checking_cell_number];
							checking_node = checking_cell.ResidentList[0];
						}
					}
				}
				else if (this_cell_number < size * size) {
					checking_cell_number = find_next_cell(this_cell_number, checked_cells, size, use_improved_method);
					while (checking_cell_number != -1 && !cellArray[checking_cell_number].nResident && !cellArray[checking_cell_number].ResidentList)
						checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
					if (checking_cell_number != -1) {
						checking_cell = cellArray[checking_cell_number];
						checking_node = checking_cell.ResidentList[0];
					}
				}
				if (DRAWING && this_cell_number < size * size) {
					bov_points_update(this_point, loop->coord[this_node.index], 1);
					bov_points_update(circle_point, loop->coord[this_node.index], 1);
					if (use_verlet)
						bov_points_update(verlet_circle_point, loop->coord[this_node.index], 1);
				}
			}
			else if (DRAWING) {
				bov_points_update(this_point, loop->coord[i], 1);
				bov_points_update(circle_point, loop->coord[i], 1);
				if (use_verlet)
					bov_points_update(verlet_circle_point, loop->coord[i], 1);
			}
			if (use_improved_method) {
				if (DRAWING) {
					if (use_cells && !(use_verlet && iterations))
						tabFinished[i] = this_node.index;
					else
						tabFinished[i - (int)(this_thread_number * ((double)nPoints) / nThreads)] = i;
					finished_order = bov_order_partial_update(finished_order, tabFinished, 0, i, 0);
				}
				if (!(use_verlet && iterations)) {
					j_check = i;
					j = i + 1;
				}
			}
		}
		i_check = i;
		if ((use_cells && checking_cell_number == -1 && !(use_verlet && iterations)) || (use_verlet && iterations && !are_still_neighbours)) {
			i++;
			j_check = j;
		}
		if (j != j_check) {
			int index_i, index_j;
			if (use_verlet && iterations) {
				index_j = checking_neighbours.index;
				index_i = i;
			}
			else if (use_cells) {
				index_j = checking_node.index;
				index_i = this_node.index;
			}
			else {
				index_j = j;
				index_i = i;
			}
			if (DRAWING)
				bov_points_update(checking_point, loop->coord[index_j], 1);
			double distance = (pow((double)loop->coord[index_j][0] - (double)loop->coord[index_i][0], 2) + pow((double)loop->coord[index_j][1] - (double)loop->coord[index_i][1], 2));
			if (distance <= pow(kh, 2) && index_i != index_j) {
				if (DRAWING) {
					tabValid[nValid] = index_j;
					nValid++;
					bov_order_partial_update(valid_order, tabValid, 0, nValid, 0);
				}
				neighbours_new(index_j, nh, index_i, distance, !iterations, 0);
				if (use_improved_method)
					neighbours_new(index_i, nh, index_j, distance, 0, 0);
			}
			else if (use_verlet && !iterations && distance <= (kh + L) && index_i != index_j) {
				if (DRAWING) {
					tabVerlet[nVerlet] = index_j;
					nVerlet++;
					bov_order_partial_update(verlet_order, tabVerlet, 0, nVerlet, 0);
				}
				neighbours_new(index_j, nh, index_i, distance, !iterations, 1);
				if (use_improved_method)
					neighbours_new(index_i, nh, index_j, distance, 0, 1);
			}
			else if (DRAWING) {
				tabInvalid[nInvalid] = index_j;
				nInvalid++;
				bov_order_partial_update(invalid_order, tabInvalid, 0, nInvalid, 0);
			}
			if (use_verlet && iterations) {
				if (checking_neighbours.next) {
					checking_neighbours = *(checking_neighbours.next);
				}
				else
					i++;
			}
			else if (use_cells)
				if (checking_node.next)
					checking_node = *(checking_node.next);
				else {
					checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
					while (checking_cell_number != -1 && !cellArray[checking_cell_number].nResident && !cellArray[checking_cell_number].ResidentList)
						checking_cell_number = find_next_cell(this_cell_number, ++checked_cells, size, use_improved_method);
					if (checking_cell_number != -1) {
						checking_cell = cellArray[checking_cell_number];
						checking_node = cellArray[checking_cell_number].ResidentList[0];
					}
					else
						i++;
				}
			else
				if (j == nPoints - 1 || (i == nPoints - 1 && (j == nPoints - 2 || use_improved_method))) {
					i++;
				}
		}
		if (DRAWING) {
			bov_particles_draw(window, unchecked_points, 0, BOV_TILL_END);
			bov_points_draw_with_order(window, invalid_points, invalid_order, 0, nInvalid);
			bov_points_draw_with_order(window, valid_points, valid_order, 0, nValid);
			if (use_verlet)
				bov_points_draw_with_order(window, verlet_points, verlet_order, 0, nVerlet);
			if (use_improved_method)
				if (use_cells)
					bov_points_draw_with_order(window, finished_points, finished_order, 0, i);
				else
					bov_points_draw_with_order(window, finished_points, finished_order, 0, i - (int)(this_thread_number * ((double)nPoints) / nThreads));
			bov_line_loop_draw(window, pointset, 0, BOV_TILL_END);
			if (use_cells)
				bov_lines_draw(window, gridPoint, 0, BOV_TILL_END);
			bov_points_draw(window, this_point, 0, nPoints);
			bov_points_draw(window, checking_point, 0, 1);
			if (use_verlet)
				bov_points_draw(window, verlet_circle_point, 0, nPoints);
			bov_points_draw(window, circle_point, 0, nPoints);
			bov_window_update(window);
		}
		j_check = j;
		if (use_improved_method && f_check != frameCount / div) {
			j++;
		}
		else if (!use_improved_method)
			j = (frameCount / div) % (nPoints - 1) + (int)(i <= ((frameCount / div) % (nPoints - 1)));
		f_check = frameCount / div;
		frameCount++;
	}

	free(tabValid);
	free(tabInvalid);
	if (use_improved_method)
		free(tabFinished);
	bov_order_delete(valid_order);
	bov_order_delete(invalid_order);
	if (use_improved_method)
		bov_order_delete(finished_order);
	bov_points_delete(this_point);
	bov_points_delete(circle_point);
	bov_points_delete(checking_point);
	bov_points_delete(valid_points);
	bov_points_delete(invalid_points);
	if (use_improved_method)
		bov_points_delete(finished_points);
	bov_points_delete(unchecked_points);
	if (use_cells)
		bov_points_delete(gridPoint);
	protected_inc(finished_threads);
	return(window);
}

int find_next_cell(int this_cell, int checked_cells, int size, int improve) {
	if (this_cell == 0) {
		if (checked_cells != 4)
			return this_cell + checked_cells / 2 * size + checked_cells % 2;
	}
	else if (this_cell == size - 1) {
		checked_cells += improve;
		if (checked_cells != 4)
			return this_cell - 1 + checked_cells / 2 * size + checked_cells % 2;
	}
	else if (this_cell == (size - 1) * size) {
		checked_cells += 2 * improve;
		if (checked_cells != 4)
			return this_cell - size + checked_cells / 2 * size + checked_cells % 2;
	}
	else if (this_cell == size * size - 1) {
		checked_cells += 3 * improve;
		if (checked_cells < 4)
			return this_cell - size - 1 + checked_cells / 2 * size + checked_cells % 2;
	}
	else if (this_cell / size == 0) {
		checked_cells += improve;
		if (checked_cells != 6)
			return this_cell - 1 + checked_cells / 3 * size + checked_cells % 3;
	}
	else if (this_cell / size == size - 1) {
		checked_cells += 4 * improve;
		if (checked_cells != 6)
			return this_cell - size - 1 + checked_cells / 3 * size + checked_cells % 3;
	}
	else if (this_cell % size == 0) {
		checked_cells += 2 * improve;
		if (checked_cells != 6)
			return this_cell - size + checked_cells / 2 * size + checked_cells % 2;
	}
	else if (this_cell % size == size - 1) {
		checked_cells += 3 * improve;
		if (checked_cells != 6)
			return this_cell - size - 1 + checked_cells / 2 * size + checked_cells % 2;
	}
	else {
		checked_cells += 4 * improve;
		if (checked_cells != 9)
			return this_cell - size - 1 + checked_cells / 3 * size + checked_cells % 3;
	}
	return -1;
}

/*
Fills the data vector
Data verctor is organised like following
data[i][0 & 1] -> coordonate X and Y
data[i][2 & 3] -> speed in X and Y
data[i][4-5-6-7] -> code RGB for visual implementation and transparency
data[i][8 & 9] -> Values of the function f=(fx , fy);
data[i][10 & 11 & 12 & 13] -> value of the divergente, grandient in X, gradient in Y and laplacien
*/
void fillData(GLfloat(*data)[14], GLfloat(*coord)[2], int nPoints)
{
	float rmax = 100.0 * sqrtf(2.0f);
	for (int i = 0; i < NPTS; i++) {
		data[i][0] = rand() * 200.0 / RAND_MAX - 100.0; // x (rand between -100 and 100)
		data[i][1] = rand() * 200.0 / RAND_MAX - 100.0; // y (rand between -100 and 100)
		coord[i][0] = data[i][0];
		coord[i][1] = data[i][1];
		float r = sqrt(data[i][0] * data[i][0] + data[i][1] * data[i][1]);
		//data[i][2] = 0; //speed x (not used by default visualization)
		//data[i][3] = 0; // speed y (not used by default visualization)
		data[i][2] = rand() * 2.0 / RAND_MAX - 1.0; //Random starting speed
		data[i][3] = rand() * 2.0 / RAND_MAX - 1.0; //Random starting speed
		colormap(r / rmax, &data[i][4]); // fill color
		data[i][7] = 0.8f; // transparency
		//You have to define you function for example here we used fx=x*sin(y) and fy=y*cos(x)
		//data[i][8] = 5;
		data[i][8] = pow(data[i][0],1);
		//data[i][9] = 9;
		data[i][9] = pow(data[i][1],1);
	}
}

//Changes the particle velocities randomly and updates the positions based, we assume elastic collisions with boundaries:
//	-timestep: time intervals at which these are updated
//	-xmin,xmax,ymin,ymax: boundaries of the domain
//	-maxspeed: the maximum speed that can be reached by the particles

void bouncyrandomupdate(GLfloat(*data)[14], GLfloat(*coord)[2], double timestep, double xmin, double xmax, double ymin, double ymax, double maxspeed) {
	for (int i = 0; i < NPTS; i++) {
		double speed = sqrtf(data[i][2] * data[i][2] + data[i][3] * data[i][3]);
		data[i][0] += data[i][2] * timestep;
		data[i][1] += data[i][3] * timestep;
		coord[i][0] = data[i][0];
		coord[i][1] = data[i][1];
		data[i][2] += ((double)rand() / RAND_MAX - 0.5) * (0.05 * maxspeed) * timestep;
		data[i][3] += ((double)rand() / RAND_MAX - 0.5) * (0.05 * maxspeed) * timestep;

		if (speed > maxspeed) {//Slows down if speed too high
			data[i][2] = data[i][2] * 0.9;
			data[i][3] = data[i][3] * 0.9;
		}

		//This next part of the code handles the cases where a particle bounces of the wall

		//Particle is too high in x
		if (data[i][0] >= xmax) {
			data[i][0] -= 2 * (data[i][0] - xmax);
			data[i][2] = -data[i][2];
		}
		//Particle is too low in x
		if (data[i][0] <= xmin) {
			data[i][0] -= 2 * (data[i][0] - xmin);
			data[i][2] = -data[i][2];
		}
		//Particle is too high in y
		if (data[i][1] >= ymax) {
			data[i][1] -= 2 * (data[i][1] - ymax);
			data[i][3] = -data[i][3];
		}
		//Particle is too low in y
		if (data[i][1] <= ymin) {
			data[i][1] -= 2 * (data[i][1] - ymin);
			data[i][3] = -data[i][3];
		}
	}
}

int compute_optimal_verlet(double timestep, double maxspeed, double kh) {
	double a = -M_PI * 4 * timestep * timestep * maxspeed * maxspeed;

	double b = -4 * (M_PI * timestep * kh * maxspeed - 9 * maxspeed * maxspeed * timestep * timestep);
	double c = kh * kh * (9 - M_PI) - 36 * timestep * kh * maxspeed;
	double d = -9 * kh * kh;
	double adev = a * 3;
	double bdev = b * 2;
	double cdev = c;
	double discri = bdev * bdev - 4 * adev * cdev;
	if (discri <= 0) {
		return -1;
	}
	else {
		double first_ans = (-bdev + sqrt(discri)) / (2 * adev);
		double second_ans = (-bdev - sqrt(discri)) / (2 * adev);
		double first_y = a * first_ans * first_ans * first_ans + b * first_ans * first_ans + c * first_ans + d;
		double second_y = a * second_ans * second_ans * second_ans + b * second_ans * second_ans + c * second_ans + d;
		double max_ans;
		if (first_y > second_y) {
			max_ans = first_ans;
		}
		else {
			max_ans = second_ans;
		}
		if (max_ans < 2) { return -1; }
		else { return max(ceil(max_ans), floor(max_ans)); }
	}
}

neighborhood** create_neighborhood_with_drawing(GLfloat(*data)[14], GLfloat(*coord)[2], int iter, int data_filled, int nPoints, int use_cells, int use_improved_method, int radius_algorithm, int search_neighborhood, int use_verlet, int use_threads, int nThreads, unsigned int seed) {
	//Defining the domain
	int xmax = 100;
	int xmin = -100;
	int ymax = 100;
	int ymin = -100;
	bov_window_t* window = bov_window_new(800, 800, "ANM Project: SPH");
	bov_window_set_color(window, (GLfloat[]) { 0.9f, 0.85f, 0.8f, 0.0f });
	double scale = 0.008;
	double points_width = 0.02;

	double timestep = 0.5;
	double maxspeed = 1;
	int optimal_verlet_steps;

	int div = 1;

	if (seed == 0) {
		// Seed the random
		seed = time(NULL);
	}
	srand(seed);
	if (!data_filled)
		fillData(data, coord, nPoints);

	pthread_t* threads = NULL;
	use_threads = use_threads && nThreads >= 1;
	if (use_threads) {
		if (nPoints < nThreads)
			nThreads = nPoints;
	}
	else {
		use_threads = 1;
		nThreads = 1;
	}
	threads = malloc(nThreads * sizeof(pthread_t));
	CHECK_MALLOC(threads);
	double kh = KH;
	if (kh == 0)
		kh = compute_kh(nPoints, radius_algorithm) * (xmax - xmin);
	double L = 0.0;
	if (use_verlet == 1) {
		optimal_verlet_steps = compute_optimal_verlet(timestep, maxspeed, kh);
		//printf("Optimal Verlet %d\n", optimal_verlet_steps);
		if (optimal_verlet_steps == -1) { use_verlet = 0; }
		else { L = optimal_verlet_steps * timestep * maxspeed; }
	}

	// create the gridded data structure
	double width = (xmax - xmin) * scale / (kh + L);
	double height = (ymax - ymin) * scale / (kh + L);
	//printf("Width: %f\n", width);
	//printf("Height: %f\n", height);
	//printf("Cells Filled \n");

	// send data to GPU, and receive reference to those data in a points object
	bov_points_t* particles = bov_particles_new(data, nPoints, GL_STATIC_DRAW);

	// setting particles appearance
	bov_points_set_width(particles, 0.02);
	bov_points_set_outline_width(particles, 0.0025);

	// fit particles in a [-0.8 0.8]x[-0.8 0.8] square
	bov_points_scale(particles, (GLfloat[2]) { scale, scale });
	bov_points_set_pos(particles, (GLfloat[2]) { 0.0, -0.1 });

	// we got 0.2 at the top to write something. The screen goes from -1 to 1
	bov_text_t* msg = bov_text_new(
		(GLubyte[]) {
		"Rendering " xstr(nPoints) " particles"
	},
		GL_STATIC_DRAW);
	bov_text_set_pos(msg, (GLfloat[2]) { -0.95, 0.82 });
	bov_text_set_fontsize(msg, 0.1);

	bov_points_t* gridPoint = NULL;
	bov_points_t* pointset = bov_points_new((float[4][2]) {
		{-100, 100},
		{ 100, 100 },
		{ 100,-100 },
		{ -100,-100 }
	}, 4, GL_STATIC_DRAW);
	bov_points_param_t lineParams = {
	.fillColor = {1.0, 1.0, 1.0, 1.0},
	.scale = {1.0, 1.0},
	.width = 0.005
	};
	cell* cellArray = NULL;
	double size = (xmax - xmin) / (kh + L);
	double cellLength = (xmax - xmin) / (size);
	use_cells = use_cells && (kh + L) < (xmax - xmin) / 3.0;
	if (use_cells) {
		gridPoint = find_grid_points((kh + L), 100);
		bov_points_set_param(gridPoint, lineParams);
		bov_points_scale(gridPoint, (GLfloat[2]) { scale, scale });
		bov_points_set_pos(gridPoint, (GLfloat[2]) { 0.0, -0.1 });
	}
	bov_points_set_param(pointset, lineParams);
	bov_points_scale(pointset, (GLfloat[2]) { scale, scale });
	bov_points_set_pos(pointset, (GLfloat[2]) { 0.0, -0.1 });

	protected_int* cellCounter = NULL;
	if (use_cells) {
		cellCounter = malloc(sizeof(protected_int));
		CHECK_MALLOC(cellCounter);
		cellCounter->mutex = PTHREAD_MUTEX_INITIALIZER;
		cellCounter->var = 0;
	}
	neighborhood** list_of_neighborhood = malloc(iter * sizeof(neighborhood*));
	CHECK_MALLOC(list_of_neighborhood);
	protected_int* finished_threads = malloc(sizeof(protected_int));
	CHECK_MALLOC(finished_threads);
	finished_threads->mutex = PTHREAD_MUTEX_INITIALIZER;
	finished_threads->var = 0;
	int iterations = 0;
	ltad** loop = malloc(nThreads * sizeof(ltad*));
	CHECK_MALLOC(loop);
	for (int i = 0; i < nThreads; i++) {
		loop[i] = malloc(sizeof(ltad));
		CHECK_MALLOC(loop[i]);
	}
	while (iterations < iter) {
		if (search_neighborhood) {
			neighborhood* nh;
			if (use_verlet && (iterations % optimal_verlet_steps))
				nh = neighborhood_new(nPoints, list_of_neighborhood[iterations - 1]);
			else
				nh = neighborhood_new(nPoints, NULL);

			if (use_cells) {
				protected_reset(cellCounter);
				cellArray = cell_new(ceil(size) * ceil(size));
				for (int i = 0; i < nPoints; i++) {
					int cellNumber = ((int)((coord[i][1] - xmin) / (xmax - xmin) * size) * ceil(size) + (int)((coord[i][0] - xmin) / (xmax - xmin) * size));
					if (data[i][1] == xmax)
						cellNumber -= size;
					if (data[i][0] == xmax)
						cellNumber -= 1;
					node_new(cellArray, cellNumber, i);
				}
			}
			for (int i = 0; i < nThreads; i++) {
				loop[i]->cells = cellArray;
				loop[i]->use_cells = use_cells;
				loop[i]->use_verlet = use_verlet;
				loop[i]->use_improved_method = use_improved_method;
				loop[i]->div = div;
				loop[i]->size = size;
				loop[i]->nh = nh;
				loop[i]->kh = kh;
				loop[i]->L = L;
				loop[i]->nPoints = nPoints;
				loop[i]->coord = coord;
				loop[i]->data = data;
				loop[i]->nThreads = nThreads;
				loop[i]->this_thread_number = i;
				loop[i]->scale = scale;
				loop[i]->cellCounter = cellCounter;
				loop[i]->finished_threads = finished_threads;
				if (use_verlet)
					loop[i]->iterations = iterations % optimal_verlet_steps;
				else
					loop[i]->iterations = 0;
				pthread_create(&threads[i], NULL, loop_thread_with_drawing, loop[i]);
			}
			while (!bov_window_should_close(window) && protected_get(finished_threads) != nThreads) {
				bov_particles_draw(window, particles, 0, BOV_TILL_END);
				bov_text_draw(window, msg);
				bov_line_loop_draw(window, pointset, 0, BOV_TILL_END);
				if (use_cells)
					bov_lines_draw(window, gridPoint, 0, BOV_TILL_END);
				bov_window_update(window);
			}
			protected_reset(finished_threads);
			// Moves the particles around
			if (use_cells)
				cell_delete(cellArray, ceil(size) * ceil(size));

			void** retval = malloc(nThreads * sizeof(void*));
			for (int i = 0; i < nThreads; i++) {
				pthread_join(threads[i], &(retval[i]));
				free(retval[i]);
			}
			free(retval);

			list_of_neighborhood[iterations] = nh;
		}
		bouncyrandomupdate(data, coord, 1, -100, 100, -100, 100, 2);
		particles = bov_particles_update(particles, data, NPTS);
		bov_particles_draw(window, particles, 0, BOV_TILL_END);
		bov_text_draw(window, msg);
		bov_line_loop_draw(window, pointset, 0, BOV_TILL_END);
		if (use_cells)
			bov_lines_draw(window, gridPoint, 0, BOV_TILL_END);
		bov_window_update(window);
		iterations++;
	}
	for (int i = 0; i < nThreads; i++)
		free(loop[i]);
	free(loop);

	if (use_cells) {
		pthread_mutex_destroy(&(cellCounter->mutex));
		free(cellCounter);
		bov_points_delete(gridPoint);
	}

	pthread_mutex_destroy(&(finished_threads->mutex));
	free(finished_threads);
	free(threads);

	bov_points_delete(pointset);
	bov_text_delete(msg);
	bov_points_delete(particles);
	bov_window_delete(window);

	return list_of_neighborhood;
}

neighborhood** create_neighborhood(GLfloat(*data)[14], GLfloat(*coord)[2], int iter, int data_filled, int nPoints, int use_cells, int use_improved_method, int radius_algorithm, int use_verlet, int use_threads, int nThreads, unsigned int seed)
{
	//Defining the domain
	int xmax = 100;
	int xmin = -100;
	int ymax = 100;
	int ymin = -100;

	double points_width = 0.02;

	double timestep = 0.5;
	double maxspeed = 1;
	int optimal_verlet_steps;

	if (seed == 0) {
		// Seed the random
		seed = time(NULL);
	}
	srand(seed);
	if (!data_filled)
		fillData(data, coord, nPoints);
	pthread_t* threads = NULL;
	use_threads = use_threads && nThreads >= 1;
	if (use_threads) {
		if (nPoints < nThreads)
			nThreads = nPoints;
	}
	else {
		use_threads = 1;
		nThreads = 1;
	}
	threads = malloc(nThreads * sizeof(pthread_t));
	CHECK_MALLOC(threads);
	double kh = KH;
	if (kh == 0)
		kh = compute_kh(nPoints, radius_algorithm) * (xmax - xmin);
	double L = 0.0;
	printf("KH = %f \n", kh);
	if (use_verlet == 1) {
		optimal_verlet_steps = compute_optimal_verlet(timestep, maxspeed, kh);
		if (optimal_verlet_steps == -1) { use_verlet = 0; }
		else { L = optimal_verlet_steps * timestep * maxspeed; }
	}

	// create the gridded data structure
	double width = (xmax - xmin) / (kh + L);
	double height = (ymax - ymin) / (kh + L);

	cell* cellArray = NULL;
	double size = (xmax - xmin) / (kh + L);
	double cellLength = (xmax - xmin) / (size);
	use_cells = use_cells && (kh + L) < (xmax - xmin) / 3.0;
	protected_int* cellCounter = NULL;
	if (use_cells) {
		cellCounter = malloc(sizeof(protected_int));
		CHECK_MALLOC(cellCounter);
		cellCounter->mutex = PTHREAD_MUTEX_INITIALIZER;
		cellCounter->var = 0;
	}
	neighborhood** list_of_neighborhood = malloc(iter * sizeof(neighborhood*));
	CHECK_MALLOC(list_of_neighborhood);
	int iterations = 0;

	lta** loop = malloc(nThreads * sizeof(lta*));
	CHECK_MALLOC(loop);
	for (int i = 0; i < nThreads; i++) {
		loop[i] = malloc(sizeof(lta));
		CHECK_MALLOC(loop[i]);
	}
	while (iterations < iter) {
		neighborhood* nh;
		if (use_verlet && (iterations % optimal_verlet_steps))
			nh = neighborhood_new(nPoints, list_of_neighborhood[iterations - 1]);
		else
			nh = neighborhood_new(nPoints, NULL);

		if (use_cells) {
			protected_reset(cellCounter);
			cellArray = cell_new(ceil(size) * ceil(size));
			for (int i = 0; i < nPoints; i++) {
				int cellNumber = ((int)((coord[i][1] - xmin) / (xmax - xmin) * size) * ceil(size) + (int)((coord[i][0] - xmin) / (xmax - xmin) * size));
				if (data[i][1] == xmax)
					cellNumber -= size;
				if (data[i][0] == xmax)
					cellNumber -= 1;
				node_new(cellArray, cellNumber, i);
			}
		}
		for (int i = 0; i < nThreads; i++) {
			loop[i]->cells = cellArray;
			loop[i]->use_cells = use_cells;
			loop[i]->use_verlet = use_verlet;
			loop[i]->use_improved_method = use_improved_method;
			loop[i]->size = size;
			loop[i]->nh = nh;
			loop[i]->kh = kh;
			loop[i]->L = L;
			loop[i]->nPoints = nPoints;
			loop[i]->coord = coord;
			loop[i]->data = data;
			loop[i]->nThreads = nThreads;
			loop[i]->this_thread_number = i;
			loop[i]->cellCounter = cellCounter;
			if (use_verlet)
				loop[i]->iterations = iterations % optimal_verlet_steps;
			else
				loop[i]->iterations = 0;
			pthread_create(&threads[i], NULL, loop_thread, loop[i]);
		}
		for (int i = 0; i < nThreads; i++) {
			pthread_join(threads[i], NULL);
		}

		// Call the kernel function
		kernel(data, coord, nh, kh);

		// Moves the particles around
		if (use_cells)
			cell_delete(cellArray, ceil(size) * ceil(size));
		list_of_neighborhood[iterations] = nh;
		bouncyrandomupdate(data, coord, 1, -100, 100, -100, 100, 2);

		iterations++;
	}

	for (int i = 0; i < nThreads; i++)
		free(loop[i]);
	free(loop);

	if (use_cells) {
		pthread_mutex_destroy(&(cellCounter->mutex));
		free(cellCounter);
	}
	free(threads);
	return list_of_neighborhood;
}

int compare_neighborhood_lists(neighborhood** nhList_1, neighborhood** nhList_2, int iter, int nPoints) {
	int is_equal = 0;
	for (int i = 0; (i < iter) && !is_equal; i++) {
		is_equal = compare_neighborhoods(nhList_1[i], nhList_2[i], nPoints);
	}
	return is_equal;
}

int compare_neighborhoods(neighborhood* nh_1, neighborhood* nh_2, int nPoints) {
	for (int i = 0; i < nPoints; i++) {
		if (nh_1[i].nNeighbours != nh_2[i].nNeighbours)
			return 0;
		neighbours* current = NULL;
		for (current = nh_1[i].list; current; current = current->next) {
			int is_equal = 0;
			neighbours* temp = NULL;
			for (temp = nh_2[i].list; temp && !is_equal; temp = temp->next) {
				is_equal = current->index == temp->index;
			}
			if (!is_equal) {
				return 0;
			}
		}
	}
	return 1;
}

void show_equivalence_of_major_algorithm_combination(int drawing) {
	GLfloat(*data)[14] = malloc(sizeof(data[0]) * NPTS);
	CHECK_MALLOC(data);
	GLfloat(*coord)[2] = malloc(sizeof(coord[0]) * NPTS);
	CHECK_MALLOC(coord);

	// Seed the random
	time_t seed = time(NULL);
	printf(" %u ", seed);
	srand(seed);

	neighborhood** nhList0, ** nhList1, ** nhList2, ** nhList3, ** nhList4, ** nhList5, ** nhList6, ** nhList7, ** nhList8, ** nhList9, ** nhList10, ** nhList11, ** nhList12, ** nhList13, ** nhList14, ** nhList15;
	
	/* Takes way more time and memory, especially when using threads, but is supposed to give the same result as whithout drawing*/
	if (drawing) {
		nhList0 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 1, NTHREADS, seed);
		nhList1 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 0, 1, seed);
		nhList2 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 1, NTHREADS, seed);
		nhList3 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 0, 1, seed);
		nhList4 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 1, NTHREADS, seed);
		nhList5 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 0, 1, seed);
		nhList6 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 1, NTHREADS, seed);
		nhList7 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 0, 1, seed);
		nhList8 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 1, NTHREADS, seed);
		nhList9 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 0, 1, seed);
		nhList10 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 1, NTHREADS, seed);
		nhList11 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 0, 1, seed);
		nhList12 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 1, NTHREADS, seed);
		nhList13 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 1, 0, 1, seed);
		nhList14 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 1, NTHREADS, seed);
		nhList15 = create_neighborhood_with_drawing(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, SEARCH_NEIGHBORHOOD, 0, 0, 1, seed);
	}
	else {
		nhList0 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, 1, 1, NTHREADS, seed);
		/*nhList1 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, 1, 0, 1, seed);
		nhList2 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, 0, 1, NTHREADS, seed);
		nhList3 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 1, RADIUS_ALGORITHM, 0, 0, 1, seed);
		nhList4 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, 1, 1, NTHREADS, seed);
		nhList5 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, 1, 0, 1, seed);
		nhList6 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, 0, 1, NTHREADS, seed);
		nhList7 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 1, 0, RADIUS_ALGORITHM, 0, 0, 1, seed);
		nhList8 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, 1, 1, NTHREADS, seed);
		nhList9 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, 1, 0, 1, seed);
		nhList10 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, 0, 1, NTHREADS, seed);
		nhList11 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 1, RADIUS_ALGORITHM, 0, 0, 1, seed);
		nhList12 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, 1, 1, NTHREADS, seed);
		nhList13 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, 1, 0, 1, seed);
		nhList14 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, 0, 1, NTHREADS, seed);
		nhList15 = create_neighborhood(data, coord, MAX_ITER, 0, NPTS, 0, 0, RADIUS_ALGORITHM, 0, 0, 1, seed);*/
	}
	/*
	printf("\n0==1 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList0, nhList1, MAX_ITER, NPTS));
	printf("\n1==2 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList1, nhList2, MAX_ITER, NPTS));
	printf("\n2==3 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList2, nhList3, MAX_ITER, NPTS));
	printf("\n3==4 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList3, nhList4, MAX_ITER, NPTS));
	printf("\n4==5 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList4, nhList5, MAX_ITER, NPTS));
	printf("\n5==6 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList5, nhList6, MAX_ITER, NPTS));
	printf("\n6==7 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList6, nhList7, MAX_ITER, NPTS));
	printf("\n7==8 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList7, nhList8, MAX_ITER, NPTS));
	printf("\n8==9 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList8, nhList9, MAX_ITER, NPTS));
	printf("\n9==10 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList9, nhList10, MAX_ITER, NPTS));
	printf("\n10==11 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList10, nhList11, MAX_ITER, NPTS));
	printf("\n11==12 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList11, nhList12, MAX_ITER, NPTS));
	printf("\n12==13 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList12, nhList13, MAX_ITER, NPTS));
	printf("\n13==14 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList13, nhList14, MAX_ITER, NPTS));
	printf("\n14==15 ?");
	printf("\n  %i  \n", compare_neighborhood_lists(nhList14, nhList15, MAX_ITER, NPTS));

	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList0[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList1[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList2[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList3[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList4[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList5[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList6[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList7[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList8[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList9[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList10[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList11[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList12[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList13[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList14[i], NPTS);
	}
	for (int i = 0; i < MAX_ITER; i++) {
		neighborhood_delete(nhList15[i], NPTS);
	}*/
	free(nhList0);
	/*free(nhList1);
	free(nhList2);
	free(nhList3);
	free(nhList4);
	free(nhList5);
	free(nhList6);
	free(nhList7);
	free(nhList8);
	free(nhList9);
	free(nhList10);
	free(nhList11);
	free(nhList12);
	free(nhList13);
	free(nhList14);
	free(nhList15);*/
	free(coord);
	free(data);
}

/*
-------------------------------------------------------------------------------------------------------------------------------------------------------------
BEGINNING OF GROUP 2
IMPLEMENTATION OF THE KERNEL
*/

/*
Implementation of the kernel function.
It helps to compute the divergente, gradient and laplacien values. 
Input : table with all informations on every particles and their coordonates, object with each the neigbours of each particle stored as a list and the radius of the neighborhood.
Output : update the divergente, gradient and laplacien of every nodes.
*/
void kernel(GLfloat(*data)[14], GLfloat(*coord)[2], neighborhood* nh, double kh) {
	for (int i = 0; i < NPTS; i++) {
		double val_node_x = data[i][8];
		double val_node_y = data[i][9];
		int nNeigh = nh[i].nNeighbours;
		double val_div = 0;
		double val_grad_x = 0;
		double val_grad_y = 0;
		double val_lapl = 0;
		double dens2 = pow(DENSITY, 2);
		neighbours* List = nh[i].list;
		if (nNeigh > 0) {
			for (int j = 0; j < nNeigh; j++) {
				int index_node2 = List->index;
				double distance = List->distance;
				double d_x = coord[index_node2][0] - coord[i][0];
				double d_y = coord[index_node2][1] - coord[i][1];

				/*
				You can choose here the desired kernel function for your code.
				*/

				//double weight_x = grad_w_cubic(distance, kh, d_x);
				//double weight_y = grad_w_cubic(distance, kh, d_y);

				double weight_x = grad_w_lucy(distance, kh, d_x);
				double weight_y = grad_w_lucy(distance, kh, d_y);

				//double weight_x = grad_w_newquartic(distance, kh, d_x);
				//double weight_y = grad_w_newquartic(distance, kh, d_y);

				//double weight_x = grad_w_quinticspline(distance, kh, d_x);
				//double weight_y = grad_w_quinticspline(distance, kh, d_y);
				val_div += -MASS / DENSITY * ((data[index_node2][8] - val_node_x) * weight_x + (data[index_node2][9] - val_node_y) * weight_y);
				val_grad_x += -DENSITY * MASS * ((val_node_x / dens2) + (data[index_node2][8] / dens2)) * weight_x;
				val_grad_y += -DENSITY * MASS * ((val_node_x / dens2) + (data[index_node2][8] / dens2)) * weight_y;
				val_lapl += 2.0 * MASS / DENSITY * (val_node_x - data[index_node2][8]) * (d_x * weight_x + d_y * weight_y) / pow(distance, 2);
				List = List->next;
			}
		}
		// All the values of the divergente gradient and laplacien are stored in the data table
		data[i][10] = val_div;
		data[i][11] = val_grad_x;
		data[i][12] = val_grad_y;
		data[i][13] = val_lapl;
	}
	//Calcul of the error based on the already know function.
	for (int j = 0; j < NPTS; j++) {
		double exact = 3 * pow(data[j][0], 2);
		double error = exact - data[j][10];
	}

}


/*
Implementation of the kernel cubic function
Input : the distance between the particles, the radius of the neighborhoods and the distance between particle on x or y regarding the desired weight
Output : the coefficient that represents the importance of each particles on the other particles.
*/
double grad_w_cubic(double distance, double kh, double d)
{
	//printf("kh wietht = %f \n", kh);
	double h = kh / 2;
	double q = distance / h;
	double weight = 0;
	double alpha_d = 15 / (7 * M_PI * pow(h, 2));

	if (q > 0) {
		if (q <= 1) {
			weight = ((-2.0 * q + 1.5 * pow(q, 2)) * d) / (pow(h, 2) * q );
		}
		else {
			if (q > 2) {
				weight = 0;
				printf("too big distance ");
			}
			else {
				weight = (-0.5 * pow((2 - q), 2) * d) / (pow(h, 2) * q);
			}
		}
	}
	//printf("weight.... = %0.9f\n ", weight);
	return weight*alpha_d;

}

double grad_w_lucy(double distance, double kh, double d)
{
	double h = kh / 1;
	double q = distance / h;
	double grad_w = 0;
	double alpha_d = (5 / (M_PI * pow(h, 2)));
	if (q >= 0 && q <= 1)
	{
		grad_w = (-12.0 * q * alpha_d * pow((1 - q), 2)) * d /(pow(h,2) * q);
	}
	else
	{
		grad_w = 0.0;
	}
	return  grad_w;
}


double grad_w_newquartic(double distance, double kh, double d)
{
	double h = kh / 2;
	double q = distance / h;
	double grad_w = 0;
	double alpha_d = (15 / (7 * M_PI * pow(h, 2)));
	if (q >= 0 && q <= 2)
	{
		grad_w = (alpha_d * (-(9.0 / 4.0) * q + (19.0 / 8.0) * pow(q, 2) - (5.0 / 8.0) * pow(q, 3)) * d) / (pow(h, 2) * q);
	}
	else
	{
		grad_w = 0;
	}
	return  grad_w;
}

double grad_w_quinticspline(double distance, double kh, double d)
{
	double h = kh / 3;
	double q = distance / h;
	double x_x, y_y;
	double grad_w = 0;
	double alpha_d = (7 / (478 * M_PI * pow(h, 2)));
	double dq = d / (h * distance);
	if (q >= 0 && q <= 1)
	{
		grad_w = (alpha_d * (-5 * pow((3 - q), 4) + 30 * pow((2 - q), 4) - 75 * pow((1 - q), 4)) * d) / (pow(h, 2) * q);
	}
	else  if (q > 1 && q <= 2)
	{
		grad_w = (alpha_d * (-5 * pow((3 - q), 4) + 30 * pow((2 - q), 4)) * d) / (pow(h, 2) * q);
	}
	else  if (q > 2 && q <= 3)
	{
		grad_w = (alpha_d * (-5 * pow((3 - q), 4)) * d) / (pow(h, 2) * q);
	}
	else
	{
		grad_w = 0;
	}
	return  grad_w;
}