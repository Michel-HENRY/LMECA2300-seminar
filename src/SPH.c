#include "SPH.h"

Setup* Setup_new(int iter, double timestep,double kh,Verlet* verlet,Kernel kernel) {
	Setup* setup = (Setup*)malloc(sizeof(Setup));
	setup->itermax = iter;
	setup->timestep = timestep;
	setup->kh = kh;
	setup->verlet = verlet;
	setup->kernel = kernel;
	return setup;
}

void Setup_free(Setup* setup) {
	if(setup->verlet != NULL)
		free(setup->verlet);
	free(setup);
}

Residual* Residual_new(){
	Residual* residual = (Residual*)malloc(sizeof(Residual));
	residual->mass_eq = 0;
	residual->momentum_x_eq = 0;
	residual->momentum_y_eq = 0;
	return residual;
}

void free_Residuals(Residual** residuals, int N) {
	for (int i = 0;i < N;i++)
		free(residuals[i]);
	free(residuals);
}

void simulate(Grid* grid, Particle** particles, Particle_derivatives** particles_derivatives, Residual** residuals, int n_p, update_positions update_positions, Setup* setup, Animation* animation) {
	for (int iter = 0; iter < setup->itermax; iter++) {
		update_cells(grid, particles, n_p);
		update_neighborhoods(grid, particles, n_p, iter, setup->verlet);

		if (animation != NULL)
			display_particles(particles, animation, false);

		update_positions(grid, particles, particles_derivatives, residuals, n_p, setup);
	}
	update_cells(grid, particles, n_p);
	update_neighborhoods(grid, particles, n_p, 0, setup->verlet);
	if (animation != NULL)
		display_particles(particles, animation, true);
}



//move randomly each particles
void random_moves(Grid* grid, Particle** particles, Particle_derivatives** particles_derivatives, Residual** residuals, int n_p, Setup* setup) {
	double max_speed = 2;
	for (int i = 0; i < n_p; i++) {
		double angle = rand_interval(0, 2)*M_PI;
		double speed = rand_interval(0, max_speed);
		Particle *p = particles[i];
		p->v->x = speed * cos(angle);
		p->v->y = speed * sin(angle);
		p->pos->x += p->v->x * setup->timestep;
		p->pos->y += p->v->y * setup->timestep;

		double s = 2;
		//bouncing with the wall
		if (p->pos->x < grid->left)
			p->pos->x = grid->left + s;
		if (p->pos->x > grid->right)
			p->pos->x = grid->right - s;
		if (p->pos->y < grid->bottom)
			p->pos->y = grid->bottom + s;
		if (p->pos->y > grid->top)
			p->pos->y = grid->top - s;
	}
}

void update_positions_seminar_5(Grid* grid, Particle** particles, Particle_derivatives** particles_derivatives, Residual** residuals, int n_p, Setup* setup) {
	// Compute Cs
	for (int i = 0; i < n_p; i++)
		compute_Cs(particles[i], setup->kernel, setup->kh);

	// Compute derivatives and residuals
	for (int i = 0; i < n_p; i++) {
		particles_derivatives[i]->div_v = compute_div(particles[i], Particle_get_v, setup->kernel, setup->kh);
		particles_derivatives[i]->lapl_v->x = compute_lapl(particles[i], Particle_get_v_x, setup->kernel, setup->kh);
		particles_derivatives[i]->lapl_v->y = compute_lapl(particles[i], Particle_get_v_y, setup->kernel, setup->kh);
		compute_grad(particles[i], Particle_get_P, setup->kernel, setup->kh, particles_derivatives[i]->grad_P);
		compute_grad(particles[i], Particle_get_Cs, setup->kernel, setup->kh, particles_derivatives[i]->grad_Cs);
		particles_derivatives[i]->lapl_Cs = compute_lapl(particles[i], Particle_get_Cs, setup->kernel, setup->kh);
		assemble_residual_NS(particles[i], particles_derivatives[i], residuals[i]);
	}

	// Integrate (new values (i.e. density, velocities) at time t+1)
	for (int i = 0; i < n_p; i++)
		time_integrate(particles[i], residuals[i], setup->timestep);
}

