#include <iostream>
using namespace std;

class Complex {
private:
    int realPart, imagPart;

public:
    Complex(int r = 0, int i = 0): realPart(r), imagPart(i) {}

    Complex operator+(const Complex& other) const {
        return Complex(realPart + other.realPart, imagPart + other.imagPart);
    }

    Complex operator*(const Complex& other) const {
        return Complex(realPart * other.realPart - imagPart * other.imagPart,
                       realPart * other.imagPart + imagPart * other.realPart);
    }

    friend ostream& operator<<(ostream& out, const Complex& value) {
        out << value.realPart;
        if (value.imagPart >= 0) out << '+';
        out << value.imagPart << 'i';
        return out;
    }
};

int main() {
    int ar, ai, br, bi;
    char op;
    cin >> ar >> ai >> br >> bi >> op;
    Complex a(ar, ai), b(br, bi);
    if (op == '+') cout << (a + b) << '\n';
    else cout << (a * b) << '\n';
    return 0;
}
