#include <cstdlib>
#include <iostream>
using namespace std;

int gcdInt(int a, int b) {
    a = abs(a);
    b = abs(b);
	while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
	return a == 0 ? 1 : a;
}

class Fraction {
private:
    int numerator, denominator;

    void normalize() {
        if (denominator < 0) {
            numerator = -numerator;
            denominator = -denominator;
        }
        int g = gcdInt(numerator, denominator);
        numerator /= g;
        denominator /= g;
    }

public:
    Fraction(int n = 0, int d = 1): numerator(n), denominator(d) {
        normalize();
    }

    Fraction operator+(const Fraction& other) const {
        return Fraction(numerator * other.denominator + other.numerator * denominator, denominator * other.denominator);
    }

    Fraction operator*(const Fraction& other) const {
        return Fraction(numerator * other.numerator, denominator * other.denominator);
    }

    void print() const {
        cout << numerator << '/' << denominator << '\n';
    }
};

int main() {
    int a, b, c, d;
    cin >> a >> b >> c >> d;
    Fraction x(a, b), y(c, d);
    (x + y).print();
    (x * y).print();
    return 0;
}