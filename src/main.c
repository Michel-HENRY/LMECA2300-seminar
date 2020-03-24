#include "print_particules.h"
#include "particle.h"
#include "SPH.h"
#include "derivatives.h"
#include <math.h>
//#include "crtdbg.h" // for memory leak detection; comment if you're on Linux

void script_csf();
void script_csf_circle();
void script_circle_to_ellipse();
void script_csf_circle_paper();
void script1();
void script2();

int main() {
	// _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // comment if on Linux
	script_csf();
// 	script_csf_circle();
// 	script_circle_to_ellipse();
// 	script_csf_circle_paper();

	return EXIT_SUCCESS;
}

// Evolution of a 2D square submitted to the tension surface force
void script_csf() {

	// Parameters of the problem
	double L = 2; // size of the domain: [-L,L] x [-L,L]
	double l = 1; // size of the square: [-l,l] x [-l,l]
	double dt = 0.01; // physical time step
	double T = 100; // duration of simulation
	// double T = dt; // duration of simulation

	// Physical parameters
	double rho_0 = 998.0; // initial (physical) density of water at 20°C (in kg/m^3)
	double mu = 1.0016e-3; // dynamic viscosity of water at 20°C (in N.s/m^2)
	double gamma = 7; // typical value for liquid (dimensionless)
	double c_0 = 1.0;//1481; // sound speed in water at 20°C (in m/s)
	double sigma = 72.86e-3; // surface tension of water-air interface at 20°C (in N/m)

	// SPH parameters
	int n_per_dim = 51; // number of particles per dimension
	double kh = sqrt(21)*2*l/n_per_dim; // kernel width to ensure 21 particles in the neighborhood
	int n_iter = (int)(T/dt); // number of iterations to perform
	Kernel kernel = Cubic; // kernel choice
	double interface_threshold = 1.5;//7; // If ||n_i|| > threshold => particle i belongs to interface (first detection approach)
	Verlet *verlet = NULL; // don't use Verlet (for now)
	double XSPH_epsilon = 0.5;
	Free_surface_detection surface_detection = DIVERGENCE;

	printf("n_iter = %d\n", n_iter);

	// Animation parameter
	double T_anim = 10; // duration of animation
	double dt_anim = T_anim / n_iter; // time step of animation

	// Initialize particles on a square
	int n_p = squared(n_per_dim); // total number of particles
	double h = 2*l / (n_per_dim-1); // step between neighboring particles
	double m = rho_0 * h*h;
	Particle** particles = (Particle**) malloc(n_p*sizeof(Particle*));
	Particle_derivatives** particles_derivatives = malloc(n_p * sizeof(Particle_derivatives*));
	Residual** residuals = malloc(n_p * sizeof(Residual*));
	for(int i = 0; i < n_per_dim; i++) {
		for(int j = 0; j < n_per_dim; j++) {
			int index = i*n_per_dim + j;
			xy *pos = xy_new(-l+i*h, -l+j*h);
			xy *v = xy_new(0, 0); // initial velocity = 0
			particles[index] = Particle_new(index, m, pos, v, interface_threshold, XSPH_epsilon, rho_0, mu, c_0, gamma, sigma);
			particles_derivatives[index] = Particle_derivatives_new(index);
			residuals[index] = residual_new();
		}
	}

	// Setup grid
	Grid *grid = Grid_new(-L, L, -L, L, kh);
	// Setup animation
	Animation *animation = Animation_new(n_p, dt_anim, grid);
	// Setup setup (lol)
	Setup *setup = Setup_new(n_iter, dt, verlet, kernel);

	//Residual* residual = residual_new(); // small struct for storing residuals

	// Start simulation
	for(int iter = 0; iter < setup->itermax; iter++) {
		printf("----------------------------------------\nITERATION %d\n", iter);
		printf("t = %lf\n", iter * dt);

		// Update cells and particle neighborhoods
		update_cells(grid, particles, n_p);
		update_neighborhoods(grid, particles, n_p, iter, setup->verlet);

		// Update animation
		if (animation != NULL)
			display_particles(particles, animation, false);

		// Compute Cs and the XSPH correction on the velocity
		for(int i = 0; i < n_p; i++) {
			compute_Cs(particles[i], kernel, grid->h);
			if (XSPH_epsilon != 0.0) compute_XSPH_correction(particles[i], kernel, grid->h);
			if (surface_detection == DIVERGENCE) particles_derivatives[i]->div_pos = compute_div(particles[i], Particle_get_pos, kernel, grid->h);
		}

		// Compute derivatives and residuals
		for(int i = 0; i < n_p; i++) {
			particles_derivatives[i]->div_v = compute_div(particles[i], Particle_get_v, kernel, grid->h);
			particles_derivatives[i]->lapl_v->x = compute_lapl(particles[i], Particle_get_v_x, kernel, grid->h);
			particles_derivatives[i]->lapl_v->y = compute_lapl(particles[i], Particle_get_v_y, kernel, grid->h);
			particles_derivatives[i]->grad_P = compute_grad(particles[i], Particle_get_P, kernel, grid->h);
			particles_derivatives[i]->grad_Cs = compute_grad(particles[i], Particle_get_Cs, kernel, grid->h);
			particles_derivatives[i]->lapl_Cs = compute_lapl(particles[i], Particle_get_Cs, kernel, grid->h);
			assemble_residual_NS(particles[i], particles_derivatives[i], residuals[i], surface_detection);
		}

		// Integrate
		for(int i = 0; i < n_p; i++) {
			// New values (i.e. density, velocities, pressure and positions) at time t+1
			time_integrate(particles[i], residuals[i], dt);
		}

	} // end of the simulation
	
	update_cells(grid, particles, n_p);
	update_neighborhoods(grid, particles, n_p, 0, setup->verlet);
	if (animation != NULL)
		display_particles(particles, animation, true);

	// Free stuff
	free_particles(particles, n_p);
	Grid_free(grid);
	Setup_free(setup);
	Animation_free(animation);

}


