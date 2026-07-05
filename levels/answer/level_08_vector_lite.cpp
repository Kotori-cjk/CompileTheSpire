#include <iostream>
using namespace std;

class VectorLite {
private:
    int* data;
    int sizeValue;
    int capacityValue;

    void grow() {
        int newCapacity = capacityValue == 0 ? 1 : capacityValue * 2;
        int* newData = new int[newCapacity];
        for (int i = 0; i < sizeValue; ++i) newData[i] = data[i];
        delete[] data;
        data = newData;
        capacityValue = newCapacity;
    }

public:
    VectorLite(): data(nullptr), sizeValue(0), capacityValue(0) {}

	// deepcopy
    VectorLite(const VectorLite& other): data(nullptr), sizeValue(other.sizeValue), capacityValue(other.capacityValue) {
        if (capacityValue > 0) {
            data = new int[capacityValue];
            for (int i = 0; i < sizeValue; ++i) data[i] = other.data[i];
        }
    }

    //your =
	VectorLite& operator=(const VectorLite& other) {
        if (this == &other) return *this;
        delete[] data;
        sizeValue = other.sizeValue;
        capacityValue = other.capacityValue;
        data = capacityValue > 0 ? new int[capacityValue] : nullptr;
        for (int i = 0; i < sizeValue; ++i) data[i] = other.data[i];
        return *this;
    }

    //your ~VectorLite()
	~VectorLite() {
        delete[] data;
    }

    //your pushback()
	void pushBack(int value) {
        if (sizeValue == capacityValue) grow();
        data[sizeValue++] = value;
    }

    void set(int index, int value) {
        if (0 <= index && index < sizeValue) data[index] = value;
    }

    void print() const {
        for (int i = 0; i < sizeValue; ++i) {
            if (i) cout << ' ';
            cout << data[i];
        }
        cout << '\n';
    }
};

int main() {
    int n; cin >> n;
    VectorLite a;
    for (int i = 0; i < n; ++i) { int x; cin >> x; a.pushBack(x); }
    VectorLite b = a;
    if (n > 0) a.set(0, 999);
    b.print();
    a.print();
    return 0;
}
