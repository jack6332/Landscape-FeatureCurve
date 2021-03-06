#include <iostream>
#include <algorithm>
#include <Fl/fl.h>
#include <windows.h>
//#include "GL/gl.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext.hpp>
#include "GL/glu.h"
#include <math.h>
#include "TrainView.H"
#include "TrainWindow.H"
#include "Utilities/3DUtils.H"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include"RenderUtilities/model.h"
#ifdef EXAMPLE_SOLUTION
#	include "TrainExample/TrainExample.H"
#endif

using namespace std;

Pnt3f GMT(Pnt3f p1, Pnt3f p2, Pnt3f p3, Pnt3f p4, float t) {
	Pnt3f q0;
	q0.x  = pow(1-t,3) * p1.x + 3*t * pow(1-t,2) * p2.x + 3*t*t*(1-t)* p3.x + t*t*t *p4.x;
	q0.y  = pow(1-t,3) * p1.y + 3*t * pow(1-t,2) * p2.y + 3*t*t*(1-t)* p3.y + t*t*t *p4.y;
	q0.z  = pow(1-t,3) * p1.z + 3*t * pow(1-t,2) * p2.z + 3*t*t*(1-t)* p3.z + t*t*t *p4.z;
	return q0;
}
Pnt3f Intersect(Pnt3f A, Pnt3f B,float length) {
	Pnt3f C;
	float m = (B.z - A.z) / (B.x - A.x);
	float _m = -1 / m;
	float bb = (_m * B.x) - B.z;
	float a = _m , b = -1, c = bb;
	float d = m, e = -1, f = -length * sqrt(m*m+1) + (m * B.x - B.z);

	float det = -_m + m;
	C.x = (c * e - b * f) / det;
	C.y = B.y;
	C.z = (-d * c + a * f) / det;
	return C;
}
Pnt3f _Intersect(Pnt3f A, Pnt3f B,float length) {
	Pnt3f C;
	float m = (B.z - A.z) / (B.x - A.x);
	float _m = -1 / m;
	float bb = (_m * B.x) - B.z;
	float a = _m , b = -1, c = bb;
	float d = m, e = -1, f = length*sqrt(m*m+1) + (m * B.x - B.z);

	float det = -_m + m;
	C.x = (c * e - b * f) / det;
	C.y = B.y;
	C.z = (-d * c + a * f) / det;
	return C;
}
glm::vec3 Rotate(glm::vec3 n, glm::vec3 v, float degree) {
	float theta = glm::radians(degree);
	n = glm::normalize(n);
	glm::mat3x3 T = {
		n.x * n.x * (1 - cos(theta)) + cos(theta)      , n.x * n.y * (1 - cos(theta)) - n.z*sin(theta)  , n.x * n.z * (1 - cos(theta)) + n.y * sin(theta),
		n.x * n.y * (1 - cos(theta)) + n.z * sin(theta), n.y * n.y * (1 - cos(theta)) + cos(theta)      , n.y * n.z* (1 - cos(theta)) - n.x *sin(theta),
		n.x * n.z * (1 - cos(theta)) - n.y * sin(theta), n.y * n.z * (1 - cos(theta)) + n.z * sin(theta), n.z*n.z * (1 - cos(theta)) + cos(theta)
	};
	return glm::vec3(v.x, v.y, v.z) * T;
}
glm::vec3 Pnt3_to_Vec3(Pnt3f a) {
	return glm::vec3(a.x, a.y, a.z);
}
Pnt3f Vec3_to_Pnt3(glm::vec3 a) {
	return Pnt3f(a.x, a.y, a.z);
}
int TrainView::sign(float n) {
	if (n > 0) {
		return 1;
	}
	else if (n < 0) {
		return -1;
	}
	else {
		return 0;
	}
}

