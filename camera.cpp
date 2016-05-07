#include <iostream>
#include "camera.h"
#include "global.h"
#include "world.h"
#include "bullet.h"

#include "sphere.h"

#define CAMERA_MOVEMENT	0.02f
#define CAMERA_ROTATE	(2.f * PI / 180.f)
#define CAMERA_ELEV	(2.f * PI / 180.f)
#define CAMERA_ACCEL	0.2f
#define CAMERA_SIZE	0.015f
#define CAMERA_HEIGHT	(CAMERA_SIZE * 10.f)

#define CAMERA_INIT_POS	glm::vec3(0.f, 1.f + CAMERA_HEIGHT, 1.f)
/*#ifdef BULLET
#define CAMERA_INIT_POS	glm::vec3(0.f, 0.f, 1.f + arena.scale + 3.f)
#else
#endif*/

using namespace std;
using namespace glm;

Camera camera;

Camera::Camera()
{
	pos = CAMERA_INIT_POS;
	rot = quat();
	speed = 0.f;
	impulse = 0.f;
	input.cursor = vec2(-1.f, -1.f);
	input.pressed = false;
	movement = CAMERA_MOVEMENT;
	backup();
	updateCalc();
}

void Camera::keyCB(int key)
{
	switch (key) {
	case GLFW_KEY_LEFT:
		// Turn camera to the left
		rotate(CAMERA_ROTATE);
		break;
	case GLFW_KEY_RIGHT:
		// Turn camera to the right
		rotate(-CAMERA_ROTATE);
		break;
	case GLFW_KEY_UP:
		// Increase the forward speed of the camera
		accelerate(CAMERA_ACCEL);
		break;
	case GLFW_KEY_DOWN:
		// Decrease the forward speed of the camera (minimum 0, stays)
		accelerate(-CAMERA_ACCEL);
		break;
	case GLFW_KEY_PAGE_UP:
		// Increase the elevation of the camera (optional)
		elevate(CAMERA_ELEV);
		break;
	case GLFW_KEY_PAGE_DOWN:
		// Decrease the elevation of the camera (optional)
		elevate(-CAMERA_ELEV);
		break;
#if 1
	case GLFW_KEY_W:
		pos += forward() * movement;
		break;
	case GLFW_KEY_S:
		pos += forward() * -movement;
		break;
	case GLFW_KEY_D:
		pos += right() * movement;
		break;
	case GLFW_KEY_A:
		pos += right() * -movement;
		break;
	case GLFW_KEY_K:
		pos += upward() * movement;
		break;
	case GLFW_KEY_J:
		pos += upward() * -movement;
		break;
	case GLFW_KEY_COMMA:
		rot = glm::rotate(quat(), movement, forward()) * rot;
		break;
	case GLFW_KEY_PERIOD:
		rot = glm::rotate(quat(), -movement, forward()) * rot;
		break;
#endif
	}
	updateCalc();
}

void Camera::mouseCB(int button, int action)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			input.pressed = true;
		} else {
			input.pressed = false;
			input.cursor = vec2(-1.f, -1.f);
		}
	}
}

void Camera::scrollCB(double yoffset)
{
	if (yoffset > 0)
		movement *= 2.f;
	else
		movement /= 2.f;
}

void Camera::cursorCB(double xpos, double ypos)
{
	if (!input.pressed)
		return;
	vec2 pos(xpos, ypos);
	if (input.cursor == pos)
		return;
	if (input.cursor.x >= 0 && input.cursor.y >= 0) {
		vec2 move(vec2(1.f, -1.f) * (input.cursor - pos));
		vec3 axis(cross(vec3(move, 0.f), vec3(0.f, 0.f, 1.f)));
		rot = glm::rotate(rot, 1.f / 512.f * glm::distance(input.cursor, pos), axis);
	}
	input.cursor = pos;
	updateCalc();
}

void Camera::updateCB(float /*time*/)
{
	pos = from_btVector3(bulletGetOrigin(rigidBody)) + vec3(0.f, CAMERA_HEIGHT, 0.f);
	modelMatrix = bulletGetMatrix(rigidBody);
	rigidBody->activate(true);
	rigidBody->applyCentralImpulse(to_btVector3(forward() * impulse));
	//pos += forward() * speed * time;
	updateCalc();
}

