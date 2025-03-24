#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>

#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace glm;

// -------------------------------------------------
// Global Variables
// -------------------------------------------------
int Width = 512;
int Height = 512;
std::vector<float> OutputImage;
// -------------------------------------------------

//점 구조체
struct Point {
	double x, y, z;
};

//두 점사이의 거리를 계산
double dist(Point a, Point b) {
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

//선
class Ray {
public:
	//pos를 지나는 3차원 선
	//pos.x + t*dir.x , pos.y + t*dir.y, pos.z + t*dir.z 를 지난다.
	Point pos;
	Point dir;

	Ray(Point position, Point direction) {
		pos = position;
		dir = direction;
	}
};

//카메라
class Camera {
public:
	Point pos;
	//dir이 0,1,2일 때, 각각 u,v,w방향을 나타낸다.
	int dir = 0;
	//각각 u방향, v방향, w방향을 나타낸다.
	Point dirPoint[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
	//u방향, v방향, w방향일 때 스크린이 어떤 방향으로 생기는 지를 표현한다.
	Point dirVertical[3][2] = { {{0,0,1}, {0,1,0}}, {{1,0,0}, {0,0,1}}, {{1,0,0}, {0,1,0}}};
	//-방향인지, +방향인지를 각각 -1, 1로 나타낸다.
	int sign = 1;
	double l, r, b, t, d;
	//dir방향으로 d만큼 떨어진 곳에, [l,r] X [b,t] 스크린이 존재함을 의미

	//direction은 부호와 문자를 이용해서 나타낸다. (ex : "-w")
	Camera(Point position, std::string direction, double L,double R,double B,double T,double D) {
		pos = position;
		if (direction[0] == '-') sign = -1;
		else sign = 1;
		if (direction.back() == 'u') dir = 0;
		else if (direction.back() == 'v') dir = 1;
		else dir = 2;
		l = L;
		r = R;
		b = B;
		t = T;
		d = D;
	}
	
	//해상도가 nx, ny일 때, ix, iy칸에 해당하는 선(Ray)를 반환한다.
	Ray getRay(int ix, int iy, int nx, int ny) {
		ix++, iy++;
		//해상도가 nx, ny일 때, 화면의 ix, iy에 해당하는 부분을 지나는 선을 리턴
		Point mid = { pos.x + sign * d * dirPoint[dir].x, pos.y + sign * d * dirPoint[dir].y , pos.z + sign * d * dirPoint[dir].z }; //스크린의 중심

		Point topleft = { mid.x - dirVertical[dir][0].x * (r - l) / 2 + dirVertical[dir][1].x * (t - b) / 2,
			mid.y - dirVertical[dir][0].y * (r - l) / 2 + dirVertical[dir][1].y * (t - b) / 2,
			mid.z - dirVertical[dir][0].z * (r - l) / 2 + dirVertical[dir][1].z * (t - b) / 2
		}; //스크린의 가장 위, 왼쪽
		Point res = { topleft.x + (2 * ix - 1) * (r - l) / (2 * nx) * dirVertical[dir][0].x - (2 * iy - 1) * (t - b) / (2 * ny) * dirVertical[dir][1].x,
			topleft.y + (2 * ix - 1) * (r - l) / (2 * nx) * dirVertical[dir][0].y - (2 * iy - 1) * (t - b) / (2 * ny) * dirVertical[dir][1].y,
			topleft.z + (2 * ix - 1) * (r - l) / (2 * nx) * dirVertical[dir][0].z - (2 * iy - 1) * (t - b) / (2 * ny) * dirVertical[dir][1].z
		}; //스크린의 좌표
		
		double distToRes = dist(pos, res);
		//방향 값이 너무 커지지 않도록 거리로 나누기
		Point diff = { (res.x - pos.x) / distToRes, (res.y - pos.y) / distToRes, (res.z - pos.z) / distToRes};
		Ray* ray = new Ray(pos, diff);
		return *ray;
	}
};


class Surface {
public:
	//override를 위한 함수
	virtual double intersect(Ray ray, double tMin, double tMax){
		return tMax + 1;
	}
};

//평면
class Plane : public Surface {
public:
	//평면의 y좌표
	double y = 0;

	Plane(double ycor) {
		y = ycor;
	}

	//평면과 선이 만나는 지점을 반환한다.
	double intersect(Ray ray, double tMin, double tMax) override{
		//평면과 선이 만나는 지점 리턴하기, 여러개라면 ray.pos에 가까운 것을 리턴

		if (ray.dir.y == 0) {
			if (y == ray.pos.y) {
				//항상 가능하다.
				return tMin;
			}
			else {
				//항상 불가능하다.
				return tMax + 1;
			}
		}
		double rest = (y - ray.pos.y) / (ray.dir.y);
		if (tMin <= rest && rest <= tMax) return rest;
		else return tMax + 1;
	}
};

//구
class Sphere : public Surface {
public:
	//구의 중심 좌표와 반지름
	Point pos;
	double r = 0;

	Sphere(Point position, double radius) {
		pos = position;
		r = radius;
	}

	//구와 ray가 만나는 지점을 반환한다.
	double intersect(Ray ray, double tMin, double tMax) override {
		//원과 선이 만나는 지점 리턴하기, 여러개라면 ray.pos에 가까운 것을 리턴

		Point diff = { ray.pos.x - pos.x, ray.pos.y - pos.y, ray.pos.z - pos.z };
		double a = ray.dir.x * ray.dir.x + ray.dir.y * ray.dir.y + ray.dir.z * ray.dir.z;
		double b = 2 * diff.x * ray.dir.x + 2 * diff.y * ray.dir.y + 2 * diff.z * ray.dir.z;
		double c = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z - r * r;
		//a*t^2 + b*t + c = 0 를 만족하는 t가 구하고 싶은 값이다.
		
		if (b * b - 4 * a * c < 0) { //ray와 원이 만나지 않을 때
			return tMax + 1;
		}

		double t1 = (- b - std::sqrt(b * b - 4 * a * c)) / (2 * a);
		double t2 = (- b + std::sqrt(b * b - 4 * a * c)) / (2 * a);
		int sign1 = 0, sign2 = 0;
		//가능한 t를 리턴, 없다면 tMax+1(불가능한 값의 표현), 여러개라면 최솟값을 반환한다.
		if (tMin <= t1 && t1 <= tMax) sign1 = 1;
		if (tMin <= t2 && t2 <= tMax) sign2 = 1;
		if (sign1 == 0 && sign2 == 1) return t2;
		else if (sign1 == 1 && sign2 == 0) return t1;
		else if (sign1 == 1 && sign2 == 1) return min(t1, t2);
		else return tMax + 1;
	}
};


//물체들의 집합
class Scene {
public:
	std::vector<Surface*> v;

	void add(Surface* s) {
		v.push_back(s);
	}

	Point trace(Ray ray, double tMin, double tMax, int* isExist) {
		//ray와 Surface들이 만나는 지점을 리턴하기, tMin과 tMax사이의 범위만 탐색한다.
		double rest = tMax + 1;
		for (Surface* i : v) {
			//i와 만나는 점을 리턴하기
			rest = min(rest, i->intersect(ray, tMin, tMax));
		}
		if (tMin <= rest && rest <= tMax) return { ray.pos.x + ray.dir.x * rest, ray.pos.y + ray.dir.y * rest ,ray.pos.z + ray.dir.z * rest };
		else {
			*isExist = 0;
			return { 0,0,0 };
		}
	}
};

void render()
{
	//Create our image. We don't want to do this in 
	//the main loop since this may be too slow and we 
	//want a responsive display of our beautiful image.
	//Instead we draw to another buffer and copy this to the 
	//framebuffer using glDrawPixels(...) every refresh
	OutputImage.clear();

	//물체 생성
	Surface* s1 = new Sphere({ -4.0,0.0,-7.0 }, 1.0);
	Surface* s2 = new Sphere({ 0.0,0.0,-7.0 }, 2.0);
	Surface* s3 = new Sphere({ 4.0,0.0,-7.0 }, 1.0);
	Surface* p1 = new Plane(-2.0);

	//Scene에 넣기
	Scene scene;
	scene.add(s1);
	scene.add(s2);
	scene.add(s3);
	scene.add(p1);

	//카메라 생성
	Camera* camera = new Camera({ 0,0,0 }, "-w", -0.1, 0.1, -0.1, 0.1, 0.1);

	for (int j = 0; j < Height; ++j) 
	{
		for (int i = 0; i < Width; ++i) 
		{
			//i, j에 해당하는 x,y
			int x = i;
			int y = Height - j - 1; //배열의 j와 좌표 축의 y는 반대로 계산하기 
			//x,y에 해당하는 선
			Ray ray = camera->getRay(x, y, Width, Height);
			//ray와 물체가 만난다면 1, 만나지 않는다면 0으로 나타낸다.
			int isExist = 1;
			//만난다면 isExist = 1이고, res에는 만나는 점이 담기게 된다.
			//만나지 않는다면 isExist = 0이 담긴다.
			Point res = scene.trace(ray, 0.01, 1000, &isExist);
				
			vec3 color = glm::vec3(0.0f, 0.0f, 0.0f);
			if (isExist == 0) {
				//존재하지 않을 때.
			}
			else {
				//존재할 때
				color = glm::vec3(1.0f, 1.0f, 1.0f);
			}

			// set the color
			OutputImage.push_back(color.x); // R
			OutputImage.push_back(color.y); // G
			OutputImage.push_back(color.z); // B
		}
	}
}


void resize_callback(GLFWwindow*, int nw, int nh) 
{
	//This is called in response to the window resizing.
	//The new width and height are passed in so we make 
	//any necessary changes:
	Width = nw;
	Height = nh;
	//Tell the viewport to use all of our screen estate
	glViewport(0, 0, nw, nh);

	//This is not necessary, we're just working in 2d so
	//why not let our spaces reflect it?
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0.0, static_cast<double>(Width)
		, 0.0, static_cast<double>(Height)
		, 1.0, -1.0);

	//Reserve memory for our render so that we don't do 
	//excessive allocations and render the image
	OutputImage.reserve(Width * Height * 3);
	render();
}


int main(int argc, char* argv[])
{
	// -------------------------------------------------
	// Initialize Window
	// -------------------------------------------------

	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
		return -1;

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(Width, Height, "OpenGL Viewer", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	//We have an opengl context now. Everything from here on out 
	//is just managing our window or opengl directly.

	//Tell the opengl state machine we don't want it to make 
	//any assumptions about how pixels are aligned in memory 
	//during transfers between host and device (like glDrawPixels(...) )
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	//We call our resize function once to set everything up initially
	//after registering it as a callback with glfw
	glfwSetFramebufferSizeCallback(window, resize_callback);
	resize_callback(NULL, Width, Height);

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		//Clear the screen
		glClear(GL_COLOR_BUFFER_BIT);

		// -------------------------------------------------------------
		//Rendering begins!
		glDrawPixels(Width, Height, GL_RGB, GL_FLOAT, &OutputImage[0]);
		//and ends.
		// -------------------------------------------------------------

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();

		//Close when the user hits 'q' or escape
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS
			|| glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