void TrainView::scale(float* image,int image_size,float* result) {
	int tmep_size = image_size * 2;
	for (int i = 0; i < tmep_size; i++) {
		for (int j = 0; j < tmep_size; j++) {
			result[(tmep_size * 4) * j + 4 * i] = image[(image_size * 4) * (j / 2) + 4 * (i / 2)];
			result[(tmep_size * 4) * j + 4 * i + 1] = image[(image_size * 4) * (j / 2) + 4 * (i / 2) + 1];
			result[(tmep_size * 4) * j + 4 * i + 2] = image[(image_size * 4) * (j / 2) + 4 * (i / 2) + 2];
			result[(tmep_size * 4) * j + 4 * i + 3] =image[(image_size * 4) * (j / 2) + 4 * (i / 2) + 3];
		}
	}
}
void TrainView::push_gradient_data(Pnt3f q0) {
	gradient_data.push_back(q0.x);
	gradient_data.push_back(q0.y);
	gradient_data.push_back(q0.z);
	// Mentioned in 5.1 
	// n = (nx, ny)
	gradient_data.push_back(q0.normal.x); // nx
	gradient_data.push_back(q0.normal.y); // ny
	gradient_data.push_back(q0.normal.z); // gradient norm
	//cout << "nx:" << q0.normal.x << " ny:" << q0.normal.y << endl;
}
void TrainView::push_elevation_data(Pnt3f q0,int Area) {
	//Area == 0 means that the point is for elevation constraint
	if (Area == 0) {
		elevation_data.push_back(q0.x);
		elevation_data.push_back(q0.y);
		elevation_data.push_back(q0.z);
		/* Mentioned in 5.1 and 5.2
		The last alpha component of the texture is used to indicate which constraints
		have been set on the different areas.
		alpha = 0 -> elevation constrain
		alpha = 0.5 -> gradient constrain
		alpha = 1.0 -> elsewhere
		*/
		elevation_data.push_back(0.0); 
	}
	//Point is for gradient constraint
	else {
		elevation_data.push_back(q0.x);
		elevation_data.push_back(q0.y);
		elevation_data.push_back(q0.z);
		/* Mentioned in 5.1 and 5.2
		The last alpha component of the texture is used to indicate which constraints
		have been set on the different areas.
		alpha = 0 -> elevation constrain
		alpha = 0.5 -> gradient constrain
		alpha = 1.0 -> elsewhere
		*/
		elevation_data.push_back(0.5);
	}
}
void TrainView::draw_elevation_map() {
	glGenFramebuffers(1, &this->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	// create a color attachment texture
	glGenTextures(1, &textureElevetionMap);
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, textureElevetionMap);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, coarsestSize, coarsestSize, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureElevetionMap, 0);
	// create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
	glGenRenderbuffers(1, &rboElevetionMap);
	glBindRenderbuffer(GL_RENDERBUFFER, rboElevetionMap);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, coarsestSize, coarsestSize); // use a single renderbuffer object for both a depth AND stencil buffer.
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboElevetionMap); // now actually attach it
	// now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << endl;
	
	/*VAO*/
	unsigned int VBO[1], VAO[1];
    glGenVertexArrays(1, VAO);
    glGenBuffers(1, VBO);
    //Curve-
    glBindVertexArray(VAO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * elevation_data.size(), &elevation_data[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// render
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glEnable(GL_DEPTH_TEST); 

	//Clean the alpha to 1.0 as defualt
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//Curve
	elevation_shader->Use();
	glm::mat4 model = glm::mat4(1.0f);
	

	glViewport(0, 0, coarsestSize, coarsestSize);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-200, 200, -200, 200, 500, -200);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(-90, 1, 0, 0);
	glGetFloatv(GL_MODELVIEW_MATRIX, &view[0][0]);
	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);

	glUniformMatrix4fv(glGetUniformLocation(elevation_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(elevation_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(elevation_shader->Program, "model"), 1, GL_FALSE, &model[0][0]);

	glBindVertexArray(VAO[0]);
	glDrawArrays(GL_TRIANGLES, 0, elevation_data.size());

	// Read color from texture
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, coarsestSize, coarsestSize, GL_RGBA, GL_FLOAT, ImageBuffer);
	//cout << ImageBuffer[0]<<" "<< ImageBuffer[1] << " " << ImageBuffer[2] << " " << ImageBuffer[3] << endl;

	// now bind back to default framebuffer and draw a quad plane with the attached framebuffer color texture
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
	// clear all relevant buffers
	glClearColor(0.0f, 0.0f, 0.3f, 1.0f); // set clear color to white (not really necessary actually, since we won't be able to see behind the quad anyways)
	glClear(GL_COLOR_BUFFER_BIT);

	glDeleteVertexArrays(1, VAO);
	glDeleteBuffers(1, VBO);

}
void TrainView::draw_gradient_map() {
	//glGenFramebuffers(1, &this->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	// create a color attachment texture
	glGenTextures(1, &textureColorbuffer1);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, coarsestSize, coarsestSize, 0, GL_RGBA, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer1, 0);
	// create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
	glGenRenderbuffers(1, &rbo1);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo1);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, coarsestSize, coarsestSize); // use a single renderbuffer object for both a depth AND stencil buffer.
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo1); // now actually attach it
	// now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << endl;

	/*VAO*/
	unsigned int VBO[1], VAO[1];
	glGenVertexArrays(1, VAO);
	glGenBuffers(1, VBO);

	//Curve
	glBindVertexArray(VAO[0]);
	glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * gradient_data.size(), &gradient_data[0], GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// render
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	// make sure we clear the framebuffer's content
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	//Curve
	gradient_shader->Use();
	glm::mat4 model = glm::mat4(1.0f);

	glViewport(0, 0, coarsestSize, coarsestSize);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-200, 200, -200, 200, 1000, -200);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(-90, 1, 0, 0);
	glGetFloatv(GL_MODELVIEW_MATRIX, &view[0][0]);
	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);

	glUniformMatrix4fv(glGetUniformLocation(gradient_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(gradient_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(gradient_shader->Program, "model"), 1, GL_FALSE, &model[0][0]);

	glBindVertexArray(VAO[0]);

	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_INCR, GL_INCR);
	glStencilMask(0xFF);
	glDrawArrays(GL_TRIANGLES, 0, gradient_data.size());

	// clean curve intersection part
	test_shader->Use();
	glStencilFunc(GL_LESS, 1, 0xFF);
	glStencilMask(0x00);
	glDisable(GL_DEPTH_TEST);
	glUniformMatrix4fv(glGetUniformLocation(test_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(test_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(test_shader->Program, "model"), 1, GL_FALSE, &model[0][0]);
	glDrawArrays(GL_TRIANGLES, 0, gradient_data.size());

	// elevation_shaderRead value from gradient map
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, coarsestSize, coarsestSize, GL_RGBA, GL_FLOAT, ImageBuffer1);
	
	glDisable(GL_STENCIL_TEST);


	// now bind back to default framebuffer and draw a quad plane with the attached framebuffer color texture
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.0f, 0.0f, 0.3f, 1.0f);

	glDeleteVertexArrays(1, VAO);
	glDeleteBuffers(1, VBO);
}
void TrainView::draw_jacobi() {
	// render
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	jacobi_shader->Use();
	glm::mat4 model = glm::mat4(1.0f);

	glUniform1i(glGetUniformLocation(jacobi_shader->Program, "F"), 20);
	glUniform1i(glGetUniformLocation(jacobi_shader->Program, "E"), 10);
	glUniform1i(glGetUniformLocation(jacobi_shader->Program, "G"), 4);

	glBindVertexArray(final_vao[0]);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// Read color from texture
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, coarsestSize, coarsestSize, GL_RGBA, GL_FLOAT, HeightMapBuffer);
}

