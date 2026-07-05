#include <iomanip>
#include <iostream>
#include <vector>
using namespace std;

class Account {
private:
    int id;
    double balance;

public:
    Account(int idValue = 0, double money = 0): id(idValue), balance(money) {}

    void deposit(double money) {
        if (money > 0) balance += money;
    }

    bool withdraw(double money) {
        if (money <= 0 || balance < money) return false;
        balance -= money;
        return true;
    }

    bool transferTo(Account& other, double money) {
        if (!withdraw(money)) return false;
        other.deposit(money);
        return true;
    }

    double getBalance() const {
        return balance;
    }
};

int main() {
    int n, m;
    cin >> n >> m;
    vector<Account> accounts(n + 1);
    for (int i = 1; i <= n; ++i) {
        double money;
        cin >> money;
        accounts[i] = Account(i, money);
    }
    for (int i = 0; i < m; ++i) {
        string op;
cin >> op;
if (op == "D") {
	int id; double money;
	cin >> id >> money;
	accounts[id].deposit(money);
} else if (op == "W") {
	int id; double money;
	cin >> id >> money;
	accounts[id].withdraw(money);
} else {
	int from, to; double money;
	cin >> from >> to >> money;
	accounts[from].transferTo(accounts[to], money);
}
    }
    cout << fixed << setprecision(2);
    for (int i = 1; i <= n; ++i) {
        if (i > 1) cout << ' ';
        cout << accounts[i].getBalance();
    }
    cout << '\n';
    return 0;
}
