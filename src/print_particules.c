#include "print_particules.h"

// Fills data with particle data
void fillData(GLfloat(*data)[8], Particle** particles, int N) {
	float rmax = 100.0*sqrtf(2.0f);
	for (int i = 0; i < N; i++) {
		Particle* p = particles[i];
		data[i][0] = p->pos->x;
		data[i][1] = p->pos->y;
		data[i][2] = p->v->x;
		data[i][3] = p->v->y;
		colormap_cell(p, &data[i][4]); // fill color
		//colormap_uni_color(&data[i][4]);
		data[i][7] = 0.8f; // transparency
	}
}

bov_points_t * load_Grid(Grid* grid)
{
	int nLines = (grid->nCellx + 1) + (grid->nCelly + 1);
	printf("\n%d\n", nLines);
	GLfloat(*data)[2] = malloc(sizeof(data[0]) * 2 * nLines);
	for (int i = 0;i < (grid->nCellx + 1);i++)
	{
		data[2 * i][0] = grid->left + i * grid->h;
		data[2 * i][1] = grid->bottom;
		data[2 * i + 1][0] = grid->left + i * grid->h;
		data[2 * i + 1][1] = grid->top;
	}
	int shift = 2 * (grid->nCellx + 1);
	for (int j = 0;j < (grid->nCelly + 1);j++)
	{
		data[shift + 2 * j][0] = grid->left;
		data[shift + 2 * j][1] = grid->bottom + j * grid->h;
		data[shift + 2 * j + 1][0] = grid->right;
		data[shift + 2 * j + 1][1] = grid->bottom + j * grid->h;
	}
	bov_points_t *points = bov_points_new(data, 2 * nLines, GL_STATIC_DRAW);
	bov_points_set_width(points, 0.005);
	bov_points_scale(points, (GLfloat[2]) { 0.008, 0.008 });
	free(data);
	return points;
}

Animation* Animation_new(int N, double timeout,Grid* grid)
{
	Animation* animation = (Animation*)malloc(sizeof(Animation));
	animation->window = bov_window_new(1024, 780, "ANM Project: SPH");
	bov_window_set_color(animation->window, (GLfloat[]) { 0.9f, 0.85f, 0.8f, 0.0f });
	bov_window_enable_help(animation->window);
	animation->N = N;
	animation->timeout = timeout;

	////set-up particles////
	GLfloat(*data)[8] = malloc(sizeof(data[0])*N);
	bov_points_t *particles = bov_particles_new(data, N, GL_STATIC_DRAW);
	free(data);
	// setting particles appearance
	bov_points_set_width(particles, 0.01);
	bov_points_set_outline_width(particles, 0.0025);
	bov_points_scale(particles, (GLfloat[2]) { 0.008, 0.008 });
	animation->particles = particles;
	////set-up grid////
	if (grid != NULL)
		animation->grid = load_Grid(grid);
	else
		animation->grid = NULL;
	return animation;
}
void Animation_free(Animation* animation)
{
	bov_points_delete(animation->particles);
	if(animation->grid != NULL)
		bov_points_delete(animation->grid);
	bov_window_delete(animation->window);
	free(animation);
}

void display_particles(Particle** particles, Animation* animation,bool end)
{
	int N = animation->N;
	GLfloat(*data)[8] = malloc(sizeof(data[0])*N);
	fillData(data, particles, N);
	colours_neighbors(data, particles, N / 2);
	animation->particles = bov_particles_update(animation->particles,data,N);
	free(data);

	bov_window_t* window = animation->window;
	double tbegin = bov_window_get_time(window);
	if (!end){
		while (bov_window_get_time(window) - tbegin < animation->timeout) {
			if(animation->grid != NULL)
				bov_lines_draw(window,animation->grid,0, BOV_TILL_END);
			bov_particles_draw(window, animation->particles, 0, BOV_TILL_END);
			bov_window_update(window);
		}
	}
	else {
		// we want to keep the window open with everything displayed
		while (!bov_window_should_close(window)) {
			if (animation->grid != NULL)
				bov_lines_draw(window, animation->grid, 0, BOV_TILL_END);
			bov_particles_draw(window, animation->particles, 0, BOV_TILL_END);
			bov_window_update_and_wait_events(window);
		}
	}
}

// Fills color with the color to use for particle p
void colormap_cell(Particle* p, float color[3]) {
	if (p->cell == NULL) {
		color[0] = 0;color[1] = 0;color[2] = 0;
	}
	else {
		if (p->cell->i % 2 == 0) {
			color[0] = 20;
			if (p->cell->j % 2 == 0)
				color[1] = 20;
			else
				color[1] = 0;
		}
		else {
			color[0] = 0;
			if (p->cell->j % 2 == 0)
				color[1] = 20;
			else
				color[1] = 0;
		}
		color[2] = 0;
	}
}

void colormap_uni_color(float color[3])
{
	color[0] = 20;color[1] = 0;color[2] = 0;

}

void colours_neighbors(GLfloat(*data)[8], Particle** particles, int index)
{
	Particle* p = particles[index];
	ListNode *node = p->neighborhood->head;
	data[index][4] = 20;data[index][5] = 20;data[index][6] = 20;
	while (node != NULL) {
		Particle* q = (Particle*)node->v;
		int i = q->index;
		data[i][4] = 0;data[i][5] = 20;data[i][6] = 20;
		node = node->next;
	}
}