void TrainView::jacobi_texture() {

	float vertices[] = {
		// positions           // texture coords
		1.0f,  1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
	   -1.0f,  1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
	   -1.0f, -1.0f, 0.0f,
	   -1.0f,  1.0f, 0.0f
	};
	
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
	// create a color attachment texture
	glGenTextures(1, &final_texture);
	glActiveTexture(GL_TEXTURE20);
	glBindTexture(GL_TEXTURE_2D, final_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, coarsestSize, coarsestSize, 0, GL_RGBA, GL_FLOAT, ImageBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_texture, 0);
	// create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
	glGenRenderbuffers(1, &final_rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, final_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, coarsestSize, coarsestSize); // use a single renderbuffer object for both a depth AND stencil buffer.
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, final_rbo); // now actually attach it
	// now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << endl;

	/*VAO*/
	glGenVertexArrays(1, final_vao);
	glGenBuffers(1, final_vbo);
	//Curve-
	glBindVertexArray(final_vao[0]);
	glBindBuffer(GL_ARRAY_BUFFER, final_vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

}

void TrainView::gradient_diffuse(float* G, int size,int iteration) {
	for (int k = 0; k < iteration; k++) {
		for (int i = 1; i < size - 1; i++) {
			for (int j = 1; j < size - 1; j++) {
				if (G[(size * 4) * j + 4 * i + 3] == 0.5) {
					G[(size * 4) * j + 4 * i] = (G[(size * 4) * (j - 1) + 4 * (i)] + G[(size * 4) * (j + 1) + 4 * (i)] + G[(size * 4) * (j)+4 * (i - 1)] + G[(size * 4) * (j)+4 * (i + 1)]) / 4.0f;
					G[(size * 4) * j + 4 * i + 1] = (G[(size * 4) * (j - 1) + 4 * (i)+1] + G[(size * 4) * (j + 1) + 4 * (i)+1] + G[(size * 4) * (j)+4 * (i - 1) + 1] + G[(size * 4) * (j)+4 * (i + 1) + 1]) / 4.0f;
					float length = sqrtf(G[(size * 4) * j + 4 * i] * G[(size * 4) * j + 4 * i] + G[(size * 4) * j + 4 * i + 1] * G[(size * 4) * j + 4 * i + 1]);
					if (length != 0) {
						G[(size * 4) * j + 4 * i] /= length;
						G[(size * 4) * j + 4 * i + 1] /=length;
					}
					G[(size * 4) * j + 4 * i + 2] = (G[(size * 4) * (j - 1) + 4 * (i)+2] + G[(size * 4) * (j + 1) + 4 * (i)+2] + G[(size * 4) * (j)+4 * (i - 1) + 2] + G[(size * 4) * (j)+4 * (i + 1) + 2]) / 4.0f;
				}
			}
		}
	}
}
void TrainView::run() {
	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	/* Mentioned in 5.2 
	Multigrid implementation
	*/

	// 128 x 128
	grid0 = new float[coarsestSize * coarsestSize * 4];
	elevation_grid0 = new float[coarsestSize * coarsestSize * 4];
	gradient_grid0 = new float[coarsestSize * coarsestSize * 4];


	memcpy(elevation_grid0, ImageBuffer, coarsestSize * coarsestSize * 4 * sizeof(float));
	memcpy(gradient_grid0, ImageBuffer1, coarsestSize * coarsestSize * 4 * sizeof(float));

	gradient_diffuse(gradient_grid0, coarsestSize, 200);
	
	glGenTextures(1, &textureColorbuffer7);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer7);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, coarsestSize, coarsestSize, 0, GL_RGBA, GL_FLOAT, gradient_grid0);

	jacobi_texture();
	for (int ii = 0; ii < iteration; ii++) {
		draw_jacobi();
	}
	memcpy(grid0, HeightMapBuffer, coarsestSize * coarsestSize * 4 * sizeof(float));

	/* Mentioned in 4.1 Figure 9.
	To fill gradient's hole caused by gradient intersection
	*/

	// Upsampling from 128 x 128 to 256 x 256
	grid1 = new float[grid1_size * grid1_size * 4];
	scale(grid0, coarsestSize, grid1);
	elevation_grid1 = new float[grid1_size * grid1_size * 4];
	scale(elevation_grid0, coarsestSize, elevation_grid1);
	gradient_grid1 = new float[grid1_size * grid1_size * 4];
	scale(gradient_grid0, coarsestSize, gradient_grid1);

	//gradient_diffuse(gradient_grid1, grid1_size, iteration * 2);
	//jacobi(grid1, elevation_grid1, gradient_grid1, grid1_size, iteration/3*2);

	// 512 x 512
	grid = new float[gridsize * gridsize * 4];
	scale(grid1, grid1_size, grid);

	elevation_grid = new float[gridsize * gridsize * 4];
	scale(elevation_grid1, grid1_size, elevation_grid);

	gradient_grid = new float[gridsize * gridsize * 4];
	scale(gradient_grid1, grid1_size, gradient_grid);
	//gradient_diffuse(gradient_grid, gridsize, iteration * 1);
	//jacobi(grid, elevation_grid, gradient_grid, gridsize, iteration/3);

	// Normalize color
	for (int i = 0; i < gridsize * gridsize * 4; i++) {
		grid[i] /= 255.0;
	}

	glGenTextures(1, &textureColorbuffer2);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gridsize, gridsize, 0, GL_RGBA, GL_FLOAT, grid);

	glGenTextures(1, &textureColorbuffer5);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer5);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gridsize, gridsize, 0, GL_RGBA, GL_FLOAT, gradient_grid);
}
//************************************************************************
//
// * Constructor to set up the GL window
//========================================================================



TrainView::
TrainView(int x, int y, int w, int h, const char* l) 
	: Fl_Gl_Window(x,y,w,h,l)
//========================================================================
{
	mode( FL_RGB|FL_ALPHA|FL_DOUBLE | FL_STENCIL );

	resetArcball();

	//Test//
	Curves.push_back(Curve());
	//Curves.push_back(Curve());
}

//************************************************************************
//
// * Reset the camera to look at the world
//========================================================================
void TrainView::
resetArcball()
//========================================================================
{
	// Set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this, 40, 250, .2f, .4f, 0);
}

