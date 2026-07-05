#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

//FighterBase
class Fighter {
protected:
    string name;
    int hp;
    int attackValue;

public:
    // constructor
    Fighter(string fighterName, int health, int attack): name(fighterName), hp(health), attackValue(attack) {}
    // virtual destructor
    virtual ~Fighter() {}

    bool alive() const {
        // alive when hp is positive
        return hp > 0;
    }

    void takeDamage(int value) {
        // subtract damage and clamp hp
        hp -= value;
        if (hp < 0) hp = 0;
    }

    // getters
    int getHp() const { return hp; }

    string getName() const { return name; }

    // base damage rule
    virtual int damageOnRound(int round) const {
        return attackValue;
    }
};

//player
class Player: public Fighter {
public:
    // constructor
    Player(string fighterName, int health, int attack): Fighter(fighterName, health, attack) {}
    int damageOnRound(int round) const {
        // player critical rule
        return round % 3 == 0 ? attackValue * 2 : attackValue;
    }
};

//monster
class Monster: public Fighter {
public:
    // constructor
    Monster(string fighterName, int health, int attack): Fighter(fighterName, health, attack) {}
    int damageOnRound(int round) const {
        // monster bonus rule
        return round % 2 == 0 ? attackValue + 5 : attackValue;
    }
};

//fight
void fight(Fighter& player, Fighter& monster, int maxRound) {
    // round loop condition
    for (int round = 1; round <= maxRound && player.alive() && monster.alive(); ++round) {
        // player attacks first
        monster.takeDamage(player.damageOnRound(round));
        // monster retaliates only if alive
        if (monster.alive()) {
            // monster attack
            player.takeDamage(monster.damageOnRound(round));
        }
    }
}

int main() {
    int playerHp, playerAtk, monsterHp, monsterAtk, rounds;
    cin >> playerHp >> playerAtk >> monsterHp >> monsterAtk >> rounds;
    Player player("hero", playerHp, playerAtk);
    Monster monster("boss", monsterHp, monsterAtk);
    fight(player, monster, rounds);
    if (player.alive() && !monster.alive()) cout << "PLAYER\n";
    else if (!player.alive() && monster.alive()) cout << "MONSTER\n";
    else if (!player.alive() && !monster.alive()) cout << "DRAW\n";
    else cout << "UNFINISHED\n";
    cout << player.getHp() << ' ' << monster.getHp() << '\n';
    return 0;
}
