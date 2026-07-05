#include <iomanip>
#include <iostream>
using namespace std;

class Date {
private:
    int year, month, day;

    bool isLeapYear(int y) const {
        return (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0);
    }

    int daysInMonth(int y, int m) const {
        static int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        if (m == 2 && isLeapYear(y)) return 29;
        return days[m];
    }

public:
    Date(int y, int m, int d): year(y), month(m), day(d) {}

    void addOneDay() {
        ++day;
        if (day > daysInMonth(year, month)) {
            day = 1;
            ++month;
            if (month > 12) {
                month = 1;
                ++year;
            }
        }
    }

    void addDays(int n) {
        for (int i = 0; i < n; ++i) addOneDay();
    }

    void print() const {
        cout << year << '-' << setw(2) << setfill('0') << month
             << '-' << setw(2) << setfill('0') << day << '\n';
    }
};

int main() {
    int y, m, d, n;
    cin >> y >> m >> d >> n;
    Date date(y, m, d);
    date.addDays(n);
    date.print();
    return 0;
}