int TrainView::handle(int event)
{
	if (tw->worldCam->value())
		if (arcball.handle(event)) 
			return 1;

	// remember what button was used
	static int last_push;

	switch(event) {
		// Mouse button being pushed event
		case FL_PUSH:
			last_push = Fl::event_button();
			// if the left button be pushed is left mouse button
			if (last_push == FL_LEFT_MOUSE  ) {
				doPick();
				damage(1);
				return 1;
			};
			break;

	   // Mouse button release event
		case FL_RELEASE: // button release
			damage(1);
			last_push = 0;
			return 1;

		// Mouse button drag event
		case FL_DRAG:

			// Compute the new control point position
			if ((last_push == FL_LEFT_MOUSE) && (selectedCube >= 0)) {
				ControlPoint *cp = &Curves[SelectedCurve].points[SelectedNode];
				
				double r1x, r1y, r1z, r2x, r2y, r2z;
				getMouseLine(r1x, r1y, r1z, r2x, r2y, r2z);

				double rx, ry, rz;
				mousePoleGo(r1x, r1y, r1z, r2x, r2y, r2z, 
								static_cast<double>(cp->pos.x), 
								static_cast<double>(cp->pos.y),
								static_cast<double>(cp->pos.z),
								rx, ry, rz,
								(Fl::event_state() & FL_CTRL) != 0);

				cp->pos.x = (float) rx;
				cp->pos.y = (float) ry;
				cp->pos.z = (float) rz;
				damage(1);
			}
			break;

		// in order to get keyboard events, we need to accept focus
		case FL_FOCUS:
			return 1;

		// every time the mouse enters this window, aggressively take focus
		case FL_ENTER:	
			focus(this);
			break;

		case FL_KEYBOARD:
		 		int k = Fl::event_key();
				int ks = Fl::event_state();
				if (k == 'p') {
					// Print out the selected control point information
					if (selectedCube >= 0) 
						printf("Selected(%d) (%g %g %g) (%g %g %g)\n",
								 selectedCube,
								 m_pTrack->points[selectedCube].pos.x,
								 m_pTrack->points[selectedCube].pos.y,
								 m_pTrack->points[selectedCube].pos.z,
								 m_pTrack->points[selectedCube].orient.x,
								 m_pTrack->points[selectedCube].orient.y,
								 m_pTrack->points[selectedCube].orient.z);
					else
						printf("Nothing Selected\n");

					return 1;
				};
				break;
	}

	return Fl_Gl_Window::handle(event);
}

//************************************************************************
//
// * this is the code that actually draws the window
//   it puts a lot of the work into other routines to simplify things
//========================================================================
void TrainView::draw()
{

	//initialized glad
	if (gladLoadGL())
	{
		//initiailize VAO, VBO, Shader...
	}
	else
		throw std::runtime_error("Could not initialize GLAD!");

	// Set up the view port
	glViewport(0,0,w(),h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0,0,.3f,0);		// background should be blue

	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glEnable(GL_DEPTH);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	

	// Blayne prefers GL_DIFFUSE
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		
	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glClear(GL_DEPTH_BUFFER_BIT);

	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	} else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}

	//*********************************************************************
	//
	// * set the light parameters
	//
	//**********************************************************************
	GLfloat lightPosition1[]	= {0,1,1,0}; // {50, 200.0, 50, 1.0};
	GLfloat lightPosition2[]	= {1, 0, 0, 0};
	GLfloat lightPosition3[]	= {0, -1, 0, 0};
	GLfloat yellowLight[]		= {0.5f, 0.5f, .1f, 1.0};
	GLfloat whiteLight[]			= {1.0f, 1.0f, 1.0f, 1.0};
	GLfloat blueLight[]			= {.1f,.1f,.3f,1.0};
	GLfloat grayLight[]			= {.3f, .3f, .3f, 1.0};

	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);



	//*********************************************************************
	// now draw the ground plane
	//*********************************************************************
	// set to opengl fixed pipeline(use opengl 1.x draw function)
	glUseProgram(0);

	setupFloor();
	glDisable(GL_LIGHTING);
	//drawFloor(200,200);


	//*********************************************************************
	// now draw the object and we need to do it twice
	// once for real, and then once for shadows
	//*********************************************************************
	glEnable(GL_LIGHTING);
	setupObjects();
	
	drawStuff();
	
	glPointSize(5);
	glBegin(GL_POINTS);
	glColor3ub(0, 255, 0);
	glVertex3f(100, 5, 0);
	glVertex3f(-100, 0, 0);
	glEnd();
	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		//setupShadows();
		drawStuff(true);
		unsetupShadows();
	}
}

//************************************************************************
//
// * This sets up both the Projection and the ModelView matrices
//   HOWEVER: it doesn't clear the projection first (the caller handles
//   that) - its important for picking
//========================================================================
void TrainView::
setProjection()
//========================================================================
{
	// Compute the aspect ratio (we'll need it)
	float aspect = (static_cast<float>(w()) / static_cast<float>(h()));

	// Check whether we use the world camp
	if (tw->worldCam->value())
		arcball.setProjection(false);
	// Or we use the top cam
}

