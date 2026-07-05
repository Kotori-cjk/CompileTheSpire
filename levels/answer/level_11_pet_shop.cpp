#include <iostream>
#include <string>
#include <vector>
using namespace std;

//your Animal
class Animal {
protected:
    string name;
public:
	//constructor
    Animal(const string& animalName): name(animalName) {}
	//destructor
    virtual ~Animal() {}
	//speak
    virtual string speak() const = 0;
};
//your Dog
class Dog: public Animal {
public:
    Dog(const string& animalName): Animal(animalName) {}
    string speak() const {
        return name + ":woof";
    }
};
//your Cat
class Cat: public Animal {
public:
    Cat(const string& animalName): Animal(animalName) {}
    string speak() const {
        return name + ":meow";
    }
};
//SpeakAll
void speakAll(const vector<Animal*>& animals) {
    for (size_t i = 0; i < animals.size(); ++i) {
        cout << animals[i]->speak() << '\n';
    }
}

int main() {
    int n;
    cin >> n;
    vector<Animal*> animals;
    for (int i = 0; i < n; ++i) {
        char type;
        string name;
        cin >> type >> name;
        if (type == 'D') animals.push_back(new Dog(name));
        else animals.push_back(new Cat(name));
    }
    speakAll(animals);
    for (size_t i = 0; i < animals.size(); ++i) delete animals[i];
    return 0;
}
