#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace std;

//your Shape
class Shape {
public:
	// your destructor
    virtual ~Shape() {}
	// your area
    virtual double area() const = 0;
};

//your Circle
class Circle: public Shape {
private:
    double radius;
public:
    Circle(double r): radius(r) {}
    double area() const {
        return 3.141592653589793 * radius * radius;
    }
};

//your Rectangle
class Rectangle: public Shape {
private:
    double width, height;
public:
    Rectangle(double w, double h): width(w), height(h) {}
    double area() const {
        return width * height;
    }
};
// your total_area
double totalArea(const vector<Shape*>& shapes) {
    double sum = 0;
    for (size_t i = 0; i < shapes.size(); ++i) sum += shapes[i]->area();
    return sum;
}

int main() {
    int n; cin >> n;
    vector<Shape*> shapes;
    for (int i = 0; i < n; ++i) {
        char type;
        cin >> type;
        if (type == 'C') {
            double r;
            cin >> r;
            shapes.push_back(new Circle(r));
        } else {
            double w, h;
            cin >> w >> h;
            shapes.push_back(new Rectangle(w, h));
        }
    }
    cout << fixed << setprecision(2) << totalArea(shapes) << '\n';
    for (size_t i = 0; i < shapes.size(); ++i) delete shapes[i];
    return 0;
}