void TrainView::drawStuff(bool doingShadows)
{
	gradient_data.clear();
	elevation_data.clear();
	for (int i = 0; i < Curves.size(); i++) {
		float t = 0;
		float divide = tw->segment->value();
		float interval = tw->interval->value();
		float count = 0;
		Curves[i].arclength_points.clear();
		ControlPoint p1 = Curves[i].points[0];
		ControlPoint p2 = Curves[i].points[1];
		ControlPoint p3 = Curves[i].points[2];
		ControlPoint p4 = Curves[i].points[3];
		Curves[i].arclength_points.push_back(p1.pos);

		/* Mentioned in 4.1
		The curve is first discretized along it and approximated
		into piecewise linear parts.
		*/
		for (int j = 0; j < divide; j++) {
			Pnt3f Q0 = GMT(p1.pos, p2.pos, p3.pos, p4.pos, t);
			Pnt3f Q1 = GMT(p1.pos, p2.pos, p3.pos, p4.pos, t += 1/divide);
			count += (Q1 - Q0).length2D();
			if (count > interval) {
				count = 0;
				Curves[i].arclength_points.push_back(Q1);
			}	
		}
		Curves[i].arclength_points.push_back(p4.pos);
	}
	/* Mentioned in 4.1
		At each linear coordinate, the
		whole parameter set is linearly interpolated from constraints
		points except for the elevation constraint where a cubic interpolation is used to avoid unnatural piecewise linear ridge
		lines on mountains. Then, at each side, quadrangles are generated in the direction of the normal whose lengths are set
		from the interpolated radii values r, a and b
	*/
	for (int i = 0; i < Curves.size(); i++) {
		Curves[i].arclength_points[Curves[i].arclength_points.size() - 1].r = Curves[i].points[3].pos.r;
		Curves[i].arclength_points[0].r = Curves[i].points[0].pos.r;

		float r_init = Curves[i].arclength_points[0].r;
		float r_interporate = (Curves[i].arclength_points[Curves[i].arclength_points.size() - 1].r - Curves[i].arclength_points[0].r)/ (Curves[i].arclength_points.size()-1);

		float a_init = Curves[i].arclength_points[0].a;
		float a_interporate = (Curves[i].arclength_points[Curves[i].arclength_points.size() - 1].a - Curves[i].arclength_points[0].a) / (Curves[i].arclength_points.size() - 1);

		float b_init = Curves[i].arclength_points[0].b;
		float b_interporate = (Curves[i].arclength_points[Curves[i].arclength_points.size() - 1].b - Curves[i].arclength_points[0].b)/ (Curves[i].arclength_points.size()-1);

		float phi_init = Curves[i].arclength_points[0].phi;
		float phi_interporate = (Curves[i].arclength_points[Curves[i].arclength_points.size() - 1].phi - Curves[i].arclength_points[0].phi) / (Curves[i].arclength_points.size() - 1);

		float theta_init = Curves[i].arclength_points[0].theta;
		float theta_interporate = (Curves[i].arclength_points[Curves[i].arclength_points.size() - 1].theta - Curves[i].arclength_points[0].theta) / (Curves[i].arclength_points.size() - 1);
		
		/* Mentioned in 3.1
		To calcualte the point which is parallel with the feature curve 
		*/
		Pnt3f p2,_p2,p4,_p4, p6, _p6;
		for (int j = 0; j < Curves[i].arclength_points.size()-2; j++) {
			Pnt3f q0 = Curves[i].arclength_points[j], q1 = Curves[i].arclength_points[j+1],q2,q3,q4,q5,q6,q7;
			
			// Point in right of feature curve
			if (q0.x < q1.x) {
				q2 = Intersect(q0, q1, r_init + r_interporate * (j + 1));
				q3 = Intersect(q1, q0, r_init + r_interporate * j);
				q4 = q2;
				q5 = q3;
				q6 = Intersect(q0, q1, r_init + r_interporate * (j + 1) + b_init + b_interporate*(j+1));
				q7 = Intersect(q1, q0, r_init + r_interporate * j + b_init + b_interporate * j);
			}
			else {
				q2 = _Intersect(q0, q1, r_init + r_interporate * (j + 1));
				q3 = _Intersect(q1, q0, r_init + r_interporate * j);
				q4 = q2;
				q5 = q3;
				q6 = _Intersect(q0, q1, r_init + r_interporate * (j + 1) + b_init + b_interporate * (j + 1));
				q7 = _Intersect(q1, q0, r_init + r_interporate * j + b_init + b_interporate * j);
			}
			glm::vec3 Axis = glm::normalize(glm::vec3(q3.x - q2.x,0.0, q3.z - q2.z));

			glm::vec3 normal = glm::normalize((Pnt3_to_Vec3(q7) - Pnt3_to_Vec3(q5)));
			glm::vec3 _normal = glm::normalize((Pnt3_to_Vec3(q6) - Pnt3_to_Vec3(q4)));

			glm::vec2 n = glm::normalize(glm::vec2(normal.x, normal.z));
			glm::vec2 _n = glm::normalize(glm::vec2(_normal.x, _normal.z));

			//cout << "first:" << n.x << " " << n.y << endl;

			q6 = Vec3_to_Pnt3(((Rotate(Axis, Pnt3_to_Vec3(q6 - q2), -90 + phi_init + phi_interporate * (j + 1)))) + Pnt3_to_Vec3(q2));
			q7 = Vec3_to_Pnt3(((Rotate(Axis, Pnt3_to_Vec3(q7 - q3), -90 + phi_init + phi_interporate * (j)))) + Pnt3_to_Vec3(q3));

			q0.normal = glm::vec3(0.0, 0.0, 1.0);
			q1.normal = glm::vec3(0.0, 0.0, 1.0);
			q2.normal = glm::vec3(0.0, 0.0, 1.0);
			q3.normal = glm::vec3(0.0, 0.0, 1.0);

			q4.normal = glm::vec3((_n.x), (_n.y), 1.0);
			q5.normal = glm::vec3((n.x), (n.y), 1.0);
			q6.normal = glm::vec3((_n.x), (_n.y), sin(glm::radians(phi_init + phi_interporate * (j + 1))));
			q7.normal = glm::vec3((n.x), (n.y), sin(glm::radians(phi_init + phi_interporate * (j))));

			/* Mentioned in 4.1
			For slope angle primitives, the vertex color is set to its corresponding interpolated value along the curve and is set to 0 at
			the end of the quadrangle so as to avoid gradient discontinuities and artifacts

			if not set point's normal value for the point, the value initially will be (0,0,0). So that normal of q6, q7 will be (0,0,0).
			Furthermore q6, q7 are the points in the end of the quadrangle.
			*/


			/*Elevation Vertex*/
			//q0,q1,q2
			push_elevation_data(q0,0);
			push_elevation_data(q1,0);
			push_elevation_data(q2,0);
			//q2,q3,q0
			push_elevation_data(q2,0);
			push_elevation_data(q3,0);
			push_elevation_data(q0,0);
			//q2,q4,q3
			push_elevation_data(q4,1);
			push_elevation_data(q6,1);
			push_elevation_data(q5,1);
			//q4,q5,q3
			push_elevation_data(q6,1);
			push_elevation_data(q7,1);
			push_elevation_data(q5,1);

			// In order to fill the hole caused by quadrangle
			if (j > 0) {
				//q0,q3,p2
				push_elevation_data(q0,0);
				push_elevation_data(q3,0);
				push_elevation_data(p2,0);
				//q0,q3,p2
				push_elevation_data(p4,1);
				push_elevation_data(q5,1);
				push_elevation_data(q7,1);
				//q0,q3,p2
				push_elevation_data(q7,1);
				push_elevation_data(p6,1);
				push_elevation_data(p4,1);
			}

			/*Gradient Vertex*/
			//q0,q1,q2
			push_gradient_data(q0);
			push_gradient_data(q1);
			push_gradient_data(q2);
			//q2,q3,q0
			push_gradient_data(q2);
			push_gradient_data(q3);
			push_gradient_data(q0);
			//q2,q4,q3
			push_gradient_data(q4);
			push_gradient_data(q6);
			push_gradient_data(q5);
			//q4,q5,q3
			push_gradient_data(q6);
			push_gradient_data(q7);
			push_gradient_data(q5);
			//Fill holes
			if (j > 0) {
				//q0,q3,p2
				push_gradient_data(q0);
				push_gradient_data(q3);
				push_gradient_data(p2);
				//q0,q3,p2
				push_gradient_data(p4);
				push_gradient_data(q5);
				push_gradient_data(q7);
				//q0,q3,p2
				push_gradient_data(q7);
				push_gradient_data(p6);
				push_gradient_data(p4);
			}
			/*Previous Points*/
			p2 = q2;
			p4 = q4;
			p6 = q6;



			// Point in left of feature curve
			if (q0.x < q1.x) {
				q2 = _Intersect(q0, q1, r_init + r_interporate * (j + 1));
				q3 = _Intersect(q1, q0, r_init + r_interporate * j);
				q4 = q2;
				q5 = q3;
				q6 = _Intersect(q0, q1, r_init + r_interporate * (j + 1) + a_init + a_interporate * (j + 1));
				q7 = _Intersect(q1, q0, r_init + r_interporate * j + a_init + a_interporate * j);
			}
			else {
				q2 = Intersect(q0, q1, r_init + r_interporate * (j + 1));
				q3 = Intersect(q1, q0, r_init + r_interporate * j);
				q4 = q2;
				q5 = q3;
				q6 = Intersect(q0, q1, r_init + r_interporate * (j + 1) + a_init + a_interporate * (j + 1));
				q7 = Intersect(q1, q0, r_init + r_interporate * j + a_init + a_interporate * j);
			}

			Axis = glm::normalize(glm::vec3(q3.x - q2.x,0, q3.z - q2.z));

			normal = glm::normalize((Pnt3_to_Vec3(q7) - Pnt3_to_Vec3(q5)));
			_normal = glm::normalize((Pnt3_to_Vec3(q6) - Pnt3_to_Vec3(q4)));

			n = glm::normalize(glm::vec2(normal.x, normal.z));
			_n = glm::normalize(glm::vec2(_normal.x, _normal.z));

			//cout << "second:" << n.x << " " << n.y << endl;

			q6 = Vec3_to_Pnt3(((Rotate(Axis, Pnt3_to_Vec3(q6 - q2), 90 - theta_init - theta_interporate * (j + 1)))) + Pnt3_to_Vec3(q2));
			q7 = Vec3_to_Pnt3(((Rotate(Axis, Pnt3_to_Vec3(q7 - q3), 90 - theta_init - theta_interporate * j))) + Pnt3_to_Vec3(q3));

			q0.normal = glm::vec3(0.0, 0.0, 1.0);
			q1.normal = glm::vec3(0.0, 0.0, 1.0);
			q2.normal = glm::vec3(0.0, 0.0, 1.0);
			q3.normal = glm::vec3(0.0, 0.0, 1.0);

			q4.normal = glm::vec3((_n.x), (_n.y), 1.0);
			q5.normal = glm::vec3((n.x), (n.y), 1.0);
			q6.normal = glm::vec3((_n.x), (_n.y), sin(glm::radians(theta_init + theta_interporate * (j + 1))));
			q7.normal = glm::vec3((n.x), (n.y), sin(glm::radians(theta_init + theta_interporate * (j))));

			/* Mentioned in 4.1
			For slope angle primitives, the vertex color is set to its corresponding interpolated value along the curve and is set to 0 at
			the end of the quadrangle so as to avoid gradient discontinuities and artifacts

			if not set point's normal value for the point, the value initially will be (0,0,0). So that normal of q6, q7 will be (0,0,0). 
			Furthermore q6, q7 are the points in the end of the quadrangle.
			*/


			/*Elevation Vertex*/
			//q0,q1,q2
			push_elevation_data(q0, 0);
			push_elevation_data(q1, 0);
			push_elevation_data(q2, 0);
			//q2,q3,q0
			push_elevation_data(q2, 0);
			push_elevation_data(q3, 0);
			push_elevation_data(q0, 0);
			//q2,q4,q3
			push_elevation_data(q4, 1);
			push_elevation_data(q6, 1);
			push_elevation_data(q5, 1);
			//q4,q5,q3
			push_elevation_data(q6, 1);
			push_elevation_data(q7, 1);
			push_elevation_data(q5, 1);

			//Fill holes
			if (j > 0) {
				//q0,q3,p2
				push_elevation_data(q0, 0);
				push_elevation_data(q3, 0);
				push_elevation_data(_p2, 0);
				//q0,q3,p2
				push_elevation_data(_p4, 1);
				push_elevation_data(q5, 1);
				push_elevation_data(q7, 1);
				//q0,q3,p2
				push_elevation_data(q7, 1);
				push_elevation_data(_p6, 1);
				push_elevation_data(_p4, 1);
			}

			/*Gradient Vertex*/
			//q0,q1,q2
			push_gradient_data(q0);
			push_gradient_data(q1);
			push_gradient_data(q2);
			//q2,q3,q0
			push_gradient_data(q2);
			push_gradient_data(q3);
			push_gradient_data(q0);
			//q2,q4,q3
			push_gradient_data(q4);
			push_gradient_data(q6);
			push_gradient_data(q5);
			//q4,q5,q3
			push_gradient_data(q6);
			push_gradient_data(q7);
			push_gradient_data(q5);

			// In order to fill the hole caused by quadrangle
			if (j > 0) {
				//q0,q3,p2
				push_gradient_data(q0);
				push_gradient_data(q3);
				push_gradient_data(_p2);
				//q0,q3,p2
				push_gradient_data(_p4);
				push_gradient_data(q5);
				push_gradient_data(q7);
				//q0,q3,p2
				push_gradient_data(q7);
				push_gradient_data(_p6);
				push_gradient_data(_p4);
			}
			_p2 = q2;
			_p4 = q4;
			_p6 = q6;
		}
	}

	if (!this->elevation_shader) {
			this->elevation_shader = new
				Shader(
					"../src/Shaders/elevation.vs",
					nullptr, nullptr, nullptr,
					"../src/Shaders/elevation.fs");
	}

	if (!this->gradient_shader) {
		this->gradient_shader = new
			Shader(
				"../src/Shaders/gradient.vs",
				nullptr, nullptr, nullptr,
				"../src/Shaders/gradient.fs");
	}

	if (!this->screen_shader) {
		this->screen_shader = new
			Shader(
				"../src/Shaders/screen.vs",
				nullptr, nullptr, nullptr,
				"../src/Shaders/screen.fs");
	}
	if (!this->heightmap_shader) {
		this->heightmap_shader = new
			Shader(
				"../src/Shaders/heightmap.vs",
				nullptr, nullptr, nullptr,
				"../src/Shaders/heightmap.fs");
	}
	if (!this->test_shader) {
		this->test_shader = new
			Shader(
				"../src/Shaders/test.vs",
				nullptr, nullptr, nullptr,
				"../src/Shaders/test.fs");
	}
	if (!this->jacobi_shader) {
		this->jacobi_shader = new
			Shader(
				"../src/Shaders/jacobi.vs",
				nullptr, nullptr, nullptr,
				"../src/Shaders/jacobi.fs");
	}
	if (!wave_model) {
		wave_model = new Model("../wave/wave.obj");
	}
	if (!mountain_texture) {
		mountain_texture = new Texture2D("../wave/mountain.png");
	}

	float vertices[] = {
    // positions                          // texture coords
     1.0f,  1.0f, 0.0f,     1.0f, 1.0f,   // top right
     1.0f, -1.0f, 0.0f,     1.0f, 0.0f,   // bottom right
	-1.0f,  1.0f, 0.0f,     0.0f, 1.0f,    // top left 
	 1.0f, -1.0f, 0.0f,     1.0f, 0.0f,   // bottom right
    -1.0f, -1.0f, 0.0f,     0.0f, 0.0f,   // bottom left
    -1.0f,  1.0f, 0.0f,     0.0f, 1.0f    // top left 
	};

	unsigned int VBO[3], VAO[3];
    glGenVertexArrays(3, VAO);
    glGenBuffers(3, VBO);

    //Curve
    glBindVertexArray(VAO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * gradient_data.size(), &gradient_data[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	//Ground
	glBindVertexArray(VAO[1]);
	glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	draw_elevation_map();
	draw_gradient_map();
	run();

	// Code below are using for visulization

	float wi, he;
	if ((static_cast<float>(w()) / static_cast<float>(h())) >= 1) {
		wi = 100;
		he = wi / (static_cast<float>(w()) / static_cast<float>(h()));
	}
	else {
		he = 100;
		wi = he * (static_cast<float>(w()) / static_cast<float>(h()));
	}
	glViewport(0, 0, w(), h());
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-wi, wi, -he, he, 200, -200);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(-90, 1, 0, 0);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();
	glGetFloatv(GL_MODELVIEW_MATRIX, &view[0][0]);
	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	

	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);

	screen_shader->Use();
	//Ground of Elevation
	glm::mat4 trans = glm::mat4(1.0f);
	trans = glm::translate(trans, glm::vec3(200, 0, 0));
	trans = glm::scale(trans, glm::vec3(100, 0, 100));
	
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "model"), 1, GL_FALSE, &trans[0][0]);
	glUniform1i(glGetUniformLocation(screen_shader->Program, "Texture"), 10);

	glBindVertexArray(VAO[1]);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	//Ground of Gradient
	glm::mat4 transs = glm::mat4(1.0f);
	transs = glm::translate(transs, glm::vec3(-200, 1, 0));
	transs = glm::scale(transs, glm::vec3(100, 1, 100));

	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "model"), 1, GL_FALSE, &transs[0][0]);
	glUniform1i(glGetUniformLocation(screen_shader->Program, "Texture"), 9);

	glBindVertexArray(VAO[1]);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	//Ground of gradient
	glm::mat4 trans_gradient= glm::mat4(1.0f);
	trans_gradient = glm::translate(trans_gradient, glm::vec3(-200, 0, 200));
	trans_gradient = glm::scale(trans_gradient, glm::vec3(100, 1, 100));

	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "model"), 1, GL_FALSE, &trans_gradient[0][0]);
	glUniform1i(glGetUniformLocation(screen_shader->Program, "Texture"), 3);

	glBindVertexArray(VAO[1]);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	//grid
	glm::mat4 trans_height = glm::mat4(1.0f);
	trans_height = glm::translate(trans_height, glm::vec3(-200, 0, -200));
	trans_height = glm::scale(trans_height, glm::vec3(100, 1, 100));

	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(screen_shader->Program, "model"), 1, GL_FALSE, &trans_height[0][0]);
	glUniform1i(glGetUniformLocation(screen_shader->Program, "Texture"), 2);

	glBindVertexArray(VAO[1]);
	glDrawArrays(GL_TRIANGLES, 0, 6);


	heightmap_shader->Use();
	//Ground of Height Map
	glm::mat4 transss = glm::mat4(1.0f);
	transss = glm::translate(transss, glm::vec3(0, 0,400));
	transss = glm::scale(transss, glm::vec3(100, 1.0f, 100));


	glUniformMatrix4fv(glGetUniformLocation(heightmap_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(heightmap_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(heightmap_shader->Program, "model"), 1, GL_FALSE, &transss[0][0]);
	glUniform1i(glGetUniformLocation(heightmap_shader->Program, "Texture"), 5);
	mountain_texture->bind(5);
	wave_model->meshes[0].textures[0].id = textureColorbuffer2;
	wave_model->Draw(*heightmap_shader);

	//Curve
	gradient_shader->Use();
	glm::mat4 model = glm::mat4(1.0f);

	glUniformMatrix4fv(glGetUniformLocation(gradient_shader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(gradient_shader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(gradient_shader->Program, "model"), 1, GL_FALSE, &model[0][0]);

	glBindVertexArray(VAO[0]);
	glDrawArrays(GL_TRIANGLES, 0, gradient_data.size());

	glDeleteVertexArrays(3, VAO);
	glDeleteBuffers(3, VBO);
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteTextures(1, &textureElevetionMap);
	glDeleteRenderbuffers(1, &rboElevetionMap);
	glDeleteTextures(1, &textureColorbuffer1);
	glDeleteRenderbuffers(1, &rbo1);
	glDeleteTextures(1, &textureColorbuffer2);
	glDeleteRenderbuffers(1, &rbo2);
	glDeleteTextures(1, &textureColorbuffer3);
	glDeleteRenderbuffers(1, &rbo3);
	glDeleteTextures(1, &textureColorbuffer4);
	glDeleteTextures(1, &textureColorbuffer5);
	glDeleteTextures(1, &textureColorbuffer6);
	glDeleteTextures(1, &textureColorbuffer7);
	glDeleteTextures(1, &textureColorbuffer8);
	glDeleteRenderbuffers(1, &rbo4);
	// final
	glDeleteTextures(1, &final_texture);
	glDeleteRenderbuffers(1, &final_rbo);
	glDeleteVertexArrays(1, final_vao);
	glDeleteBuffers(1, final_vbo);

	delete grid0;
	delete grid1;
	delete grid;

	delete elevation_grid0;
	delete elevation_grid1;
	delete elevation_grid;

	delete gradient_grid0;
	delete gradient_grid1;
	delete gradient_grid;

	glUseProgram(0);

	if (!tw->trainCam->value()) {
		for (int i = 0; i < Curves.size(); i++) {
			for (int j = 0; j < Curves[i].points.size(); j++) {
				if (SelectedCurve == i && j == SelectedNode)
					glColor3ub(240, 240, 30);
				else
					glColor3ub(240, 60, 60);
				Curves[i].points[j].draw();
			}
		}
	}

	for (int i = 0; i < Curves.size(); i++) {
		float t = 0;
		ControlPoint p1 = Curves[i].points[0];
		ControlPoint p2 = Curves[i].points[1];
		ControlPoint p3 = Curves[i].points[2];
		ControlPoint p4 = Curves[i].points[3];
		Pnt3f c;
		for (int j = 0; j < 100; j++) {
			Pnt3f q0 = GMT(p1.pos, p2.pos, p3.pos, p4.pos, t);
			Pnt3f q1 = GMT(p1.pos, p2.pos, p3.pos, p4.pos, t += 0.01);
			glBegin(GL_LINES);
			glColor3ub(0, 255, 0);
			glVertex3f(q0.x, q0.y, q0.z);
			glVertex3f(q1.x, q1.y, q1.z);
			glEnd();
		}
	}
}

void TrainView::
doPick()
//========================================================================
{
	// since we'll need to do some GL stuff so we make this window as 
	// active window
	make_current();		

	// where is the mouse?
	int mx = Fl::event_x(); 
	int my = Fl::event_y();

	// get the viewport - most reliable way to turn mouse coords into GL coords
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Set up the pick matrix on the stack - remember, FlTk is
	// upside down!
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	gluPickMatrix((double)mx, (double)(viewport[3]-my), 
						5, 5, viewport);

	// now set up the projection
	setProjection();

	// now draw the objects - but really only see what we hit
	GLuint buf[100];
	glSelectBuffer(100,buf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	int count = 0;
	for (int i = 0;i < Curves.size(); i++) {
		for(int j=0;j<Curves[i].points.size();j++){
			glLoadName((GLuint) ((count++)+1));
			Curves[i].points[j].draw();
		}
	}


	// go back to drawing mode, and see how picking did
	int hits = glRenderMode(GL_RENDER);
	if (hits) {
		// warning; this just grabs the first object hit - if there
		// are multiple objects, you really want to pick the closest
		// one - see the OpenGL manual 
		// remember: we load names that are one more than the index
		selectedCube = buf[3]-1;
		int ii = 0;
		int temp = selectedCube;
		for (ii = 0; ii < Curves.size(); ii++) {
			if (temp <= Curves[ii].points.size() - 1) {
				break;
			}
			else {
				temp -= Curves[ii].points.size();
			}
		}
		SelectedCurve = ii;
		SelectedNode = temp;

		tw->radius->value(Curves[ii].points[temp].pos.r);
		tw->a->value(Curves[ii].points[temp].pos.a);
		tw->b->value(Curves[ii].points[temp].pos.b);
		tw->theta->value(Curves[ii].points[temp].pos.theta);
		tw->phi->value(Curves[ii].points[temp].pos.phi);

	} else // nothing hit, nothing selected
		selectedCube = -1;

	printf("Selected Cube %d\n",selectedCube);
}