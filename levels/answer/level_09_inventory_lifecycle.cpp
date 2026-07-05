#include <iostream>
#include <string>
using namespace std;

// your class Item
class Item {
private:
    string name;
    int power;

public:
	// your constructor
    Item(string itemName = "empty", int itemPower = 0): name(itemName), power(itemPower) {}
	
	// your copy constructor
    Item(const Item& other): name(other.name), power(other.power) {}

    Item& operator=(const Item& other) {
        if (this != &other) {
            name = other.name;
            power = other.power;
        }
        return *this;
    }

    void strengthen(int delta) {
        power += delta;
    }

    int getPower() const { return power; }
    string getName() const { return name; }
};

// your class Inventory
class Inventory {
private:
    Item* items;
    int sizeValue;

public:

	// constructor
    Inventory(int n = 0): items(n > 0 ? new Item[n] : nullptr), sizeValue(n) {}

	// copy constructor
    //what it is?
// The copy constructor of `Inventory`
	Inventory(const Inventory& other): items(other.sizeValue > 0 ? new Item[other.sizeValue] : nullptr), sizeValue(other.sizeValue) {
        for (int i = 0; i < sizeValue; ++i) items[i] = other.items[i];
    }

    Inventory& operator=(const Inventory& other) {
        if (this == &other) return *this;
        delete[] items;
        sizeValue = other.sizeValue;
        items = sizeValue > 0 ? new Item[sizeValue] : nullptr;
        for (int i = 0; i < sizeValue; ++i) items[i] = other.items[i];
        return *this;
    }

    ~Inventory() {
        delete[] items;
    }

    void setItem(int index, const Item& item) {
        if (0 <= index && index < sizeValue) items[index] = item;
    }

    void strengthenItem(int index, int delta) {
        if (0 <= index && index < sizeValue) items[index].strengthen(delta);
    }

    int totalPower() const {
        int sum = 0;
        for (int i = 0; i < sizeValue; ++i) sum += items[i].getPower();
        return sum;
    }
};

int main() {
    int n; cin >> n;
    Inventory original(n);
    for (int i = 0; i < n; ++i) { string name; int power; cin >> name >> power; original.setItem(i, Item(name, power)); }
    Inventory copied = original;
    if (n > 0) original.strengthenItem(0, 100);
    cout << copied.totalPower() << '\n';
    cout << original.totalPower() << '\n';
    return 0;
}