// Evolution of a 2D circle submitted to the tension surface force
void script_csf_circle() {

	// Computational Parameters
	double L = 100; // size of the domain: [-L,L] x [-L,L]
	double l = 50; // size of the square: [-l,l] x [-l,l]
	int n_per_dim = 51; // number of particles per dimension
	double kh = 0.1*L; // kernel width
	double dt = 1.0; // physical time step
	double dt_anim = 0.001; // time step for animation
	int n_iter = 2000; // number of iterations to perform
	Verlet *verlet = NULL; // don't use Verlet (for now)
	Kernel kernel = Cubic; // kernel choice
	double interface_threshold = 0.08; // If ||n_i|| > threshold => particle i belongs to interface (first detection approach)
	double XSPH_epsilon = 0.5;
	Free_surface_detection surface_detection = CSF;
	
	// Physical parameters
	double rho_0 = 998.0; // initial (physical) density of water at 20°C (in kg/m^3)
	double mu = 1.0016e-3; // dynamic viscosity of water at 20°C (in N.s/m^2)
	double gamma = 7.0; // typical value for liquid (dimensionless)
	double c_0 = 1.0;//1481; // sound speed in water at 20°C (in m/s)
	double sigma = 72.86e-3; // surface tension of water-air interface at 20°C (in N/m)

	// Initialize particles on a square
	int n_p = squared(n_per_dim); // total number of particles
	double h = 2*l / (n_per_dim-1); // step between neighboring particles
	double m = rho_0 * M_PI * l * l / n_p;
	double deltat_max = 0.25 * h / c_0;
// 	printf(">>>>>>>>>>>>>> deltat_max = %2.6f \n", deltat_max);
	Particle** particles = (Particle**) malloc(n_p*sizeof(Particle*));
	Particle_derivatives** particles_derivatives = malloc(n_p * sizeof(Particle_derivatives*));
	Residual** residuals = malloc(n_p * sizeof(Residual*));
	int k = 0;
	int alpha = 2;
	int nb = (int)(alpha*n_per_dim);
	for(int i = 0; i < n_per_dim; i++) {
		for(int j = 0; j < n_per_dim; j++) {
			int index = i*n_per_dim + j;
			xy *pos = generate_circle(k, n_p, nb, l);
// 			xy *pos_circle = map_to_circle(pos);
// 			pos->x = pos_circle->x;
// 			pos->y = pos_circle->y;
			xy *v = xy_new(0, 0); // initial velocity = 0
			//xy *v = xy_new(pos->x/10000, pos->y/10000); // initial velocity = 0
			particles[index] = Particle_new(index, m, pos, v, interface_threshold, XSPH_epsilon, rho_0, mu, c_0, gamma, sigma);
			particles_derivatives[index] = Particle_derivatives_new(index);
			k++;
		}
	}


	// Setup grid
	Grid *grid = Grid_new(-L, L, -L, L, kh);
	// Setup animation
	Animation *animation = Animation_new(n_p, dt_anim, grid);
	//Animation *animation = NULL;
	// Setup setup (lol)
	Setup *setup = Setup_new(n_iter, dt, verlet, kernel);



	Residual* residual = residual_new();
	// Start simulation
	for(int iter = 0; iter < setup->itermax; iter++) {
		printf("----------------------------------------\nITERATION %d\n", iter);
		update_cells(grid, particles, n_p);
		update_neighborhoods(grid, particles, n_p, iter, setup->verlet);
		if (animation != NULL)
			display_particles(particles, animation, false);

		// Compute Cs and the XSPH correction on the velocity
		for(int i = 0; i < n_p; i++) {
			compute_Cs(particles[i], kernel, grid->h);
			if (XSPH_epsilon != 0.0) compute_XSPH_correction(particles[i], kernel, grid->h);
			if (surface_detection == DIVERGENCE) particles_derivatives[i]->div_pos = compute_div(particles[i], Particle_get_pos, kernel, grid->h);
		}

		// Compute derivatives
		for(int i = 0; i < n_p; i++) {
			//printf("%lf %lf\n", particles[i]->v->x, particles[i]->v->y);
			// Compute all useful derivatives
			particles_derivatives[i]->div_v = compute_div(particles[i], Particle_get_v, kernel, grid->h);
			particles_derivatives[i]->lapl_v->x = compute_lapl(particles[i], Particle_get_v_x, kernel, grid->h);
			particles_derivatives[i]->lapl_v->y = compute_lapl(particles[i], Particle_get_v_y, kernel, grid->h);
			particles_derivatives[i]->grad_P = compute_grad(particles[i], Particle_get_P, kernel, grid->h);
			particles_derivatives[i]->grad_Cs = compute_grad(particles[i], Particle_get_Cs, kernel, grid->h);
			particles_derivatives[i]->lapl_Cs = compute_lapl(particles[i], Particle_get_Cs, kernel, grid->h);

		}

		// Compute residuals and integrate!
		for(int i = 0; i < n_p; i++) {
			// Residual (i.e. RHS term of the Navier-Stokes eqs) at time t (explicit time integration)
			assemble_residual_NS(particles[i], particles_derivatives[i], residual, surface_detection);
			// New values (i.e. density, velocities) at time t+1
			time_integrate(particles[i], residual, dt);
			// TODO: update the positions based on the velocities at time t+1, and update the pressure and colour function based on the density at time t+1
			// updatePositions(particles);
			// updatePressure(particles);
			// updateColourFunction(particles);
			//compute_Cs(particles[i], kernel, grid->h);
		}

	}
	update_cells(grid, particles, n_p);
	update_neighborhoods(grid, particles, n_p, 0, setup->verlet);
	if (animation != NULL)
		display_particles(particles, animation, true);

	// Free stuff
	free_particles(particles, n_p);
	Grid_free(grid);
	Setup_free(setup);
	Animation_free(animation);

}