void compute_Cs(Particle *particle, Kernel kernel, double kh) {
	particle->Cs = 0;
	Particle *pi = particle;
	ListNode *node = pi->neighborhood->head;
	while (node != NULL) {
		Particle *pj = node->v;
		particle->Cs += (pj->m / pj->rho) * eval_kernel(pi->pos, pj->pos, kh, kernel);
		node = node->next;
	}

}
// Assemble the residual of the (incompressible) Navier-Stokes equations based on the derivatives available
void assemble_residual_NS(Particle* particle, Particle_derivatives* particle_derivatives, Residual* residual) {
	double mu_i = particle->param->dynamic_viscosity;

	double rho_i = particle->rho;
	double div_vel_i = particle_derivatives->div_v;
	xy* grad_P = particle_derivatives->grad_P;
	xy* lapl_v = particle_derivatives->lapl_v;

	xy *n = particle_derivatives->grad_Cs; // surface normal inward
	double norm_n = norm(n); // norm of n
	double lapl_Cs = particle_derivatives->lapl_Cs;
	double kappa = - lapl_Cs / norm_n; // curvature

	double fs_x = 0; double fs_y = 0;
	// Apply surface tension only on particles in the vicinity the interface
	// Identification based on the norm of the normal
	if (detection == CSF && norm_n > particle->interface_threshold) {
	      particle->on_free_surface = true;
	      fs_x = - particle->param->sigma * lapl_Cs * n->x / norm_n;
	      fs_y = - particle->param->sigma * lapl_Cs * n->y / norm_n;
  // 	    printf("pos = (%lf, %lf), n = (%lf, %lf), fs = (%lf, %lf), lapl_Cs = %lf\n", particle->pos->x, particle->pos->y, n->x, n->y, fs->x, fs->y, lapl_Cs);
	}
	// Identification based on the divergence of the position vector
	else if(detection == DIVERGENCE && particle_derivatives->div_pos > particle->interface_threshold) {
	      particle->on_free_surface = true;
	      fs_x = - particle->param->sigma * lapl_Cs * n->x / norm_n;
	      fs_y = - particle->param->sigma * lapl_Cs * n->y / norm_n;
  // 	    printf("pos = (%lf, %lf), n = (%lf, %lf), fs = (%lf, %lf), lapl_Cs = %lf\n", particle->pos->x, particle->pos->y, n->x, n->y, fs->x, fs->y, lapl_Cs);
	}
	else
		particle->on_free_surface = false;
	
	// Exact values of normal and curvature for a circle centered in (0,0)
	/*xy* n_exact = xy_new(particle->pos->x, particle->pos->y);
	double norm_n_exact = norm(n_exact);
	double circle_radius = 1e-3;
	double epsilon = 1e-5;
	double kappa_exact = 1.0 / circle_radius;

	// To print quantities on the surface of the circle
	if (pow(particle->pos->x,2) + pow(particle->pos->y,2) <= pow(circle_radius+epsilon,2) &&  pow(particle->pos->x,2) + pow(particle->pos->y,2) >= pow(circle_radius-epsilon,2)) {
	  printf("pos = (%lf, %lf), n_exact = (%lf, %lf), n = (%lf, %lf), ||n|| = %lf, fs = (%lf, %lf), kappa_exact = %2.3f, kappa = %2.6f \n", particle->pos->x, particle->pos->y,-n_exact->x / norm_n_exact, -n_exact->y / norm_n_exact, n->x / norm_n, n->y / norm_n, norm_n, fs->x, fs->y, kappa_exact, kappa);
	}

	// To print quantities on the surface of the square
// 	double x_pos = particle->pos->x, y_pos = particle->pos->y;
// 	if (x_pos == 50.0 || x_pos == -50.0 || y_pos == 50.0 || y_pos == -50.0) {
// 	  printf("pos = (%lf, %lf), n = (%lf, %lf), ||n|| = %lf, fs = (%lf, %lf), kappa = %2.6f \n",   particle->pos->x, particle->pos->y, n->x / norm_n, n->y / norm_n, norm_n, fs->x, fs->y, kappa);
// 	}

	residual->mass_eq = -rho_i * div_vel_i;
	residual->momentum_x_eq = (-1.0/rho_i) * grad_P->x + (mu_i/rho_i) * lapl_v->x + fs_x;
	residual->momentum_y_eq = (-1.0/rho_i) * grad_P->y + (mu_i/rho_i) * lapl_v->y + fs_y;

}

// Time integrate the Navier-Stokes equations based on the residual already assembled
void time_integrate(Particle* particle, Residual* residual, double delta_t) {

	// Update position
	particle->pos->x += delta_t * particle->v->x;
	particle->pos->y += delta_t * particle->v->y;


	// Update density and velocity
	particle->rho += delta_t * residual->mass_eq;
	particle->v->x += delta_t * residual->momentum_x_eq;
	particle->v->y += delta_t * residual->momentum_y_eq;
	
	// Update pressure with Tait's equation of state
	double B = squared(particle->param->sound_speed) * particle->param->rho_0 / particle->param->gamma;
	particle->P = B * (pow(particle->rho / particle->param->rho_0, particle->param->gamma) - 1);

}


void update_positions_ellipse(Grid* grid, Particle** particles, Particle_derivatives** particles_derivatives, Residual** residuals, int n_p, Setup* setup) {
	// Compute Cs
	for (int i = 0; i < n_p; i++)
		compute_Cs(particles[i], setup->kernel, setup->kh);

	// Compute derivatives and residuals
	for (int i = 0; i < n_p; i++) {
		particles_derivatives[i]->div_v = compute_div(particles[i], Particle_get_v, setup->kernel, setup->kh);
		particles_derivatives[i]->lapl_v->x = compute_lapl(particles[i], Particle_get_v_x, setup->kernel, setup->kh);
		particles_derivatives[i]->lapl_v->y = compute_lapl(particles[i], Particle_get_v_y, setup->kernel, setup->kh);
		compute_grad(particles[i], Particle_get_P, setup->kernel, setup->kh, particles_derivatives[i]->grad_P);
		compute_grad(particles[i], Particle_get_Cs, setup->kernel, setup->kh, particles_derivatives[i]->grad_Cs);
		particles_derivatives[i]->lapl_Cs = compute_lapl(particles[i], Particle_get_Cs, setup->kernel, setup->kh);
		assemble_residual_NS(particles[i], particles_derivatives[i], residuals[i]);
	}
	int index_x_max, index_x_min, index_y_max, index_y_min;
	double pos_x_max = -INFINITY, pos_x_min = INFINITY, pos_y_max = -INFINITY, pos_y_min = INFINITY;
	// Integrate (new values (i.e. density, velocities) at time t+1)
	for (int i = 0; i < n_p; i++) {
		time_integrate(particles[i], residuals[i], setup->timestep);
		if (particles[i]->pos->x > pos_x_max) pos_x_max = particles[i]->pos->x, index_x_max = i;
		if (particles[i]->pos->x < pos_x_min) pos_x_min = particles[i]->pos->x, index_x_min = i;
		if (particles[i]->pos->y > pos_y_max) pos_y_max = particles[i]->pos->y, index_y_max = i;
		if (particles[i]->pos->y < pos_y_min) pos_y_min = particles[i]->pos->y, index_y_min = i;
	}
	double a_ellipse = particles[index_x_max]->pos->x - particles[index_x_min]->pos->x;
	double b_ellipse = particles[index_y_max]->pos->y - particles[index_y_min]->pos->y;
	printf("a = %lf, b = %lf\n", a_ellipse, b_ellipse);
}
