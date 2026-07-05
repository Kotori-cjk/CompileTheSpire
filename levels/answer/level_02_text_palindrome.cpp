#include <cctype>
#include <iostream>
#include <string>
using namespace std;

//your normalize
string normalize(const string& text) {
    string result;
    for (char ch : text) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (isalnum(c)) result += static_cast<char>(tolower(c));
    }
    return result;
}

//your isPalindrome
bool isPalindrome(const string& s) {
    int left = 0, right = static_cast<int>(s.size()) - 1;
    while (left < right) {
        if (s[left] != s[right]) return false;
        ++left;
        --right;
    }
    return true;
}

int main() {
    string line;
    getline(cin, line);
    string cleaned = normalize(line);
    cout << cleaned << '\n';
    cout << (isPalindrome(cleaned) ? "YES" : "NO") << '\n';
    return 0;
}

