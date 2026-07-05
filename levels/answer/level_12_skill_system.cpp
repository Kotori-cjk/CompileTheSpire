#include <iostream>
#include <string>
#include <vector>
using namespace std;

//class Charactor
class Character {
private:
    string name;
    int hp;

public:
	//your constructor
    Character(string characterName = "", int health = 0): name(characterName), hp(health) {}

    void damage(int value) {
        hp -= value;
        if (hp < 0) hp = 0;
    }

    void heal(int value) {
        hp += value;
    }
	
	//getName
    int getHp() const { return hp; }

    string getName() const { return name; }
};

//class Skill
class Skill {
public:
	//your destructor
    virtual ~Skill() {}

	//use skill
    virtual void cast(Character& self, Character& target) const = 0;
};

//class Fire
class FireSkill: public Skill {
public:
    void cast(Character& self, Character& target) const {
	//fire!
        target.damage(30);
    }
};

//class Heal
class HealSkill: public Skill {
public:
    void cast(Character& self, Character& target) const {
	//heal!
        self.heal(20);
    }
};

int main() {
    Character hero("hero", 100), enemy("enemy", 100);
    int n; cin >> n;
    vector<Skill*> skills;
    for (int i = 0; i < n; ++i) {
		string type;
		cin >> type;
		if (type == "fire") skills.push_back(new FireSkill());
		else skills.push_back(new HealSkill()); }
    for (size_t i = 0; i < skills.size(); ++i) skills[i]->cast(hero, enemy);
    cout << hero.getHp() << ' ' << enemy.getHp() << '\n';
    for (size_t i = 0; i < skills.size(); ++i) delete skills[i];
    return 0;
}
