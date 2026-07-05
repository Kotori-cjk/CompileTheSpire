#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace std;

//Your readScores
vector<int> readScores(int n) {
    vector<int> scores(n);
    for (int i = 0; i < n; ++i) cin >> scores[i];
    return scores;
}
//Your averageScore
double averageScore(const vector<int>& scores) {
    int sum = 0;
    for (int x : scores) sum += x;
    return scores.empty() ? 0.0 : 1.0 * sum / scores.size();
}
//Your countPassed
int countPassed(const vector<int>& scores) {
    int cnt = 0;
    for (int x : scores) {
        if (x >= 60) ++cnt;
    }
    return cnt;
}
//Your highestScore
int highestScore(const vector<int>& scores) {
    return scores.empty() ? 0 : *max_element(scores.begin(), scores.end());
}

int main() {
    int n;
    cin >> n;
    vector<int> scores = readScores(n);
    cout << fixed << setprecision(2) << averageScore(scores) << '\n';
    cout << countPassed(scores) << '\n';
    cout << highestScore(scores) << '\n';
    return 0;
}