// Evolution of a 2D circle submited to initial velocity field.
// Test case from "Simulating Free Surface Flows with SPH", Monaghan (1994)
void script_circle_to_ellipse() {

	// Parameters of the problem
	double l = 1.0; // radius of the circle
	double L = 2.0*l; // size of the domain: [-L,L] x [-L,L]
	double dt = 1.0e-5; // physical time step
	double T = 0.0076; // duration of simulation
	
	// Physical parameters
	double rho_0 = 1000.0; // initial (physical) density of water at 20°C (in kg/m^3)
	double mu = 1.0016e-3; // dynamic viscosity of water at 20°C (in N.s/m^2)
	double gamma = 7.0; // typical value for liquid (dimensionless)
	double c_0 = 1400.0;//1481; // sound speed in water at 20°C (in m/s)
	double sigma = 0.0; // surface tension of water-air interface at 20°C (in N/m)
	
	// SPH parameters
	Verlet *verlet = NULL; // don't use Verlet (for now)
	Kernel kernel = Cubic; // kernel choice
	double interface_threshold = 1000.0; // If ||n_i|| > threshold => particle i belongs to interface (first detection approach)
	double XSPH_epsilon = 0.5;
	Free_surface_detection surface_detection = CSF;
	int N_c = 30; // number of circonferences on which points are placed
	int N_p = 6; // number of points on the first circonference (doubled for every circonference)
	int N_tot = 1; // total number of points
	for (int i=1; i < N_c; i++) {
	  N_tot += i*N_p;
	}
	printf("N_tot = %d \n", N_tot);
	int n_iter = (int)(T/dt); // number of iterations to perform
	double kh = 0.2*l;// 0.4*l is ideal to reach t = 0.0076; // kernel width
// 	printf("kh_old = %lf \n", kh);
// 	double target = 21.0 / N_tot;
// 	double tolerance = 0.0000000001;
// 	double kh_min = 1.0;
// 	double kh_max = sqrt(2.0);
// 	while (fabs(kh_max - kh_min) >= tolerance) {
// 		kh_max = kh_min;
// 		kh_min = sqrt((target - sin(acos(1.0 / kh_min)) * kh_min) / ((M_PI / 4 - acos(1.0 / kh_min))));
// 		printf("kh_new = %lf \n", kh_min);
// 	}
// 	kh = kh_min;
// 	printf("kh_new = %lf \n", kh);
	
	// Animation parameter
	double T_anim = 10; // duration of animation
	double dt_anim = T_anim / n_iter; // time step of animation

	// Initialize particles in a circle
	double m = rho_0 * M_PI * l * l / N_tot; // mass of each particle

	Particle** particles = (Particle**) malloc(N_tot*sizeof(Particle*));
	Particle_derivatives** particles_derivatives = malloc(N_tot * sizeof(Particle_derivatives*));
	Residual** residuals = malloc(N_tot * sizeof(Residual*));
	
	// parameters defining the circle
	double b, delta_s, k, theta; 
	delta_s = l / ((double)N_c - 1.0);
	theta = (2*M_PI) / ((double)N_p);
	
	int index = 0;
	for(int i = 0; i < N_c; i++) {
	  b = i;
	  if (b==0) {
	    xy *pos = xy_new(0.0, 0.0);
	    xy *v = xy_new(0.0, 0.0);
	    particles[index] = Particle_new(index, m, pos, v, interface_threshold, XSPH_epsilon, rho_0, mu, c_0, gamma, sigma);
	    particles_derivatives[index] = Particle_derivatives_new(index);
	    residuals[index] = residual_new();
	    index++;
	  }
	  else {
	    for (int j = 0; j < i*N_p; j++) {
		k = (double) j/b;
		xy *pos = xy_new(b*delta_s*cos(k*theta), b*delta_s*sin(k*theta));
		xy *v = xy_new(-100.0*pos->x, 100.0*pos->y);
		particles[index] = Particle_new(index, m, pos, v, interface_threshold, XSPH_epsilon, rho_0, mu, c_0, gamma, sigma);
		particles_derivatives[index] = Particle_derivatives_new(index);
		residuals[index] = residual_new();
		index++;
	    }
	  }
	}

	// Setup grid
	Grid *grid = Grid_new(-L, L, -L, L, kh);
	// Setup animation
	Animation *animation = Animation_new(N_tot, dt_anim, grid);
	// Setup setup (lol)
	Setup *setup = Setup_new(n_iter, dt, verlet, kernel);


	int index_x_max, index_x_min, index_y_max, index_y_min; // index to compute major axois of ellipse
	// Start simulation
	double time = 0.0;
	for(int iter = 0; iter < setup->itermax; iter++) {
		printf("----------------------------------------\nITERATION %d\n", iter);
		printf("t = %lf\n", iter * dt);
		
		// Update cells and particle neighborhoods
		update_cells(grid, particles, N_tot);
		update_neighborhoods(grid, particles, N_tot, iter, setup->verlet);
		
		// Update animation
		if (animation != NULL)
			display_particles(particles, animation, false);

		// Compute Cs and the XSPH correction on the velocity
		for(int i = 0; i < N_tot; i++) {
			compute_Cs(particles[i], kernel, grid->h);
			if (XSPH_epsilon != 0.0) compute_XSPH_correction(particles[i], kernel, grid->h);
			if (surface_detection == DIVERGENCE) particles_derivatives[i]->div_pos = compute_div(particles[i], Particle_get_pos, kernel, grid->h);
		}

		// Compute derivatives and residuals
		for(int i = 0; i < N_tot; i++) {
			particles_derivatives[i]->div_v = compute_div(particles[i], Particle_get_v, kernel, grid->h);
			particles_derivatives[i]->lapl_v->x = compute_lapl(particles[i], Particle_get_v_x, kernel, grid->h);
			particles_derivatives[i]->lapl_v->y = compute_lapl(particles[i], Particle_get_v_y, kernel, grid->h);
			particles_derivatives[i]->grad_P = compute_grad(particles[i], Particle_get_P, kernel, grid->h);
			particles_derivatives[i]->grad_Cs = compute_grad(particles[i], Particle_get_Cs, kernel, grid->h);
			particles_derivatives[i]->lapl_Cs = compute_lapl(particles[i], Particle_get_Cs, kernel, grid->h);
			assemble_residual_NS(particles[i], particles_derivatives[i], residuals[i], surface_detection);
		}

		// Integrate
		double pos_x_max = 0.0, pos_x_min = -10.0, pos_y_max = 0.0, pos_y_min = -10.0;
		for(int i = 0; i < N_tot; i++) {
			// New values (i.e. density, velocities, pressure and positions) at time t+1
			time_integrate(particles[i], residuals[i], dt);
			// Compute lengths of major axis of the ellipse
			if (particles[i]->pos->x > pos_x_max) pos_x_max = particles[i]->pos->x, index_x_max = i;
			if (particles[i]->pos->x < pos_x_min) pos_x_min = particles[i]->pos->x, index_x_min = i;
			if (particles[i]->pos->y > pos_y_max) pos_y_max = particles[i]->pos->y, index_y_max = i;
			if (particles[i]->pos->y < pos_y_min) pos_y_min = particles[i]->pos->y, index_y_min = i;

		}
		time += dt;
		// Compute lengths of major axis of the ellipse
		double a_ellipse = particles[index_x_max]->pos->x - particles[index_x_min]->pos->x;
		double b_ellipse = particles[index_y_max]->pos->y - particles[index_y_min]->pos->y;
		printf("a = %lf, b = %lf @ t = %lf \n", a_ellipse, b_ellipse, time);

	} // end of the simulation
	
	update_cells(grid, particles, N_tot);
	update_neighborhoods(grid, particles, N_tot, 0, setup->verlet);
	if (animation != NULL)
		display_particles(particles, animation, true);

	// Free stuff
	free_particles(particles, N_tot);
	Grid_free(grid);
	Setup_free(setup);
	Animation_free(animation);

}

