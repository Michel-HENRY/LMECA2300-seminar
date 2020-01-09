 /*************************************************************************
  * triangles example program using Draw_tools, a wrapper around
  * OpenGL and GLFW (www.glfw.org) to draw simple 2D graphics.
  *------------------------------------------------------------------------
  * Copyright (c) 2019-2020 Célestin Marot <marotcelestin@gmail.com>
  *
  * This software is provided 'as-is', without any express or implied
  * warranty. In no event will the authors be held liable for any damages
  * arising from the use of this software.
  *
  * Permission is granted to anyone to use this software for any purpose,
  * including commercial applications, and to alter it and redistribute it
  * freely, subject to the following restrictions:
  *
  * 1. The origin of this software must not be misrepresented; you must not
  *    claim that you wrote the original software. If you use this software
  *    in a product, an acknowledgment in the product documentation would
  *    be appreciated but is not required.
  *
  * 2. Altered source versions must be plainly marked as such, and must not
  *    be misrepresented as being the original software.
  *
  * 3. This notice may not be removed or altered from any source
  *    distribution.
  *
  *************************************************************************/

#include "draw_tools.h"
#include <math.h>


int main(int argc, char* argv[])
{
	window_t* window = window_new(1080,640, argv[0]);

	text_t* common_text = text_new(
	    (unsigned char[]){"rendering points using"},
	    GL_STATIC_DRAW);
	text_set_pos(common_text, (GLfloat[2]) {-1.0, 0.9});

	GLfloat textPos[2] = {-1.0 + 23 * 0.025, 0.9};

	text_t* points_draw_label = text_new(
	    (unsigned char[]){
	    "points_draw()"},
	    GL_STATIC_DRAW);
	// the default size for a character is 0.025 in width and 0.05 in height
	text_set_pos(points_draw_label, textPos);

	text_t* triangles_draw_label = text_new(
	    (unsigned char[]) {"triangles_draw()"},
	    GL_STATIC_DRAW);
	text_set_pos(triangles_draw_label, textPos);

	text_t* fast_triangles_draw_label = text_new(
	    (unsigned char[]) {"fast_triangles_draw()"},
	    GL_STATIC_DRAW);
	text_set_pos(fast_triangles_draw_label, textPos);

	text_t* triangle_strip_draw_label = text_new(
	    (unsigned char[]) {"triangle_strip_draw()"},
	    GL_STATIC_DRAW);
	text_set_pos(triangle_strip_draw_label, textPos);

	text_t* fast_triangle_strip_draw_label = text_new(
	    (unsigned char[]) {"fast_triangle_strip_draw()"},
	    GL_STATIC_DRAW);
	text_set_pos(fast_triangle_strip_draw_label, textPos);

	text_t* triangle_fan_draw_label = text_new(
	    (unsigned char[]) {"triangle_fan_draw()"},
	    GL_STATIC_DRAW);
	text_set_pos(triangle_fan_draw_label, textPos);

	text_t* fast_triangle_fan_draw_label = text_new(
	    (unsigned char[]) {"fast_triangle_fan_draw()"},
	    GL_STATIC_DRAW);
	text_set_pos(fast_triangle_fan_draw_label, textPos);

	points_t* pointset = points_new((float[10][2]) {
	                                    {-1.0,  0.0},
	                                    {-0.8, -0.6},
	                                    {-0.7,  0.6},
	                                    {-0.5,  0.0},
	                                    {-0.2, -0.4},
	                                    { 0.0,  0.8},
	                                    { 0.3,  0.0},
	                                    { 0.5, -0.6},
	                                    { 0.7,  0.6},
	                                    { 0.0, -0.9}
	                                }, 10, GL_STATIC_DRAW);
	points_set_color(pointset, (float[4]) {0.05, 0.1, 0.2, 0.6});
	points_set_outline_width(pointset, 0.025);

	order_t* order = order_new((GLuint[10]) {4, 3, 6, 9, 1, 0, 2, 5, 8, 7},
	                           10, GL_STATIC_DRAW);

	unsigned long frameCount = 0;
	while(!window_should_close(window)) {
		double wtime = window_get_time(window);

		text_draw(window, common_text);
		points_set_width(pointset, 0.0);
		points_set_outline_color(pointset, (GLfloat[4]) {0.3, 0.0, 0.0, 0.5});

		switch( (unsigned) wtime / 4 % 7) {
		case 0:
			text_draw(window, points_draw_label);
			points_draw(window, pointset, 0, TILL_END);
			break;
		case 1:
			text_draw(window, triangles_draw_label);
			triangles_draw(window, pointset, 0, TILL_END);
			break;
		case 2:
			text_draw(window, triangle_strip_draw_label);
			triangle_strip_draw(window, pointset, 0, TILL_END);
			break;
		case 3:
			text_draw(window, triangle_fan_draw_label);
			triangle_fan_draw(window, pointset, 0, TILL_END);
			break;
		case 4:
			text_draw(window, fast_triangles_draw_label);
			fast_triangles_draw(window, pointset, 0, TILL_END);
			break;
		case 5:
			text_draw(window, fast_triangle_strip_draw_label);
			fast_triangle_strip_draw(window, pointset, 0, TILL_END);
			break;
		case 6:
			text_draw(window, fast_triangle_fan_draw_label);
			fast_triangle_fan_draw(window, pointset, 0, TILL_END);
			break;
		}

		points_set_width(pointset, 0.05);
		points_set_outline_color(pointset, (GLfloat[4]) {1.0, 0.0, 0.0, 1.0});
		points_draw(window, pointset, 0, TILL_END);

		window_update(window);

		frameCount++;
	}

	printf("Ended correctly - %.2f second, %lu frames, %.2f fps\n",
	       window_get_time(window),
	       frameCount,
	       frameCount / window_get_time(window));

	text_delete(points_draw_label);
	text_delete(triangles_draw_label);
	text_delete(triangle_strip_draw_label);
	text_delete(triangle_fan_draw_label);
	text_delete(fast_triangles_draw_label);
	text_delete(fast_triangle_strip_draw_label);
	text_delete(fast_triangle_fan_draw_label);

	points_delete(pointset);

	order_delete(order);

	window_delete(window);

	return EXIT_SUCCESS;
}