void Camera::setup()
{
	sphere = new Sphere(16);
	btTransform t = btTransform(btQuaternion(0, 0, 0, 1), to_btVector3(CAMERA_INIT_POS));
	rigidBody = sphere->createRigidBody(10.f, CAMERA_SIZE, t);
	rigidBody->setLinearVelocity(btVector3(0.f, 0.f, 0.f));
	bulletAddRigidBody(rigidBody, BULLET_CAMERA);
}

void Camera::reset()
{
	rigidBody->getWorldTransform().setIdentity();
	rigidBody->getWorldTransform().setOrigin(to_btVector3(CAMERA_INIT_POS));
	rigidBody->activate(true);
	rigidBody->setLinearVelocity(btVector3(0.f, 0.f, 0.f));
}

void Camera::render()
{
	glEnable(GL_CULL_FACE);
	glUseProgram(programs[PROGRAM_TEXTURE].id);
	uniformMap &uniforms = programs[PROGRAM_TEXTURE].uniforms;

	glUniform3fv(uniforms[UNIFORM_LIGHT_DIRECTION], 1, (GLfloat *)&environment.light.direction);
	glUniform3fv(uniforms[UNIFORM_LIGHT_INTENSITY], 1, (GLfloat *)&environment.light.intensity);
	glUniform3fv(uniforms[UNIFORM_VIEWER], 1, (GLfloat *)&position());

	matrix.model = modelMatrix;
	matrix.model = scale(matrix.model, vec3(CAMERA_SIZE));
	matrix.update();
	glUniformMatrix4fv(uniforms[UNIFORM_MVP], 1, GL_FALSE, (GLfloat *)&matrix.mvp);
	glUniformMatrix4fv(uniforms[UNIFORM_MODEL], 1, GL_FALSE, (GLfloat *)&matrix.model);
	glUniformMatrix3fv(uniforms[UNIFORM_NORMAL], 1, GL_FALSE, (GLfloat *)&matrix.normal);

	glUniform3fv(uniforms[UNIFORM_ENVIRONMENT], 1, (GLfloat *)&environment.ambient);
	glUniform3f(uniforms[UNIFORM_AMBIENT], 1.f, 1.f, 1.f);
	glUniform3f(uniforms[UNIFORM_DIFFUSE], 1.f, 1.f, 1.f);
	glUniform3f(uniforms[UNIFORM_EMISSION], 0.f, 0.f, 0.f);
	glUniform3f(uniforms[UNIFORM_SPECULAR], 1.f, 1.f, 1.f);
	glUniform1f(uniforms[UNIFORM_AMBIENT], 0.3f);
	glUniform1f(uniforms[UNIFORM_SHININESS], 30.f);

	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_CAMERA].texture);
	sphere->bind();
	sphere->render();
}

void Camera::backup()
{
	bak.pos = pos;
	bak.rot = rot;
}

void Camera::restore()
{
	pos = bak.pos;
	rot = bak.rot;
	speed = 0;
}

void Camera::rotate(float angle)
{
	rot = glm::rotate(rot, angle, vec3(0.f, 1.f, 0.f));
}

void Camera::accelerate(float v)
{
	impulse += impulse * v;
	if (v > 0) {
		if (impulse < v)
			impulse = v;
	} else {
		if (impulse < -v)
			impulse = 0.f;
	}
}

void Camera::elevate(float angle)
{
	rot = glm::rotate(rot, angle, vec3(1.f, 0.f, 0.f));
}

void Camera::print()
{
	clog << "Camera @(" << pos.x << ", " << pos.y << ", " << pos.z << "), (";
	clog << rot.w << ", " << rot.x << ", " << rot.y << ", " << rot.z << ")" << endl;
}

void Camera::updateCalc()
{
	matrix.view = view();
}

glm::mat4 Camera::view() const
{
	return lookAt(pos, pos + forward(), upward());
}