// Evolution of a 2D circle with surface tension forces: 
// Test case based on "Modelling surface tension of two-dimensional droplet using Smoothed Particle Hydrodynamics", Nowoghomwenma, 2018

void script_csf_circle_paper() {

	// Parameters of the problem
	double l = 1e-3; // radius of the circle
	double L = 2.0*l; // size of the domain: [-L,L] x [-L,L]
	double dt = 0.00001; // physical time step
	double T = 0.05; // duration of simulation
	
	// Physical parameters
	double rho_0 = 1000.0; // initial (physical) density of water at 20°C (in kg/m^3)
	double mu = 0.01; // dynamic viscosity of water at 20°C (in N.s/m^2)
	double gamma = 7.0; // typical value for liquid (dimensionless)
	double c_0 = 5.0;//5.0;//1481; // sound speed in water at 20°C (in m/s)
	double sigma = 1.0; // surface tension of water-air interface at 20°C (in N/m)

	// SPH parameters
	Verlet *verlet = NULL; // don't use Verlet (for now)
	Kernel kernel = Cubic; // kernel choice
	double interface_threshold = 3360.0; // If ||n_i|| > threshold => particle i belongs to interface (first detection approach)
	double XSPH_epsilon = 0.5;
	Free_surface_detection surface_detection = CSF;
	int N_c = 21; // number of circonferences on which points are placed
	int N_p = 6; // number of points on the first circonference (doubled for every circonference)
	int N_tot = 1; // total number of points
	for (int i=1; i < N_c; i++) {
	  N_tot += i*N_p;
	}
	printf("N_tot = %d \n", N_tot);
	int n_iter = (int)(T/dt); // number of iterations to perform
	double kh = 0.5*l; // kernel width

	// Animation parameter
	double T_anim = 10; // duration of animation
	double dt_anim = T_anim / n_iter; // time step of animation
	
	
	// Initialize particles in a circle
	double m = rho_0 * M_PI * l * l / N_tot; // mass of each particle

	Particle** particles = (Particle**) malloc(N_tot*sizeof(Particle*));
	Particle_derivatives** particles_derivatives = malloc(N_tot * sizeof(Particle_derivatives*));
	Residual** residuals = malloc(N_tot * sizeof(Residual*));
	
	// parameters defining the circle
	double b, delta_s, k, theta; 
	delta_s = l / ((double)N_c - 1.0);
	theta = (2*M_PI) / ((double)N_p);
	
	int index = 0;
	for(int i = 0; i < N_c; i++) {
	  b = i;
	  if (b==0) {
	    xy *pos = xy_new(0.0, 0.0);
	    xy *v = xy_new(0.0, 0.0);
	    particles[index] = Particle_new(index, m, pos, v, interface_threshold, XSPH_epsilon, rho_0, mu, c_0, gamma, sigma);
	    particles_derivatives[index] = Particle_derivatives_new(index);
	    residuals[index] = residual_new();
	    index++;
	  }
	  else {
	    for (int j = 0; j < i*N_p; j++) {
		k = (double) j/b;
		xy *pos = xy_new(b*delta_s*cos(k*theta), b*delta_s*sin(k*theta));
		xy *v = xy_new(0.0, 0.0);
		particles[index] = Particle_new(index, m, pos, v, interface_threshold, XSPH_epsilon, rho_0, mu, c_0, gamma, sigma);
		particles_derivatives[index] = Particle_derivatives_new(index);
		residuals[index] = residual_new();
		if (b == N_c - 1) particles[index]->on_free_surface = true; // WARNING: to be removed
		index++;
	    }
	  }
	}
	
	// Setup grid
	Grid *grid = Grid_new(-L, L, -L, L, kh);
	// Setup animation
	Animation *animation = Animation_new(N_tot, dt_anim, grid);
	// Setup setup (lol)
	Setup *setup = Setup_new(n_iter, dt, verlet, kernel);


	// Start simulation
	int index_x_max, index_x_min, index_y_max, index_y_min; // index to compute major axois of ellipse
	for(int iter = 0; iter < setup->itermax; iter++) {
		printf("----------------------------------------\nITERATION %d\n", iter);
		printf("t = %lf\n", iter * dt);
		
		// Update cells and particle neighborhoods
		update_cells(grid, particles, N_tot);
		update_neighborhoods(grid, particles, N_tot, iter, setup->verlet);
		
		// Update animation
		if (animation != NULL)
			display_particles(particles, animation, false);

		// Compute Cs and the XSPH correction on the velocity
		double pos_x_max = 0.0, pos_x_min = -10.0, pos_y_max = 0.0, pos_y_min = -10.0;
		for(int i = 0; i < N_tot; i++) {
			compute_Cs(particles[i], kernel, grid->h);
			if (XSPH_epsilon != 0.0) compute_XSPH_correction(particles[i], kernel, grid->h);
			if (surface_detection == DIVERGENCE) particles_derivatives[i]->div_pos = compute_div(particles[i], Particle_get_pos, kernel, grid->h);
			// Compute radius of circle
			if (particles[i]->pos->x > pos_x_max) pos_x_max = particles[i]->pos->x, index_x_max = i;
			if (particles[i]->pos->x < pos_x_min) pos_x_min = particles[i]->pos->x, index_x_min = i;
			if (particles[i]->pos->y > pos_y_max) pos_y_max = particles[i]->pos->y, index_y_max = i;
			if (particles[i]->pos->y < pos_y_min) pos_y_min = particles[i]->pos->y, index_y_min = i;
		}
		double radius_circle = particles[index_x_max]->pos->x - particles[index_x_min]->pos->x;

		// Compute derivatives and residuals
		for(int i = 0; i < N_tot; i++) {
			particles_derivatives[i]->div_v = compute_div(particles[i], Particle_get_v, kernel, grid->h);
			particles_derivatives[i]->lapl_v->x = compute_lapl(particles[i], Particle_get_v_x, kernel, grid->h);
			particles_derivatives[i]->lapl_v->y = compute_lapl(particles[i], Particle_get_v_y, kernel, grid->h);
			particles_derivatives[i]->grad_P = compute_grad(particles[i], Particle_get_P, kernel, grid->h);
			particles_derivatives[i]->grad_Cs = compute_grad(particles[i], Particle_get_Cs, kernel, grid->h);
			particles_derivatives[i]->lapl_Cs = compute_lapl(particles[i], Particle_get_Cs, kernel, grid->h);
			assemble_residual_NS(particles[i], particles_derivatives[i], residuals[i], surface_detection);
// 			assemble_residual_NS_test(particles[i], particles_derivatives[i], residuals[i], radius_circle);
		}

		// Integrate
		for(int i = 0; i < N_tot; i++) {
			// New values (i.e. density, velocities, pressure and positions) at time t+1
			time_integrate(particles[i], residuals[i], dt);
		}

	} // end of the simulation
	
	update_cells(grid, particles, N_tot);
	update_neighborhoods(grid, particles, N_tot, 0, setup->verlet);
	if (animation != NULL)
		display_particles(particles, animation, true);

	// Free stuff
	free_particles(particles, N_tot);
	Grid_free(grid);
	Setup_free(setup);
	Animation_free(animation);

}

// Without Verlet
void script1() {
	int N = 2000;
	Particle** particles = build_particles(N, 100);
	double kh = 30;
	double timestep = 1;
	Verlet* verlet = NULL;
	Grid* grid = Grid_new(-100, 100, -100, 100, kh);
	Animation* animation = Animation_new(N, 0.05, grid);
	Kernel kernel = Cubic;
	Setup* setup = Setup_new(50, timestep, verlet,kernel);

	simulate(grid, particles, N, setup, animation);

}

// With Verlet
void script2() {
	int N = 2000;
	Particle** particles = build_particles(N, 100);
	double kh = 10;
	double vmax = 2;
	int T = 4;
	double timestep = 1;
	double L = 2*T*vmax*timestep;
	Verlet* verlet = Verlet_new(kh, L, T);


	Grid* grid = Grid_new(-100, 100, -100, 100, kh+L);
	Animation* animation = Animation_new(N, 0.2, grid);
	Kernel kernel = Cubic;
	Setup* setup = Setup_new(50, timestep, verlet,kernel);

	simulate(grid, particles, N, setup, animation);

	free_particles(particles, N);
	Grid_free(grid);
	Setup_free(setup);
	Animation_free(animation);
}
